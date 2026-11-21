"""Private CPU context candidate on original full-GT arena; no GPU grant."""
import copy,sys
from workflow.gt_arena import jobs as previous

def jobs(dependencies):
    plan=copy.deepcopy(previous(dependencies))
    outputs=['build/tests/intel-bcs-context-adapter','build/tests/intel-bcs-context-boot','build/tests/intel-bdw-bcs-context']
    first=plan['development'][0];first['argv'].extend(outputs);first['outputs'].extend(outputs)
    for level in ('development','qualification'):
        plan[level].append(dict(name='context-image',argv=['make','build/intel-bcs-context/srvros.elf'],cache=False,
            inputs=dependencies,outputs=['build/intel-bcs-context/srvros.elf','build/intel-bcs-context/context.o','build/intel-bcs-context/probe.o'],timeout=1800))
        plan[level].append(dict(name='context-model',argv=[sys.executable,'-m','unittest','-v','test_intel_bdw_bcs_context','test_intel_bcs_context'],
            env={'PYTHONPATH':'tools','SRVROS_FIXTURE_BATCH':'1'},inputs=dependencies+outputs,outputs=[],timeout=1200))
    return plan
