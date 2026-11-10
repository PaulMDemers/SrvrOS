/* Independent dynamically encrypted AP helper for host-only stack fixtures. */
#include "mt76-stack5-peer.h"
#include "wpa2-handshake-synthetic.h"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <string.h>

#define STACK5_ETH_MAX 1514u
#define STACK5_PLAIN_MAX 1508u
#define STACK5_USB_MAX 1580u
#define STACK5_PN_MAX UINT64_C(0xffffffffffff)

static bool stack5_overlap(const void *a, size_t an, const void *b, size_t bn) {
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return an && bn && (x <= y ? y - x < an : x - y < bn);
}

/* Test-only AP generation state. This is not production key ownership or PN
 * state: the modeled AP explicitly selects a public synthetic association.
 * Keep the first generation byte-identical to the retained legacy fixtures. */
static unsigned stack5_generation;
static uint8_t stack5_snonce[32], stack5_ptk[48], stack5_m3[sizeof hs_m3];

static bool stack5_prf(const uint8_t snonce[32], uint8_t ptk[48]) {
    static const uint8_t label[] = "Pairwise key expansion";
    uint8_t input[sizeof label + 12 + 64 + 1], digest[EVP_MAX_MD_SIZE];
    const uint8_t *a = hs_aa, *b = hs_spa, *c = hs_an, *d = snonce;
    if (memcmp(a, b, 6) > 0) { a = hs_spa; b = hs_aa; }
    if (memcmp(c, d, 32) > 0) { c = snonce; d = hs_an; }
    size_t n = 0;
    memcpy(input + n, label, sizeof label); n += sizeof label;
    memcpy(input + n, a, 6); n += 6; memcpy(input + n, b, 6); n += 6;
    memcpy(input + n, c, 32); n += 32; memcpy(input + n, d, 32); n += 32;
    bool ok = true;
    for (unsigned i = 0; i < 3 && ok; i++) {
        unsigned bytes = 0; input[n] = (uint8_t)i;
        ok = HMAC(EVP_sha1(), hs_pmk, sizeof hs_pmk, input, n + 1,
                  digest, &bytes) != NULL && bytes == 20;
        if (ok) memcpy(ptk + 20 * i, digest, i == 2 ? 8 : 20);
    }
    OPENSSL_cleanse(input, sizeof input);
    OPENSSL_cleanse(digest, sizeof digest);
    return ok;
}

static bool stack5_wrap(bool encrypt, const uint8_t kek[16],
                        const uint8_t *in, size_t n, uint8_t *out, size_t wanted) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;
    EVP_CIPHER_CTX_set_flags(ctx, EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);
    int bytes = 0, tail = 0;
    bool ok;
    if (encrypt) {
        ok = EVP_EncryptInit_ex(ctx, EVP_aes_128_wrap(), NULL, kek, NULL) == 1 &&
             EVP_EncryptUpdate(ctx, out, &bytes, in, (int)n) == 1 &&
             EVP_EncryptFinal_ex(ctx, out + bytes, &tail) == 1;
    } else {
        ok = EVP_DecryptInit_ex(ctx, EVP_aes_128_wrap(), NULL, kek, NULL) == 1 &&
             EVP_DecryptUpdate(ctx, out, &bytes, in, (int)n) == 1 &&
             EVP_DecryptFinal_ex(ctx, out + bytes, &tail) == 1;
    }
    EVP_CIPHER_CTX_free(ctx);
    return ok && bytes >= 0 && tail >= 0 && (size_t)(bytes + tail) == wanted;
}

bool stack5_peer_generation(unsigned generation) {
    if (generation != 1 && generation != 2) return false;
    uint8_t nonce[32], ptk[48], message[sizeof hs_m3], plain[sizeof hs_m3];
    uint8_t digest[EVP_MAX_MD_SIZE]; unsigned digest_size = 0;
    memcpy(nonce, hs_sn, sizeof nonce);
    if (generation == 2) for (unsigned i = 0; i < sizeof nonce; i++) nonce[i] ^= 0x80;
    bool ok = stack5_prf(nonce, ptk);
    memcpy(message, hs_m3, sizeof message);
    size_t encrypted = (size_t)message[97] * 256 + message[98];
    ok = ok && encrypted >= 16 && encrypted + 99 == sizeof message;
    if (generation == 1) ok = ok && !memcmp(ptk, hs_ptk, sizeof ptk);
    else if (ok) {
        ok = memcmp(ptk, hs_ptk, sizeof ptk) != 0 &&
             stack5_wrap(false, hs_ptk + 16, hs_m3 + 99, encrypted,
                         plain, encrypted - 8) &&
             stack5_wrap(true, ptk + 16, plain, encrypted - 8,
                         message + 99, encrypted);
        memset(message + 81, 0, 16);
        ok = ok && HMAC(EVP_sha1(), ptk, 16, message, sizeof message,
                        digest, &digest_size) != NULL && digest_size == 20;
        if (ok) memcpy(message + 81, digest, 16);
    }
    if (ok) {
        memcpy(stack5_snonce, nonce, sizeof nonce);
        memcpy(stack5_ptk, ptk, sizeof ptk);
        memcpy(stack5_m3, message, sizeof message);
        stack5_generation = generation;
    }
    OPENSSL_cleanse(nonce, sizeof nonce); OPENSSL_cleanse(ptk, sizeof ptk);
    OPENSSL_cleanse(message, sizeof message); OPENSSL_cleanse(plain, sizeof plain);
    OPENSSL_cleanse(digest, sizeof digest);
    return ok;
}

static bool stack5_ready(void) {
    return stack5_generation || stack5_peer_generation(1);
}

bool stack5_peer_nonce(uint8_t out[32]) {
    if (!out || !stack5_ready()) return false;
    memcpy(out, stack5_snonce, sizeof stack5_snonce);
    return true;
}

bool stack5_peer_message3(uint8_t *out, size_t cap, size_t *size) {
    if (size) *size = 0;
    if (!out || !size || cap < sizeof stack5_m3 ||
        stack5_overlap(out, cap, size, sizeof *size) || !stack5_ready()) return false;
    memcpy(out, stack5_m3, sizeof stack5_m3);
    *size = sizeof stack5_m3;
    return true;
}

static unsigned stack5_le16(const uint8_t *p) {
    return (unsigned)p[0] | (unsigned)p[1] << 8;
}

static uint32_t stack5_le32(const uint8_t *p) {
    return stack5_le16(p) | (uint32_t)stack5_le16(p + 2) << 16;
}

static bool stack5_zero(const uint8_t *p, size_t n) {
    unsigned value = 0;
    for (size_t i = 0; i < n; i++) value |= p[i];
    return value == 0;
}

static bool stack5_protocol(const uint8_t *p) {
    return p[0] == 8 && (p[1] == 0 || p[1] == 6);
}

static void stack5_aad_nonce(const uint8_t h[24], uint64_t pn,
                             uint8_t aad[22], uint8_t nonce[13]) {
    aad[0] = h[0] & 0x8f;
    aad[1] = h[1] & 0xc7;
    memcpy(aad + 2, h + 4, 18);
    aad[20] = h[22] & 15;
    aad[21] = 0;
    nonce[0] = 0;
    memcpy(nonce + 1, h + 10, 6);
    for (unsigned i = 0; i < 6; i++) nonce[7 + i] = (uint8_t)(pn >> (40 - 8 * i));
}

static bool stack5_decrypt(const uint8_t *h, const uint8_t *cipher,
                           size_t n, uint64_t pn, uint8_t *plain) {
    uint8_t aad[22], nonce[13];
    stack5_aad_nonce(h, pn, aad, nonce);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int size = 0;
    bool ok = ctx &&
        EVP_DecryptInit_ex(ctx, EVP_aes_128_ccm(), NULL, NULL, NULL) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_IVLEN, sizeof nonce, NULL) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_TAG, 8, (void *)(cipher + n)) == 1 &&
        EVP_DecryptInit_ex(ctx, NULL, NULL, stack5_ptk + 32, nonce) == 1 &&
        EVP_DecryptUpdate(ctx, NULL, &size, NULL, (int)n) == 1 &&
        EVP_DecryptUpdate(ctx, NULL, &size, aad, sizeof aad) == 1 &&
        EVP_DecryptUpdate(ctx, plain, &size, cipher, (int)n) == 1 && size == (int)n;
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

bool stack5_peer_open(const uint8_t *usb, size_t usb_n,
                      uint8_t *eth, size_t cap, size_t *eth_n, uint64_t *pn) {
    if (eth_n) *eth_n = 0;
    if (pn) *pn = 0;
    if (!usb || !eth || !eth_n || !pn || usb_n < 76 || usb_n > STACK5_USB_MAX ||
        stack5_overlap(usb, usb_n, eth, cap) || !stack5_ready()) return false;
    size_t dma = stack5_le16(usb), n = stack5_le16(usb + 10);
    if (dma < 68 || dma + 8 != usb_n || dma != ((20 + n + 3) & ~(size_t)3) ||
        stack5_le32(usb) != (dma | (1u << 19) | (1u << 24) | (2u << 25)) ||
        !stack5_zero(usb + 4, 2) || stack5_le16(usb + 6) != 0x2000 ||
        usb[8] != 1 || usb[9] != 255 || !stack5_zero(usb + 12, 11) || usb[23] != 128 ||
        n < 48 || n > STACK5_PLAIN_MAX + 40 || n + 24 > usb_n ||
        !stack5_zero(usb + 24 + n, usb_n - 24 - n)) return false;
    const uint8_t *h = usb + 24, *iv = h + 24;
    if (stack5_le16(h) != 0x4108 || stack5_le16(h + 2) != 60 ||
        memcmp(h + 4, hs_aa, 6) || memcmp(h + 10, hs_spa, 6) ||
        (stack5_le16(h + 22) & 15) || !(stack5_le16(h + 22) >> 4) ||
        iv[2] != 0 || iv[3] != 0x20) return false;
    uint64_t packet = iv[0] | (uint64_t)iv[1] << 8;
    for (unsigned i = 0; i < 4; i++) packet |= (uint64_t)iv[4 + i] << (16 + 8 * i);
    size_t length = n - 40;
    if (!packet || length + 6 > cap) return false;
    uint8_t plain[STACK5_PLAIN_MAX];
    static const uint8_t snap[6] = {0xaa, 0xaa, 3, 0, 0, 0};
    bool ok = stack5_decrypt(h, h + 32, length, packet, plain) &&
        !memcmp(plain, snap, sizeof snap) && stack5_protocol(plain + 6);
    if (ok) {
        memcpy(eth, h + 16, 6);
        memcpy(eth + 6, h + 10, 6);
        memcpy(eth + 12, plain + 6, length - 6);
        *eth_n = length + 6;
        *pn = packet;
    }
    OPENSSL_cleanse(plain, sizeof plain);
    return ok;
}

bool stack5_peer_seal(const uint8_t *eth, size_t n, uint64_t pn,
                      uint8_t *mpdu, size_t cap, size_t *mpdu_n) {
    if (mpdu_n) *mpdu_n = 0;
    if (!eth || !mpdu || !mpdu_n || n < 14 || n > STACK5_ETH_MAX ||
        !pn || pn > STACK5_PN_MAX || cap < n + 34 ||
        stack5_overlap(eth, n, mpdu, cap) || !stack5_protocol(eth + 12) ||
        (!(eth[0] & 1) && memcmp(eth, hs_spa, 6)) ||
        (eth[6] & 1) || stack5_zero(eth + 6, 6) || !stack5_ready()) return false;
    uint8_t packet[1554] = {0}, plain[STACK5_PLAIN_MAX], aad[22], nonce[13];
    packet[0] = 8;
    packet[1] = 0x42;
    memcpy(packet + 4, eth, 6);
    memcpy(packet + 10, hs_aa, 6);
    memcpy(packet + 16, eth + 6, 6);
    unsigned sequence = (unsigned)(pn & 4095) << 4;
    packet[22] = (uint8_t)sequence;
    packet[23] = (uint8_t)(sequence >> 8);
    uint8_t *iv = packet + 24;
    iv[0] = (uint8_t)pn;
    iv[1] = (uint8_t)(pn >> 8);
    iv[3] = (eth[0] & 1) ? 0x60 : 0x20;
    for (unsigned i = 0; i < 4; i++) iv[4 + i] = (uint8_t)(pn >> (16 + 8 * i));
    static const uint8_t snap[6] = {0xaa, 0xaa, 3, 0, 0, 0};
    memcpy(plain, snap, sizeof snap);
    memcpy(plain + 6, eth + 12, n - 12);
    size_t length = n - 6;
    stack5_aad_nonce(packet, pn, aad, nonce);
    const uint8_t *key = eth[0] & 1 ? hs_gtk : stack5_ptk + 32;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int size = 0, tail = 0;
    bool ok = ctx &&
        EVP_EncryptInit_ex(ctx, EVP_aes_128_ccm(), NULL, NULL, NULL) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_IVLEN, sizeof nonce, NULL) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_SET_TAG, 8, NULL) == 1 &&
        EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) == 1 &&
        EVP_EncryptUpdate(ctx, NULL, &size, NULL, (int)length) == 1 &&
        EVP_EncryptUpdate(ctx, NULL, &size, aad, sizeof aad) == 1 &&
        EVP_EncryptUpdate(ctx, packet + 32, &size, plain, (int)length) == 1 &&
        size == (int)length && EVP_EncryptFinal_ex(ctx, packet + 32 + length, &tail) == 1 &&
        tail == 0 && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_CCM_GET_TAG, 8, packet + 32 + length) == 1;
    if (ok) {
        memcpy(mpdu, packet, n + 34);
        *mpdu_n = n + 34;
    }
    EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(packet, sizeof packet);
    OPENSSL_cleanse(plain, sizeof plain);
    return ok;
}
