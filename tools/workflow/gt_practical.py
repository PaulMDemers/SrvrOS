"""Measured practical policy, retaining every original and wide-policy gate."""
import copy,sys
from workflow.gt_seconds import jobs as previous

def jobs(dependencies):
    plan=copy.deepcopy(previous(dependencies))
    outputs=['build/tests/intel-gt-practical-adapter','build/tests/intel-gt-practical-boot']
    build=plan['development'][0];build['argv'].extend(outputs);build['outputs'].extend(outputs)
    for level in ('development','qualification'):
        plan[level].append(dict(name='practical-model-policy',argv=[sys.executable,'-m','unittest','-v','test_intel_gt_practical'],
            env={'PYTHONPATH':'tools','SRVROS_FIXTURE_BATCH':'1','SRVROS_REQUIRE_GT_SECONDS_HISTORY':'1'},
            inputs=dependencies+outputs+['build/device-fixtures/gt-seconds-history/*/*.log','build/device-fixtures/gt-seconds-history/*/report.json'],outputs=[],timeout=600))
        plan[level].append(dict(name='practical-image',argv=['make','build/intel-gt-practical/srvros.elf'],cache=False,
            inputs=dependencies,outputs=['build/intel-gt-practical/srvros.elf'],timeout=1800))
    return plan
