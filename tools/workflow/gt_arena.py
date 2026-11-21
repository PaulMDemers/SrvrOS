"""Private command storage on original full-GT lifetime; prior gates retained."""
import copy,sys
from workflow.gt_memory import jobs as previous


def jobs(dependencies):
    plan=copy.deepcopy(previous(dependencies))
    outputs=['build/tests/intel-gt-arena-adapter','build/tests/intel-gt-arena-boot','build/tests/intel-command-native',
             'build/tests/intel-command-mapping','build/tests/intel-command-mapping-policy','build/tests/intel-command-mapping-practical',
             'build/tests/intel-bdw-primary-range']
    outputs.extend('build/tests/'+name+suffix for name in ('intel-bdw-bcs-submit','intel-bdw-bcs-flush','intel-bdw-bcs-flush-submit')
                   for suffix in ('','-policy','-practical'))
    build=plan['development'][0];build['argv'].extend(outputs);build['outputs'].extend(outputs)
    for level in ('development','qualification'):
        plan[level].append(dict(name='arena-image',argv=['make','build/intel-gt-arena/srvros.elf','build/intel-command-memory/srvros.elf','build/intel-ggtt-native/srvros.elf'],cache=False,
            inputs=dependencies,outputs=['build/intel-gt-arena/srvros.elf','build/intel-gt-arena/probe.o','build/intel-command-memory/srvros.elf','build/intel-command-memory/probe.o','build/intel-ggtt-native/srvros.elf'],timeout=1800))
        tests=['test_intel_gt_arena']
        if level=='development':tests=['test_intel_gt_arena.Arena.'+m for m in (
            'test_original_parent_new_epoch_and_single_release','test_every_callback_and_terminal_child_retains_prefix',
            'test_receipts_and_old_wire_refuse_forged_completion','test_exact_recipe_stage_and_separate_image')]
        plan[level].append(dict(name='arena-native-model',argv=[sys.executable,'-m','unittest','-v',*tests],
            env={'PYTHONPATH':'tools','SRVROS_FIXTURE_BATCH':'1'},inputs=dependencies+outputs,outputs=[],timeout=1200))
        plan[level].append(dict(name='arena-legacy-policy',argv=[sys.executable,'-m','unittest','-v','test_intel_command_native','test_intel_command_mapping','test_intel_bdw_primary_range','test_intel_bdw_bcs_submit','test_intel_bdw_bcs_flush'],
            env={'PYTHONPATH':'tools','SRVROS_FIXTURE_BATCH':'1'},inputs=dependencies+outputs,outputs=[],timeout=1200))

    for index,name in enumerate(('arena-image','arena-native-model'),1):
        job=next(j for j in plan['development'] if j['name']==name)
        plan['development'].remove(job);plan['development'].insert(index,job)
    # Exercise explicit core/image allowlists before expensive original-owner
    # matrices. The full qualification suite still reruns both entire modules.
    image_guards=next(j for j in plan['development'] if j['name']=='memory-operation-cores')
    image_guards['argv'].extend(['test_intel_vtd_drain.Drain.test_native_image_is_explicit_and_old_images_stay_isolated',
        'test_intel_ggtt_invalidate.Invalidation.test_only_dedicated_combined_image_caller'])
    plan['development'].remove(image_guards);plan['development'].insert(1,image_guards)
    return plan
