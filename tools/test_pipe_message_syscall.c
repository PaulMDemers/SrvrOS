/* Fault-injected tests for pipe message syscall boundary and copyout. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_SYSCALL_IO_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"

static const void *unmapped;
static bool invalidate_after_wait;
static bool invalidate_legacy_copyout;
static unsigned receives, commits, sends, closed;
static unsigned legacy_calls;

bool vmm_user_range_mapped(uint64_t address, uint64_t length, bool write) {
    (void)length;
    (void)write;
    return address != (uint64_t)unmapped;
}
struct process *process_current(void) { return (struct process *)1; }
int64_t process_file_send_message(struct process *process, uint64_t fd,
    const uint8_t *buffer, uint64_t length, const struct srv_pipe_message *message) {
    assert(process == (struct process *)1 && fd == 7);
    assert(length == SRV_PIPE_MESSAGE_MAX && buffer[0] == 'A');
    assert(message->count == 1 && message->fds[0] == 9);
    sends++;
    return (int64_t)length;
}
int64_t process_file_recv_message_checked(struct process *process, uint64_t fd,
    uint8_t *buffer, uint64_t length, struct srv_pipe_message *message,
    process_message_copyout copyout, void *context) {
    assert(process == (struct process *)1 && fd == 7 && length >= 4);
    receives++;
    memcpy(buffer, "data", 4);
    message->count = 1;
    message->fds[0] = 12;
    extern const void *invalidate_pointer;
    if (invalidate_after_wait) unmapped = invalidate_pointer;
    if (!copyout(context, buffer, 4, message)) return SRV_ERR_FAULT;
    commits++;
    return 4;
}
const void *invalidate_pointer;
int64_t process_file_recv_rights_checked(struct process *process, uint64_t fd,
    uint64_t capacity, process_rights_copyout copyout, void *context) {
    assert(process == (struct process *)1 && fd == 7 && capacity == 1);
    legacy_calls++;
    int32_t delivered = 12;
    if (invalidate_legacy_copyout) unmapped = invalidate_pointer;
    if (!copyout(context, &delivered, 1, 0)) {
        closed++; /* The process layer rolls back the installed descriptor. */
        return SRV_ERR_FAULT;
    }
    return 1;
}

static void reset(void) {
    unmapped = invalidate_pointer = NULL;
    invalidate_after_wait = invalidate_legacy_copyout = false;
    receives = commits = sends = closed = legacy_calls = 0;
}

int main(void) {
    char input[SRV_PIPE_MESSAGE_MAX + 8];
    char first[2] = {0}, second[2] = {0};
    char control[16] = {0}, header[16] = {0};
    struct srv_iovec iov[] = {{first, sizeof(first)}, {second, sizeof(second)}};
    struct srv_pipe_message_layout layout = {iov, 2, control, sizeof(control), header, sizeof(header)};
    struct srv_pipe_message message = {.fds = {9}, .count = 1};

    memset(input, 'A', sizeof(input));
    reset();
    assert(syscall_pipe_message(7, &layout, &message, 2) == SRV_ERR_INVAL);
    assert(receives == 0 && sends == 0);

    struct srv_iovec single_iov = {input, sizeof(input)};
    struct srv_pipe_message_layout single_layout = {.iov = &single_iov, .iov_count = 1};
    reset();
    assert(syscall_pipe_message(7, &single_layout, &message, SRV_PIPE_MESSAGE_SEND) == SRV_PIPE_MESSAGE_MAX);
    assert(sends == 1);

    struct srv_iovec send_iov[] = {{input, 2}, {input + 2, SRV_PIPE_MESSAGE_MAX - 2}};
    struct srv_pipe_message_layout send_layout = {.iov = send_iov, .iov_count = 2};
    reset();
    assert(syscall_pipe_message(7, &send_layout, &message, SRV_PIPE_MESSAGE_SEND) == SRV_PIPE_MESSAGE_MAX);
    assert(sends == 1);

    reset();
    unmapped = input + 2;
    assert(syscall_pipe_message(7, &send_layout, &message, SRV_PIPE_MESSAGE_SEND) == SRV_ERR_FAULT);
    assert(sends == 0);

    reset();
    unmapped = input;
    assert(syscall_pipe_message(7, &single_layout, &message, SRV_PIPE_MESSAGE_SEND) == SRV_ERR_FAULT);
    assert(sends == 0);

    reset();
    message.flags = SRV_PIPE_MSG_CTRUNC;
    assert(syscall_pipe_message(7, &single_layout, &message, SRV_PIPE_MESSAGE_SEND) == SRV_ERR_INVAL);
    assert(sends == 0);
    message.flags = 0;

    reset();
    message.count = 1;
    assert(syscall_pipe_message(7, &layout, &message, SRV_PIPE_MESSAGE_RECV) == 4);
    assert(!memcmp(first, "da", 2) && !memcmp(second, "ta", 2));
    assert(message.fds[0] == 12 && message.count == 1 && commits == 1);

    /* A bad output on an empty live socket must fail before it can wait. */
    reset();
    unmapped = second;
    message.count = 1;
    assert(syscall_pipe_message(7, &layout, &message, SRV_PIPE_MESSAGE_RECV) == SRV_ERR_FAULT);
    assert(receives == 0 && commits == 0);

    reset();
    layout.header_length = SRV_PIPE_MESSAGE_AUX_MAX + 1;
    assert(syscall_pipe_message(7, &layout, &message, SRV_PIPE_MESSAGE_RECV) == SRV_ERR_INVAL);
    assert(receives == 0);
    layout.header_length = sizeof(header);

    const void *bad[] = {second, control, header, &message};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        reset();
        memset(first, 0, sizeof(first));
        memset(second, 0, sizeof(second));
        message.count = 1;
        invalidate_pointer = bad[i];
        invalidate_after_wait = true;
        assert(syscall_pipe_message(7, &layout, &message, SRV_PIPE_MESSAGE_RECV) == SRV_ERR_FAULT);
        assert(receives == 1 && commits == 0 && first[0] == 0 && second[0] == 0);
    }

    reset();
    int32_t legacy_fd = -1;
    uint64_t flags = 0;
    invalidate_pointer = &flags;
    invalidate_legacy_copyout = true;
    assert(syscall_pipe_recv_rights(7, &legacy_fd, 1, &flags) == -1);
    assert(legacy_calls == 1 && closed == 1);

    puts("pipe-message-syscall: PASS bounded send, scatter, revalidation and legacy cleanup");
    return 0;
}
