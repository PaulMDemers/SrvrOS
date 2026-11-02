"""Strict projection of disabled-device workspace checkpoints in Wi-Fi traces.

Keep the original raw log for prefix and completion verification. Only remove
complete seven-line cached checkpoints from the per-visit Wi-Fi projection;
unrecognized lines remain for the caller's diagnostic-family validator.
The fixed disabled-device profile is deliberate. Other profiles must be
validated separately rather than silently broadening this diagnostic gate.
"""
import re

def scan_checkpoint_trace(leg,raw,start):
 assert isinstance(start,int) and 0<=start<=len(raw) and raw[start:start+len(leg)]==leg
 lines=leg.splitlines(keepends=True);out=[];rows=[];offset=start;i=0
 while i<len(lines):
  line=lines[i]
  if not line.startswith('workspace-scheduler:'):
   out.append(line);offset+=len(line);i+=1;continue
  group=lines[i:i+7];assert len(group)==7 and len(rows)<2
  match=re.fullmatch(r'workspace-scheduler: sampled_ticks=(\d+) idle_ticks=(\d+) switches=(\d+) preemptions=(\d+) monotonic_us=(\d+)\n',group[0]);assert match
  values=list(map(int,match.groups()));assert all(str(v)==s and 0<=v<1<<64 for s,v in zip(match.groups(),values))
  ticks,idle,switches,preempt,sampled=values;assert ticks and sampled and idle<=ticks and preempt<=switches
  if rows:assert sampled>rows[-1]['sampled_us']
  expected=[
   f'workspace-devices: smc_state=0 flags=0x0000000000000000 fan_millirpm=0 sample_us=0 samples=0 failures=0 error=0 monotonic_us={sampled}\n',
   f'workspace-devices: pcm_state=0 owner=0 handle=0 submitted=0 position=0 queued=0 underruns=0 monotonic_us={sampled}\n',
   f'workspace-devices: audio_state=0 error=0 output=0 volume=25 pending=0 started=0 completed=0 headphones=0 caps=0x0000000000000000 monotonic_us={sampled}\n',
   f'workspace-devices: thermal_state=0 flags=0x0000000000000000 core_mc=0 package_mc=0 sample_us=0 samples=0 monotonic_us={sampled}\n']
  assert group[1:5]==expected
  snapshot=re.fullmatch(r'workspace-log: snapshot=(session-[ab]\.log) monotonic_us=(\d+) total_before=(\d+) tail_limit=1048576\n',group[5]);assert snapshot
  name,stamp,total=snapshot.groups();stamp=int(stamp);total=int(total)
  assert sampled<=stamp<1<<64 and str(stamp)==snapshot[2] and str(total)==snapshot[3]
  position=offset+sum(map(len,group[:5]));assert total==len(raw[:position].encode())
  assert raw[offset:offset+sum(map(len,group))]==''.join(group)
  assert group[6]=='workspace: flushed\n'
  rows.append(dict(name=name,sampled_us=sampled,snapshot_us=stamp,total_before=total,snapshot_bytes=total+len(group[5].encode())))
  offset+=sum(map(len,group));i+=7
 return ''.join(out),rows
