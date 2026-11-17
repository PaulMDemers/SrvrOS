"""Adds wide-budget gates without removing any original qualification job."""
import copy,sys
from workflow.gt_bootstrap import jobs as original

def jobs(dependencies):
    plan=copy.deepcopy(original(dependencies))
    outputs=['build/tests/intel-gt-seconds-adapter','build/tests/intel-gt-seconds-boot']
    build=plan['development'][0];build['argv'].extend(outputs);build['outputs'].extend(outputs)
    for level in ('development','qualification'):
        plan[level].append(dict(name='seconds-model-series',argv=[sys.executable,'-m','unittest','-v','test_intel_gt_seconds','test_gt_seconds_series'],
                               env={'PYTHONPATH':'tools','SRVROS_FIXTURE_BATCH':'1'},inputs=dependencies+outputs,outputs=[],timeout=600))
        plan[level].append(dict(name='seconds-image',argv=['make','build/intel-gt-seconds/srvros.elf'],cache=False,
                               inputs=dependencies,outputs=['build/intel-gt-seconds/srvros.elf'],timeout=1800))
    return plan
