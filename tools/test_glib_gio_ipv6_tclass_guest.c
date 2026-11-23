/* GIO IPv6 traffic-class codec and the SDK's IPv6 socket boundary. */
#include <gio/gio.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHECK(phase, condition) do { if (!(condition)) { \
    printf("gio-ipv6-tclass: failure %s line=%d errno=%d\n", phase, __LINE__, errno); \
    return 1; \
} } while (0)

int main(void) {
    for (unsigned dscp = 0; dscp < 64; ++dscp) {
        for (unsigned ecn = 0; ecn < 4; ++ecn) {
            GSocketControlMessage *message = g_ipv6_tclass_message_new((guint8)dscp, (GEcnCodePoint)ecn);
            CHECK("new", message && G_IS_IPV6_TCLASS_MESSAGE(message));
            GSocketControlMessageClass *klass = G_SOCKET_CONTROL_MESSAGE_GET_CLASS(message);
            CHECK("class", klass && klass->deserialize);
            CHECK("metadata", g_socket_control_message_get_size(message) == sizeof(int) &&
                  g_socket_control_message_get_level(message) == IPPROTO_IPV6 &&
                  g_socket_control_message_get_msg_type(message) == IPV6_TCLASS);
            CHECK("getters", g_ipv6_tclass_message_get_dscp(G_IPV6_TCLASS_MESSAGE(message)) == dscp &&
                  (unsigned)g_ipv6_tclass_message_get_ecn(G_IPV6_TCLASS_MESSAGE(message)) == ecn);
            struct { int before; int value; int after; } payload = { 0x12345678, -1, 0x23456789 };
            g_socket_control_message_serialize(message, &payload.value);
            CHECK("serialize", payload.before == 0x12345678 && payload.after == 0x23456789 &&
                  payload.value == (int)((dscp << 2) | ecn));
            GSocketControlMessage *decoded = klass->deserialize(IPPROTO_IPV6, IPV6_TCLASS,
                                                                sizeof(payload.value), &payload.value);
            CHECK("deserialize", decoded && G_IS_IPV6_TCLASS_MESSAGE(decoded) &&
                  g_ipv6_tclass_message_get_dscp(G_IPV6_TCLASS_MESSAGE(decoded)) == dscp &&
                  (unsigned)g_ipv6_tclass_message_get_ecn(G_IPV6_TCLASS_MESSAGE(decoded)) == ecn);
            CHECK("wrong-level", klass->deserialize(IPPROTO_IP, IPV6_TCLASS, sizeof(payload.value), &payload.value) == NULL);
            CHECK("wrong-type", klass->deserialize(IPPROTO_IPV6, IPV6_TCLASS + 1, sizeof(payload.value), &payload.value) == NULL);
            CHECK("zero-size", klass->deserialize(IPPROTO_IPV6, IPV6_TCLASS, 0, &payload.value) == NULL);
            CHECK("short-size", klass->deserialize(IPPROTO_IPV6, IPV6_TCLASS, sizeof(payload.value) - 1, &payload.value) == NULL);
            CHECK("large-size", klass->deserialize(IPPROTO_IPV6, IPV6_TCLASS, sizeof(payload), &payload) == NULL);
            CHECK("guards", payload.before == 0x12345678 && payload.after == 0x23456789 &&
                  payload.value == (int)((dscp << 2) | ecn));
            g_object_unref(decoded);
            g_object_unref(message);
        }
    }
    errno = 0;
    int fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (fd >= 0) close(fd);
    CHECK("ipv6-udp-refused", fd == -1 && errno == EAFNOSUPPORT);
    errno = 0;
    fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (fd >= 0) close(fd);
    CHECK("ipv6-tcp-refused", fd == -1 && errno == EAFNOSUPPORT);
    printf("gio-ipv6-tclass: ok\n");
    return 0;
}
