"""Deterministic, quiet stereo channel-check WAV for the bundled player."""
import io
import struct
import wave


def channel_test_wav():
    samples=bytearray(6*48000*4)
    for frame in range(6*48000):
        part=frame//96000;local=frame%96000
        if not 9600<=local<86400:continue
        envelope=min(local-9600,86399-local,960)
        a=frame%96;b=frame%64
        left=(a-24 if a<48 else 72-a)*160
        right=(b-16 if b<32 else 48-b)*240
        left=int(left*envelope/960) if part!=1 else 0
        right=int(right*envelope/960) if part!=0 else 0
        struct.pack_into('<hh',samples,frame*4,left,right)
    out=io.BytesIO()
    with wave.open(out,'wb') as wav:
        wav.setparams((2,2,48000,0,'NONE',''));wav.writeframes(samples)
    return out.getvalue()
