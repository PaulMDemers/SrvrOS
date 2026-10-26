"""Deterministic, quiet stereo channel-check WAV for the bundled player."""
import io
import struct
import wave


def channel_test_wav(repeats=1):
    if not isinstance(repeats,int) or not 1<=repeats<=20:raise ValueError("Expected 1 to 20 channel-check cycles")
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
        wav.setparams((2,2,48000,0,'NONE',''));wav.writeframes(samples*repeats)
    return out.getvalue()


def verify_channel_capture(samples, rate, start_frame=0):
    """Locate the channel fixture after earlier audio and compare every sample."""
    from array import array
    assert rate == 48000 and len(samples) % 2 == 0
    with wave.open(io.BytesIO(channel_test_wav()), 'rb') as wav:
        expected = array('h', wav.readframes(wav.getnframes()))
    expected = array('h', (int(value / 4) for value in expected))
    def active(data, start):
        return next(i for i in range(start, len(data)//2)
                    if max(abs(data[2*i]), abs(data[2*i+1])) > 20)
    first = active(expected, 0)
    actual_first = active(samples, start_frame)
    origin = actual_first - first
    last = max(i for i in range(len(expected)//2)
               if max(abs(expected[2*i]), abs(expected[2*i+1])) > 20)
    finish = last + 4800
    assert origin >= start_frame and origin + finish <= len(samples)//2, 'Truncated channel-check capture'
    maximum = max(abs(samples[(origin+i)*2+ch]-expected[i*2+ch])
                  for i in range(first, finish) for ch in (0,1))
    assert maximum <= 2, ('Channel-check waveform mismatch', maximum)
    return dict(status='PASS', max_sample_error=maximum, left_only=True,
                right_only=True, distinct_stereo=True, quiet_tail=True)
