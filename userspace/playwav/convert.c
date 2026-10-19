#include "convert.h"
int pcm_convert_init(struct pcm_convert *c,unsigned rate,uint64_t frames,
                     int (*read_frame)(void *,int16_t [2]),void *context) {
    if(!c || !read_frame || (rate!=44100 && rate!=48000) || !frames || frames>UINT32_MAX/2)return -1;
    *c=(struct pcm_convert){.context=context,.read_frame=read_frame,.rate=rate,.input_frames=frames,
                           .output_frames=(frames*48000+rate-1)/rate};
    if(read_frame(context,c->current)<0)return c->failed=-1;
    if(frames>1) { if(read_frame(context,c->next)<0)return c->failed=-1; }
    else {c->next[0]=c->current[0];c->next[1]=c->current[1];}
    return 0;
}
long pcm_convert_read(struct pcm_convert *c,int16_t *out,size_t capacity) {
    if(!c || c->failed || !out || !capacity || capacity>4096)return -1;
    size_t count=0;
    while(count<capacity && c->output_at<c->output_frames) {
        for(unsigned ch=0;ch<2;ch++)out[2*count+ch]=(int16_t)(((int64_t)c->current[ch]*(48000-c->fraction)+(int64_t)c->next[ch]*c->fraction)/48000);
        count++;c->output_at++;c->fraction+=c->rate;
        if(c->fraction>=48000) {
            c->fraction-=48000;c->input_at++;
            c->current[0]=c->next[0];c->current[1]=c->next[1];
            if(c->input_at+1<c->input_frames) {
                if(c->read_frame(c->context,c->next)<0)return c->failed=-1;
            } else {c->next[0]=c->current[0];c->next[1]=c->current[1];}
        }
    }
    return (long)count;
}
