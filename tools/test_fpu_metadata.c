/* No trap capabilities are minted; exercise only metadata initialization. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/arch/x86_64/fpu.c"
void console_write(const char *s) { (void)s; }
int main(void) {
    struct fpu_state state={0};
    memset(state.fxsave_area,0x71,sizeof(state.fxsave_area));
    state.active_user_trap=(void *)1;available=false;
    fpu_init_state(&state);assert(!state.initialized && !state.active_user_trap);
    for(unsigned i=0;i<sizeof(state.fxsave_area);i++)assert(!state.fxsave_area[i]);
    memset(state.fxsave_area,0x62,sizeof(state.fxsave_area));state.initialized=true;
    state.active_user_trap=(void *)2;fpu_invalidate_user_trap(&state);
    assert(!state.active_user_trap && state.initialized);
    for(unsigned i=0;i<sizeof(state.fxsave_area);i++)assert(state.fxsave_area[i]==0x62);
    fpu_invalidate_user_trap(NULL);
    for(unsigned i=0;i<sizeof(initial_fxsave_area);i++)initial_fxsave_area[i]=(uint8_t)(i*7+3);
    state.active_user_trap=(void *)3;available=true;
    fpu_init_state(&state);assert(state.initialized && !state.active_user_trap);
    assert(!memcmp(state.fxsave_area,initial_fxsave_area,sizeof(state.fxsave_area)));
    puts("fpu-metadata-host: PASS (initialization and explicit invalidation only)");
}
