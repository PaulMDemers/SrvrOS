#include <assert.h>
#include "../userspace/playwav/io_stress_volume.h"
int main(void) {
 unsigned v=25;assert(io_stress_volume("1 200\n",100,&v)==1 && v==1);
 v=25;assert(!io_stress_volume("1 100\n",100,&v) && v==25);
 assert(!io_stress_volume("1 99\n",100,&v));
 const char *bad[]={"","0 200","101 200","1","1 200 extra","-1 200","1 -1","1 99999999"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(io_stress_volume(bad[i],100,&v)==-1 && v==25);
 assert(io_stress_volume("1 200",0,&v)==-1);
 puts("audio-io-volume: PASS explicit percentage, expiry, malformed/invalid input and unchanged default");
}
