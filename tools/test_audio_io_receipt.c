#include "../userspace/playwav/io_stress_receipt.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    struct audio_io_receipt r={.first=2000000,.last=11000000,.bursts=50,.bytes=50*262144,.max_us=500000};
#define VALID(v) audio_io_receipt_valid(&(v),12,1000000,13000000,2304000,0)
    assert(VALID(r));
    struct audio_io_receipt v=r;v.first=14000000;v.last=24000000;assert(!VALID(v)); /* finished audio before I/O */
    v=r;v.bursts=0;v.bytes=0;assert(!VALID(v));
    v=r;v.bytes--;assert(!VALID(v));
    v=r;v.first=1;assert(!VALID(v));
    v=r;v.last=14000000;assert(!VALID(v));
    v=r;v.last=v.first+1000000;assert(!VALID(v)); /* one burst is not sustained */
    assert(!audio_io_receipt_valid(&r,12,1000000,13000000,2304000,1));
    assert(!audio_io_receipt_valid(&r,12,1000000,13000000,192000,0));
    assert(!audio_io_receipt_valid(&r,12,0,13000000,2304000,0));
    /* Extra PCM may cover an already admitted slow final burst. Neither the
     * sustained disk span nor its containment in live playback is relaxed. */
    v=r;v.last=14000000;
    assert(audio_io_receipt_valid(&v,12,1000000,15000000,2496000,0));
    assert(!audio_io_receipt_valid(&v,12,1000000,13000000,2496000,0));
    assert(!audio_io_receipt_valid(&r,12,1000000,75000000,73ull*192000,0));
    assert(!audio_io_receipt_valid(&r,12,1000000,15000000,2496001,0));
    puts("audio I/O receipt regression: PASS");
}
