"""Same-owner full-GT memory child; retains all prior qualification gates."""
import copy,sys
from workflow.gt_practical import jobs as previous

def jobs(dependencies):
    plan=copy.deepcopy(previous(dependencies))
    outputs=['build/tests/intel-gt-memory-adapter','build/tests/intel-gt-memory-boot',
             'build/tests/intel-vtd-drain','build/tests/intel-vtd-drain-policy',
             'build/tests/intel-ggtt-invalidate','build/tests/intel-ggtt-invalidate-policy']
    build=plan['development'][0];build['argv'].extend(outputs);build['outputs'].extend(outputs)
    for level in ('development','qualification'):
        for name,methods in [('memory-native-model',['test_intel_gt_memory']),
                             ('memory-operation-cores',['test_intel_vtd_drain','test_intel_ggtt_invalidate'])]:
            if level=='development':
                methods=(["test_intel_gt_memory.Memory."+m for m in ('test_original_epoch_full_operations_and_retirement','test_every_completed_child_boundary_blocks_parent_return','test_receiver_mutations_cannot_forge_child','test_recipe_stage_exclusivity_and_image_isolation')] if name=='memory-native-model' else ['test_intel_vtd_drain.Drain.test_actual_core_fault_matrix','test_intel_ggtt_invalidate.Invalidation.test_actual_protocol_and_both_policy_boundaries'])
            plan[level].append(dict(name=name,argv=[sys.executable,'-m','unittest','-v',*methods],
                env={'PYTHONPATH':'tools','SRVROS_FIXTURE_BATCH':'1'},inputs=dependencies+outputs,outputs=[],timeout=1200))
        plan[level].append(dict(name='memory-image',argv=['make','build/intel-gt-memory/srvros.elf'],cache=False,
            inputs=dependencies,outputs=['build/intel-gt-memory/srvros.elf'],timeout=1800))
    return plan
