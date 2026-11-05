#ifndef SRVROS_SIGNAL_ABI_H
#define SRVROS_SIGNAL_ABI_H
#include <srvros/signal_syscall_numbers.h>
/* Wire layouts only. This header does not allocate syscalls or enable delivery. */
#ifndef __ASSEMBLER__
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
#define SRV_SIGNAL_ALIGN16 alignas(16)
#define SRV_SIGNAL_ASSERT static_assert
#define SRV_SIGNAL_ALIGNOF alignof
#else
#define SRV_SIGNAL_ALIGN16 _Alignas(16)
#define SRV_SIGNAL_ASSERT _Static_assert
#define SRV_SIGNAL_ALIGNOF _Alignof
#endif
/* Public operations; capability reporting remains disabled until qualification. */
#define SRV_SIGNAL_CONTROL_FEATURES 1u
#define SRV_SIGNAL_CONTROL_QUERY_ACTION 2u
#define SRV_SIGNAL_CONTROL_EXCHANGE_ACTION 3u
#define SRV_SIGNAL_CONTROL_MASK 4u
#define SRV_SIGNAL_CONTROL_LEGACY_TAKE 5u
#define SRV_SIGNAL_CONTROL_BLOCKED_TAKE 6u
#define SRV_SIGNAL_NATIVE_INFO UINT32_C(0x00000004)
#define SRV_SIGNAL_NATIVE_RESTART UINT32_C(0x10000000)
#define SRV_SIGNAL_NATIVE_RESET UINT32_C(0x80000000)
#define SRV_SIGNAL_SUSPEND_BEGIN 1u
#define SRV_SIGNAL_SUSPEND_FINISH 2u
#define SRV_SIGNAL_IO_BEGIN 1u
#define SRV_SIGNAL_IO_CALL 2u
#define SRV_SIGNAL_IO_END 3u
#define SRV_SIGNAL_IO_UNWIND 4u
#define SRV_SIGNAL_IO_LOCAL_BEGIN 5u
#define SRV_SIGNAL_IO_LOCAL_END 6u
#define SRV_SIGNAL_IO_RESULT 7u
#define SRV_SIGNAL_IO_CAPTURE_DESCRIPTORS 8u
#define SRV_SIGNAL_IO_POLL_LIST UINT64_C(1)
#define SRV_SIGNAL_IO_LEGACY_MASKED UINT64_C(2)
#define SRV_SIGNAL_IO_FINALIZE_NATIVE UINT64_C(1)
#define SRV_SIGNAL_IO_RESULT_MONO_VALID UINT64_C(1)
#define SRV_SIGNAL_IO_RESULT_TICKS_VALID UINT64_C(2)
#define SRV_SIGNAL_IO_RESULT_COMPLETED_NATIVE UINT64_C(4)
#define SRV_SIGNAL_ROOT_RAW 1u
#define SRV_SIGNAL_ROOT_READ 2u
#define SRV_SIGNAL_ROOT_WRITE 3u
#define SRV_SIGNAL_ROOT_POLL 4u
#define SRV_SIGNAL_ROOT_EPOLL 5u
#define SRV_SIGNAL_ROOT_SEM_WAIT 6u
#define SRV_SIGNAL_ROOT_WAITPID 7u
#define SRV_SIGNAL_ROOT_SLEEP 8u
#define SIGNAL_CONTEXT_VERSION 1u
#define SIGNAL_CONTEXT_FPU_SIZE 512u
struct signal_context_snapshot {
    uint32_t size,version;
    uint64_t flags,token,interrupted_mask;
    uint64_t r15,r14,r13,r12,r11,r10,r9,r8;
    uint64_t rbp,rdi,rsi,rdx,rcx,rbx,rax;
    uint64_t rip,rsp,rflags;
    SRV_SIGNAL_ALIGN16 uint8_t fpu[SIGNAL_CONTEXT_FPU_SIZE];
};
struct srv_signal_info_v1 { uint32_t size,version,signo,code; uint64_t valid,target_tid,reserved[4]; };
struct srv_signal_action_v1 { uint64_t handler,mask; uint32_t flags,reserved; };
struct srv_signal_control_v1 {
    uint32_t size,version,operation,signum;
    struct srv_signal_action_v1 action;
    uint64_t trampoline,mask_how,mask,reserved[4];
};
struct srv_signal_features_v1 { uint32_t size,version; uint64_t capabilities,info_signals,info_flags,context_bytes,info_bytes,max_depth,reserved; };
struct srv_signal_legacy_take_v1 { uint32_t size,version; uint64_t signum; struct srv_signal_action_v1 action; uint64_t previous_mask,generation,reserved; };
struct srv_signal_io_v1 { uint32_t size,version; uint64_t operation,ticket,raw_syscall,args[6],descriptor_vector,descriptor_count,flags,deadline,reserved[2]; };
struct srv_signal_io_result_v1 { uint32_t size,version; uint64_t cookie,syscall_number; int64_t result; uint64_t elapsed_us,elapsed_ticks,validity,reserved; };
struct srv_signal_descriptor_v1 { int64_t fd; uint64_t value; }; /* reserved input / generation output */
SRV_SIGNAL_ASSERT(sizeof(struct srv_signal_descriptor_v1)==16,"descriptor wire size");
SRV_SIGNAL_ASSERT(sizeof(struct signal_context_snapshot)==688,"snapshot wire size");
SRV_SIGNAL_ASSERT(offsetof(struct signal_context_snapshot,fpu)==176,"snapshot FX offset");
SRV_SIGNAL_ASSERT(sizeof(struct srv_signal_info_v1)==64,"info wire size");
SRV_SIGNAL_ASSERT(sizeof(struct srv_signal_action_v1)==24,"action wire size");
SRV_SIGNAL_ASSERT(sizeof(struct srv_signal_control_v1)==96,"control wire size");
SRV_SIGNAL_ASSERT(sizeof(struct srv_signal_features_v1)==64,"features wire size");
SRV_SIGNAL_ASSERT(sizeof(struct srv_signal_legacy_take_v1)==64,"legacy wire size");
SRV_SIGNAL_ASSERT(sizeof(struct srv_signal_io_v1)==128,"IO wire size");
SRV_SIGNAL_ASSERT(sizeof(struct srv_signal_io_result_v1)==64,"IO result wire size");
#define SRV_SIGNAL_OFFSET(t,f,n) SRV_SIGNAL_ASSERT(offsetof(struct t,f)==(n),#t "." #f " offset")
SRV_SIGNAL_ASSERT(SRV_SIGNAL_ALIGNOF(struct signal_context_snapshot)==16,"signal_context_snapshot alignment");
SRV_SIGNAL_OFFSET(signal_context_snapshot,size,0);
SRV_SIGNAL_OFFSET(signal_context_snapshot,version,4);
SRV_SIGNAL_OFFSET(signal_context_snapshot,flags,8);
SRV_SIGNAL_OFFSET(signal_context_snapshot,token,16);
SRV_SIGNAL_OFFSET(signal_context_snapshot,interrupted_mask,24);
SRV_SIGNAL_OFFSET(signal_context_snapshot,r15,32);
SRV_SIGNAL_OFFSET(signal_context_snapshot,r14,40);
SRV_SIGNAL_OFFSET(signal_context_snapshot,r13,48);
SRV_SIGNAL_OFFSET(signal_context_snapshot,r12,56);
SRV_SIGNAL_OFFSET(signal_context_snapshot,r11,64);
SRV_SIGNAL_OFFSET(signal_context_snapshot,r10,72);
SRV_SIGNAL_OFFSET(signal_context_snapshot,r9,80);
SRV_SIGNAL_OFFSET(signal_context_snapshot,r8,88);
SRV_SIGNAL_OFFSET(signal_context_snapshot,rbp,96);
SRV_SIGNAL_OFFSET(signal_context_snapshot,rdi,104);
SRV_SIGNAL_OFFSET(signal_context_snapshot,rsi,112);
SRV_SIGNAL_OFFSET(signal_context_snapshot,rdx,120);
SRV_SIGNAL_OFFSET(signal_context_snapshot,rcx,128);
SRV_SIGNAL_OFFSET(signal_context_snapshot,rbx,136);
SRV_SIGNAL_OFFSET(signal_context_snapshot,rax,144);
SRV_SIGNAL_OFFSET(signal_context_snapshot,rip,152);
SRV_SIGNAL_OFFSET(signal_context_snapshot,rsp,160);
SRV_SIGNAL_OFFSET(signal_context_snapshot,rflags,168);
SRV_SIGNAL_OFFSET(signal_context_snapshot,fpu,176);
SRV_SIGNAL_ASSERT(SRV_SIGNAL_ALIGNOF(struct srv_signal_info_v1)==8,"srv_signal_info_v1 alignment");
SRV_SIGNAL_OFFSET(srv_signal_info_v1,size,0);
SRV_SIGNAL_OFFSET(srv_signal_info_v1,version,4);
SRV_SIGNAL_OFFSET(srv_signal_info_v1,signo,8);
SRV_SIGNAL_OFFSET(srv_signal_info_v1,code,12);
SRV_SIGNAL_OFFSET(srv_signal_info_v1,valid,16);
SRV_SIGNAL_OFFSET(srv_signal_info_v1,target_tid,24);
SRV_SIGNAL_OFFSET(srv_signal_info_v1,reserved,32);
SRV_SIGNAL_ASSERT(SRV_SIGNAL_ALIGNOF(struct srv_signal_action_v1)==8,"srv_signal_action_v1 alignment");
SRV_SIGNAL_OFFSET(srv_signal_action_v1,handler,0);
SRV_SIGNAL_OFFSET(srv_signal_action_v1,mask,8);
SRV_SIGNAL_OFFSET(srv_signal_action_v1,flags,16);
SRV_SIGNAL_OFFSET(srv_signal_action_v1,reserved,20);
SRV_SIGNAL_ASSERT(SRV_SIGNAL_ALIGNOF(struct srv_signal_control_v1)==8,"srv_signal_control_v1 alignment");
SRV_SIGNAL_OFFSET(srv_signal_control_v1,size,0);
SRV_SIGNAL_OFFSET(srv_signal_control_v1,version,4);
SRV_SIGNAL_OFFSET(srv_signal_control_v1,operation,8);
SRV_SIGNAL_OFFSET(srv_signal_control_v1,signum,12);
SRV_SIGNAL_OFFSET(srv_signal_control_v1,action,16);
SRV_SIGNAL_OFFSET(srv_signal_control_v1,trampoline,40);
SRV_SIGNAL_OFFSET(srv_signal_control_v1,mask_how,48);
SRV_SIGNAL_OFFSET(srv_signal_control_v1,mask,56);
SRV_SIGNAL_OFFSET(srv_signal_control_v1,reserved,64);
SRV_SIGNAL_ASSERT(SRV_SIGNAL_ALIGNOF(struct srv_signal_features_v1)==8,"srv_signal_features_v1 alignment");
SRV_SIGNAL_OFFSET(srv_signal_features_v1,size,0);
SRV_SIGNAL_OFFSET(srv_signal_features_v1,version,4);
SRV_SIGNAL_OFFSET(srv_signal_features_v1,capabilities,8);
SRV_SIGNAL_OFFSET(srv_signal_features_v1,info_signals,16);
SRV_SIGNAL_OFFSET(srv_signal_features_v1,info_flags,24);
SRV_SIGNAL_OFFSET(srv_signal_features_v1,context_bytes,32);
SRV_SIGNAL_OFFSET(srv_signal_features_v1,info_bytes,40);
SRV_SIGNAL_OFFSET(srv_signal_features_v1,max_depth,48);
SRV_SIGNAL_OFFSET(srv_signal_features_v1,reserved,56);
SRV_SIGNAL_ASSERT(SRV_SIGNAL_ALIGNOF(struct srv_signal_legacy_take_v1)==8,"srv_signal_legacy_take_v1 alignment");
SRV_SIGNAL_OFFSET(srv_signal_legacy_take_v1,size,0);
SRV_SIGNAL_OFFSET(srv_signal_legacy_take_v1,version,4);
SRV_SIGNAL_OFFSET(srv_signal_legacy_take_v1,signum,8);
SRV_SIGNAL_OFFSET(srv_signal_legacy_take_v1,action,16);
SRV_SIGNAL_OFFSET(srv_signal_legacy_take_v1,previous_mask,40);
SRV_SIGNAL_OFFSET(srv_signal_legacy_take_v1,generation,48);
SRV_SIGNAL_OFFSET(srv_signal_legacy_take_v1,reserved,56);
SRV_SIGNAL_ASSERT(SRV_SIGNAL_ALIGNOF(struct srv_signal_io_v1)==8,"srv_signal_io_v1 alignment");
SRV_SIGNAL_OFFSET(srv_signal_io_v1,size,0);
SRV_SIGNAL_OFFSET(srv_signal_io_v1,version,4);
SRV_SIGNAL_OFFSET(srv_signal_io_v1,operation,8);
SRV_SIGNAL_OFFSET(srv_signal_io_v1,ticket,16);
SRV_SIGNAL_OFFSET(srv_signal_io_v1,raw_syscall,24);
SRV_SIGNAL_OFFSET(srv_signal_io_v1,args,32);
SRV_SIGNAL_OFFSET(srv_signal_io_v1,descriptor_vector,80);
SRV_SIGNAL_OFFSET(srv_signal_io_v1,descriptor_count,88);
SRV_SIGNAL_OFFSET(srv_signal_io_v1,flags,96);
SRV_SIGNAL_OFFSET(srv_signal_io_v1,deadline,104);
SRV_SIGNAL_OFFSET(srv_signal_io_v1,reserved,112);
SRV_SIGNAL_ASSERT(SRV_SIGNAL_ALIGNOF(struct srv_signal_io_result_v1)==8,"srv_signal_io_result_v1 alignment");
SRV_SIGNAL_OFFSET(srv_signal_io_result_v1,size,0);
SRV_SIGNAL_OFFSET(srv_signal_io_result_v1,version,4);
SRV_SIGNAL_OFFSET(srv_signal_io_result_v1,cookie,8);
SRV_SIGNAL_OFFSET(srv_signal_io_result_v1,syscall_number,16);
SRV_SIGNAL_OFFSET(srv_signal_io_result_v1,result,24);
SRV_SIGNAL_OFFSET(srv_signal_io_result_v1,elapsed_us,32);
SRV_SIGNAL_OFFSET(srv_signal_io_result_v1,elapsed_ticks,40);
SRV_SIGNAL_OFFSET(srv_signal_io_result_v1,validity,48);
SRV_SIGNAL_OFFSET(srv_signal_io_result_v1,reserved,56);
#undef SRV_SIGNAL_OFFSET
#undef SRV_SIGNAL_ALIGNOF
#undef SRV_SIGNAL_ALIGN16
#undef SRV_SIGNAL_ASSERT
#endif
#endif
