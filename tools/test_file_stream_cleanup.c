#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
static uint64_t process_test_irq_save(void);
static void process_test_irq_restore(uint64_t flags);
static uint64_t process_test_rflags(void);
#define SRVROS_PROCESS_HOST_TEST
#define SRVROS_STREAM_PROCESS_HOST_TEST
#define SRVROS_PROCESS_SERVICE_HOST_TEST
#define SRVROS_PROCESS_IRQ_HOST_TEST
#include "../kernel/src/process.c"
static bool irq_enabled,admit,manager_ok;
static unsigned saves,restores,begins,ends,manager_calls,depth;
static uint64_t last_owner;
static uint64_t process_test_irq_save(void){uint64_t flags=irq_enabled?(1ull<<9):0;irq_enabled=false;saves++;return flags;}
static void process_test_irq_restore(uint64_t flags){irq_enabled=(flags&(1ull<<9))!=0;restores++;}
static uint64_t process_test_rflags(void){return irq_enabled?(1ull<<9):0;}
bool scheduler_io_begin(void){assert(!irq_enabled);begins++;if(!admit)return false;depth++;return true;}
void scheduler_io_end(void){assert(!irq_enabled && depth);ends++;depth--;}
bool file_stream_cleanup(uint64_t owner){assert(!irq_enabled && depth);manager_calls++;last_owner=owner;return manager_ok;}
int main(void){
    irq_enabled=true;assert(process_stream_cleanup(0));
    assert(irq_enabled && !saves && !restores && !begins && !manager_calls);
    admit=false;assert(!process_stream_cleanup(17));
    assert(irq_enabled && saves==1 && restores==1 && begins==1 && !ends && !manager_calls);
    irq_enabled=false;assert(!process_stream_cleanup(17));
    assert(!irq_enabled && saves==2 && restores==2 && begins==2 && !ends && !manager_calls);
    irq_enabled=true;admit=true;manager_ok=true;assert(process_stream_cleanup(19));
    assert(irq_enabled && depth==0 && ends==1 && manager_calls==1 && last_owner==19);
    manager_ok=false;assert(!process_stream_cleanup(20));
    assert(irq_enabled && depth==0 && ends==2 && manager_calls==2 && last_owner==20);
    irq_enabled=false;depth=1;manager_ok=true;assert(process_stream_cleanup(21));
    assert(!irq_enabled && depth==1 && ends==3 && manager_calls==3 && last_owner==21);
    assert(saves==5 && restores==5 && begins==5);
    puts("file stream cleanup admission: PASS");return 0;
}
