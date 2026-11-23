/* GIO IPv4 ToS control-message behavior and the SDK's socket refusal. */
#include <gio/gio.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHECK(phase, condition) do { if (!(condition)) { \
    printf("gio-ip-tos: failure %s line=%d errno=%d\n", phase, __LINE__, errno); \
    return 1; \
} } while (0)

int main(void) {
    for (unsigned dscp = 0; dscp < 64; ++dscp) {
        for (unsigned ecn = 0; ecn < 4; ++ecn) {
            GSocketControlMessage *message = g_ip_tos_message_new((guint8)dscp, (GEcnCodePoint)ecn);
            CHECK("new", message != NULL && G_IS_IP_TOS_MESSAGE(message));
            GSocketControlMessageClass *klass = G_SOCKET_CONTROL_MESSAGE_GET_CLASS(message);
            CHECK("class", klass != NULL && klass->deserialize != NULL);
            CHECK("metadata", g_socket_control_message_get_size(message) == 1 &&
                  g_socket_control_message_get_level(message) == IPPROTO_IP &&
                  g_socket_control_message_get_msg_type(message) == IP_TOS);
            CHECK("getters", g_ip_tos_message_get_dscp(G_IP_TOS_MESSAGE(message)) == dscp &&
                  (unsigned)g_ip_tos_message_get_ecn(G_IP_TOS_MESSAGE(message)) == ecn);
            unsigned char bytes[3] = { 0xa5, 0, 0x5a };
            g_socket_control_message_serialize(message, &bytes[1]);
            CHECK("serialize", bytes[0] == 0xa5 && bytes[1] == ((dscp << 2) | ecn) && bytes[2] == 0x5a);
            GSocketControlMessage *decoded = klass->deserialize(IPPROTO_IP, IP_TOS, 1, &bytes[1]);
            CHECK("deserialize", decoded && G_IS_IP_TOS_MESSAGE(decoded) &&
                  g_ip_tos_message_get_dscp(G_IP_TOS_MESSAGE(decoded)) == dscp &&
                  (unsigned)g_ip_tos_message_get_ecn(G_IP_TOS_MESSAGE(decoded)) == ecn &&
                  bytes[0] == 0xa5 && bytes[2] == 0x5a);
            CHECK("wrong-level", klass->deserialize(IPPROTO_TCP, IP_TOS, 1, &bytes[1]) == NULL);
            CHECK("wrong-type", klass->deserialize(IPPROTO_IP, IP_TOS + 1, 1, &bytes[1]) == NULL);
            CHECK("zero-size", klass->deserialize(IPPROTO_IP, IP_TOS, 0, &bytes[1]) == NULL);
            CHECK("large-size", klass->deserialize(IPPROTO_IP, IP_TOS, sizeof(bytes), bytes) == NULL);
            CHECK("guards", bytes[0] == 0xa5 && bytes[2] == 0x5a);
            g_object_unref(decoded);
            g_object_unref(message);
        }
    }
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK("socket", fd >= 0);
    int type = -1;
    socklen_t type_len = sizeof(type);
    CHECK("socket-type", getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &type_len) == 0 &&
          type == SOCK_DGRAM && type_len == sizeof(type));
    int tos = 0xb8;
    errno = 0;
    CHECK("set-refused", setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) == -1 && errno == ENOSYS && tos == 0xb8);
    int output = 0x12345678;
    socklen_t length = sizeof(output);
    errno = 0;
    CHECK("get-refused", getsockopt(fd, IPPROTO_IP, IP_TOS, &output, &length) == -1 &&
          errno == ENOSYS && output == 0x12345678 && length == sizeof(output));
    CHECK("close", close(fd) == 0);
    printf("gio-ip-tos: ok\n");
    return 0;
}
