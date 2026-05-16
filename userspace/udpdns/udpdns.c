#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static uint16_t read_be16(const unsigned char *data) {
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static uint32_t read_be32(const unsigned char *data) {
    return ((uint32_t)data[0] << 24) |
        ((uint32_t)data[1] << 16) |
        ((uint32_t)data[2] << 8) |
        data[3];
}

static void write_be16(unsigned char *data, uint16_t value) {
    data[0] = (unsigned char)(value >> 8);
    data[1] = (unsigned char)value;
}

static int encode_name(unsigned char *packet, size_t *offset, size_t capacity, const char *name) {
    size_t label = 0;
    for (size_t i = 0;; i++) {
        if (name[i] == '.' || name[i] == '\0') {
            size_t length = i - label;
            if (length == 0 || length > 63 || *offset + length + 1 >= capacity) {
                return -1;
            }
            packet[(*offset)++] = (unsigned char)length;
            for (size_t j = label; j < i; j++) {
                packet[(*offset)++] = (unsigned char)name[j];
            }
            if (name[i] == '\0') {
                packet[(*offset)++] = 0;
                return 0;
            }
            label = i + 1;
        }
        if (i > 180) {
            return -1;
        }
    }
}

static size_t skip_name(const unsigned char *packet, size_t length, size_t offset) {
    unsigned jumps = 0;
    while (offset < length && jumps++ < 64) {
        unsigned char label = packet[offset++];
        if (label == 0) {
            return offset;
        }
        if ((label & 0xc0) == 0xc0) {
            return offset < length ? offset + 1 : 0;
        }
        if ((label & 0xc0) != 0 || offset + label > length) {
            return 0;
        }
        offset += label;
    }
    return 0;
}

static int build_query(unsigned char *packet, size_t *length, const char *name) {
    memset(packet, 0, 512);
    write_be16(packet + 0, 0x7372);
    write_be16(packet + 2, 0x0100);
    write_be16(packet + 4, 1);

    size_t offset = 12;
    if (encode_name(packet, &offset, 512, name) < 0 || offset + 4 > 512) {
        return -1;
    }
    write_be16(packet + offset, 1);
    offset += 2;
    write_be16(packet + offset, 1);
    offset += 2;
    *length = offset;
    return 0;
}

static int parse_response(const unsigned char *packet, size_t length, const char *name, uint32_t *ip_out) {
    if (length < 12 || read_be16(packet) != 0x7372) {
        return -1;
    }

    uint16_t qd = read_be16(packet + 4);
    uint16_t an = read_be16(packet + 6);
    size_t offset = 12;
    for (uint16_t i = 0; i < qd; i++) {
        offset = skip_name(packet, length, offset);
        if (offset == 0 || offset + 4 > length) {
            return -1;
        }
        offset += 4;
    }

    for (uint16_t i = 0; i < an; i++) {
        offset = skip_name(packet, length, offset);
        if (offset == 0 || offset + 10 > length) {
            return -1;
        }
        uint16_t type = read_be16(packet + offset);
        uint16_t class = read_be16(packet + offset + 2);
        uint16_t rdlength = read_be16(packet + offset + 8);
        offset += 10;
        if (offset + rdlength > length) {
            return -1;
        }
        if (type == 1 && class == 1 && rdlength == 4) {
            *ip_out = read_be32(packet + offset);
            return 0;
        }
        offset += rdlength;
    }

    printf("udpdns: no A answer for %s\n", name);
    return -1;
}

static void print_ip(uint32_t ip) {
    printf("%u.%u.%u.%u",
        (unsigned)((ip >> 24) & 0xff),
        (unsigned)((ip >> 16) & 0xff),
        (unsigned)((ip >> 8) & 0xff),
        (unsigned)(ip & 0xff));
}

int main(int argc, char **argv) {
    const char *name = argc > 1 ? argv[1] : "example.com";
    const char *server = argc > 2 ? argv[2] : "10.0.2.3";
    unsigned char packet[512];
    size_t query_length = 0;
    int fd = -1;
    struct sockaddr_in address;

    if (build_query(packet, &query_length, name) < 0) {
        printf("udpdns: bad name\n");
        return 2;
    }

    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        perror("udpdns: socket");
        return 1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(53);
    if (inet_pton(AF_INET, server, &address.sin_addr) != 1) {
        printf("udpdns: bad server\n");
        close(fd);
        return 2;
    }

    if (sendto(fd, packet, query_length, 0, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("udpdns: sendto");
        close(fd);
        return 1;
    }

    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
    };
    int ready = poll(&pfd, 1, 2000);
    if (ready <= 0) {
        printf("udpdns: timeout\n");
        close(fd);
        return 1;
    }

    ssize_t count = recvfrom(fd, packet, sizeof(packet), 0, 0, 0);
    if (count < 0) {
        perror("udpdns: recvfrom");
        close(fd);
        return 1;
    }

    uint32_t answer = 0;
    if (parse_response(packet, (size_t)count, name, &answer) < 0) {
        close(fd);
        return 1;
    }

    printf("udpdns: %s A ", name);
    print_ip(answer);
    printf("\n");
    close(fd);
    return 0;
}
