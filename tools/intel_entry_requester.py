"""Original state-aware entry/reset and original-claim requester restoration."""
from intel_ring_reset import reset_recipe,reset_result,verify_reset_checkpoint
from intel_gpu_hold import gpu_hold_result

def requester_recipe(cmd):return reset_recipe(cmd,requester=True)
def requester_result(text):
 from intel_completion_audit import entry_census_lines
 entry_census_lines(text)

 r=reset_result(text,requester=True)
 if not r:return None
 g=gpu_hold_result(text,entry_reset=True)
 if not g or r['joint']!=g['joint']:raise ValueError('entry/requester original joint source')
 complete=bool(r['complete'] and g['complete'])
 if r['joint']['wake']['summary']['complete'] and not complete:raise ValueError('entry/requester false enclosing completion')
 return dict(complete=complete,native_complete=False,entry_reset=r,requester=g,hardware_ownership=False,managed_pages=0,physical_tearfree=False,enabled_writer_complete=False)
def native_requester_result(text):
 r=requester_result(text)
 if r and r['complete']:
  from intel_gt_wake import native_wake_result
  if not native_wake_result(text)['native_wake_complete']:raise ValueError('entry/requester native prerequisites')
  r['native_complete']=True
 return r
def verify_requester_checkpoint(pre,text,native=False):
 if 'intel-gpu-hold-' in pre:raise ValueError('entry/requester late prerequisite checkpoint')
 verify_reset_checkpoint(pre,text,native=native,requester=True)
 if not requester_result(text):raise ValueError('entry/requester missing final result')
 return True
