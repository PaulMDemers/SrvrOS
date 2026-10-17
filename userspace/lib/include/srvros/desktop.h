#ifndef SRVROS_USER_DESKTOP_H
#define SRVROS_USER_DESKTOP_H
#include <srvros/desktop_protocol.h>
struct desk_packet desk_packet_new(uint32_t type);
int desk_send(uint64_t target, const struct desk_packet *packet);
/* 1 packet, 0 empty, -1 transport error, 2 discarded unrelated/invalid packet. */
int desk_receive(uint64_t *source, struct desk_packet *packet);
int desk_process_alive(uint64_t pid);
uint32_t *desk_pixels_alloc(uint32_t width, uint32_t height);
void desk_pixels_free(uint32_t *pixels, uint32_t width, uint32_t height);
#endif
