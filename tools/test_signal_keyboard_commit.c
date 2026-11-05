#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
static bool irqs=true,retire_after_commit;
static int refusal;
static unsigned commits,waits;
static uint64_t test_save(void){uint64_t old=irqs?512:0;irqs=false;return old;}
static void test_restore(uint64_t old){if(old&512)irqs=true;if(irqs && retire_after_commit && commits){refusal=-4;retire_after_commit=false;}}
#define SRVROS_KEYBOARD_HOST_TEST
#define SRVROS_KEYBOARD_TEST_IRQ_SAVE test_save
#define SRVROS_KEYBOARD_TEST_IRQ_RESTORE test_restore
#include "../kernel/src/arch/x86_64/keyboard.c"
static bool serial_ready;
static char serial_byte,published;
static unsigned serial_consumed;
bool serial_has_char(void){return serial_ready;}
bool serial_try_read_char(char *out){assert(!irqs);if(!serial_ready)return false;*out=serial_byte;serial_ready=false;serial_consumed++;return true;}
void scheduler_wake_all(struct scheduler_wait_queue *q){(void)q;}
void process_file_poll_wake(void){}
bool process_should_exit_current(void){return false;}
bool process_signal_pending_current(void){return refusal==-4;}
static int eligible(void *ctx){assert(ctx==&published && !irqs);return refusal;}
static void commit(void *ctx,char c){assert(ctx==&published && !irqs && !refusal);published=c;commits++;}
static int wake_mode;
bool scheduler_wait(struct scheduler_wait_queue *q,bool(*condition)(void *),void *ctx){
    assert(q==&keyboard_wait_queue && irqs && ++waits<=1);
    if(wake_mode==1)refusal=-4;
    else if(wake_mode==2)keyboard_inject_char('W');
    uint64_t flags=test_save();assert(condition(ctx));test_restore(flags);return true;
}
int main(void){
    char out='?';keyboard_inject_char('Q');refusal=-9;
    assert(keyboard_read_char_commit(&out,false,eligible,commit,&published)==-9 && out=='?' && keyboard_buffered_count()==1 && !commits);
    refusal=0;assert(keyboard_read_char_commit(&out,false,eligible,commit,&published)==1 && out=='Q' && published=='Q' && commits==1);
    serial_ready=true;serial_byte=0;refusal=-14;
    assert(keyboard_read_char_commit(&out,false,eligible,commit,&published)==-14 && serial_ready && !serial_consumed);
    refusal=0;assert(keyboard_read_char_commit(&out,false,eligible,commit,&published)==1 && out==0 && published==0 && serial_consumed==1);
    assert(keyboard_read_char_commit(&out,false,eligible,commit,&published)==0 && commits==2);
    wake_mode=1;waits=0;assert(keyboard_read_char_commit(&out,true,eligible,commit,&published)==-4 && waits==1 && irqs);
    refusal=0;wake_mode=2;waits=0;assert(keyboard_read_char_commit(&out,true,eligible,commit,&published)==1 && out=='W' && waits==1);
    keyboard_inject_char('P');retire_after_commit=true;commits=0;refusal=0;
    assert(keyboard_read_char_commit(&out,true,eligible,commit,&published)==1 && out=='P' && published=='P' && commits==1 && refusal==-4);
    assert(!keyboard_buffered_count() && irqs);puts("signal-keyboard: atomic eligibility/consume/commit PASS");
}
