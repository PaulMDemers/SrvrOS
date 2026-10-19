#include <srvros/audio.h>
#include <srvros/sys.h>
long srv_pcm_open(void) { return srv_syscall4(SYS_AUDIO_STREAM,SRV_PCM_OPEN,0,0,0); }
long srv_pcm_write(uint64_t h,const void *p,size_t n) { return srv_syscall4(SYS_AUDIO_STREAM,SRV_PCM_WRITE,(long)h,(long)p,n); }
long srv_pcm_finish(uint64_t h) { return srv_syscall4(SYS_AUDIO_STREAM,SRV_PCM_FINISH,(long)h,0,0); }
long srv_pcm_query(uint64_t h,struct srv_pcm_status *p) { return srv_syscall4(SYS_AUDIO_STREAM,SRV_PCM_QUERY,(long)h,(long)p,sizeof(*p)); }
long srv_pcm_close(uint64_t h) { return srv_syscall4(SYS_AUDIO_STREAM,SRV_PCM_CLOSE,(long)h,0,0); }
