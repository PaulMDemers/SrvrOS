#ifndef SRVROS_X86_64_USER_FRAME_COPY_H
#define SRVROS_X86_64_USER_FRAME_COPY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "fpu_trap.h"
#define USER_FRAME_COPY_LIMIT 1024u
#define USER_FRAME_STACK_FRAMES 32u
struct user_frame_source { _Alignas(16) uint8_t bytes[USER_FRAME_COPY_LIMIT]; };
_Static_assert(sizeof(struct user_frame_source)==1024 && _Alignof(struct user_frame_source)==16,"private frame source layout");
struct user_frame_stack { uint64_t base,physical,frames; };
struct user_frame_page { uint64_t virtual_address,physical_address,flags; };
/* Trusted private inputs only; current CPU0 IF0 continuation, no reusable plan.
 * The process wrapper validates exact current owned mappings and live proof.
 * This low-level helper revalidates hardware policy/translations and copies only
 * after all checks. No public/user caller or signal publication is wired. */
bool vmm_user_frame_copy_policy_current(uint64_t root);
bool vmm_copy_user_frame_current(uint64_t root,uint64_t destination,
    const struct user_frame_source *source,size_t length,
    const struct user_frame_stack *stack,const struct user_frame_page pages[2],size_t page_count,
    const struct isr_frame *frame,const struct fpu_state *user_fpu,const struct fpu_state *kernel_fpu);
bool process_copy_user_frame_current(const struct isr_frame *frame,
    const struct user_frame_source *source,uint64_t destination,size_t length);
#endif
