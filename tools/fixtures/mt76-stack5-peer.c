/* Independent dynamically encrypted AP helper for host-only stack fixtures. */
#include "mt76-stack5-peer.h"
#include "wpa2-handshake-synthetic.h"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <string.h>

#define STACK5_ETH_MAX 1514u
#define STACK5_PLAIN_MAX 1508u
#define STACK5_USB_MAX 1580u
#define STACK5_PN_MAX UINT64_C(0xffffffffffff)

static bool stack5_overlap(const void *a, size_t an, const void *b, size_t bn) {
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return an && bn && (x <= y ? y - x < an : x - y < bn);
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
        EVP_DecryptInit_ex(ctx, NULL, NULL, hs_ptk + 32, nonce) == 1 &&
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
        stack5_overlap(usb, usb_n, eth, cap)) return false;
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
        (eth[6] & 1) || stack5_zero(eth + 6, 6)) return false;
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
    const uint8_t *key = eth[0] & 1 ? hs_gtk : hs_ptk + 32;
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
