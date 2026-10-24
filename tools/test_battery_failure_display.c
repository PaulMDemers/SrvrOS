#include <assert.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/runtime/battery_failure_display.h"
static unsigned calls,fail_at;
static bool delay(uint64_t us){assert(us==100000);return calls++!=fail_at;}
int main(void){
 for(unsigned failure=0;failure<=300;failure++){
  calls=0;fail_at=failure;
  assert(battery_failure_display_wait(delay)==failure);
  assert(calls==(failure<300?failure+1:300));
 }
 puts("battery failure display: bounded success and every clock-failure boundary PASS");
}
