"""Frozen qualification for the separate original full-GT native owner."""
import sys


def jobs(dependencies):
    outputs=['build/tests/intel-gt-bootstrap-adapter','build/tests/boot-gt-bootstrap',
             'build/tests/intel-gt-boot','build/tests/intel-ring-entry-adapter','build/tests/boot-ring-entry',
             'build/tests/intel-cache-probe','build/tests/intel-handoff-probe','build/tests/exfat-driver',
             'build/tests/intel-bdw-gt-bootstrap','build/tests/intel-bdw-gt-bootstrap-policy',
             'build/tests/bootlog-bootstrap','build/tests/workspace-log-bootstrap']
    build=dict(name='fixtures',argv=[sys.executable,'tools/fixture_compile.py','--under-workflow-lock',*outputs],
               cache=False,inputs=dependencies,outputs=outputs,timeout=1200)
    def test(name,*methods,timeout=1800):
        return dict(name=name,argv=[sys.executable,'-m','unittest','-v',*methods],
                    env={'PYTHONPATH':'tools','SRVROS_FIXTURE_BATCH':'1'},inputs=dependencies+outputs,outputs=[],timeout=timeout)
    prefix='test_intel_gt_bootstrap_native.Native.'
    quick=test('native-development',*[prefix+m for m in ('test_complete_original_owners_and_new_epoch',
        'test_recipe_preinit_and_profile_isolation','test_native_receiver_checkpoint_and_raw_preservation',
        'test_receiver_mutations_cannot_promote_a_partial_or_changed_source')],timeout=300)
    structural=test('workflow-structure','test_hardware_boot','test_hardware_desktop','test_device_workflow',timeout=300)
    image=dict(name='native-image',argv=['make','build/intel-gt-bootstrap/srvros.elf','build/initramfs.tar'],
               cache=False,inputs=dependencies,outputs=['build/intel-gt-bootstrap/srvros.elf','build/initramfs.tar'],timeout=1800)
    full=[build,quick,structural]
    for name,method in [('native-admissions','test_every_native_admission_boundary_retains_independent_parent'),
                        ('native-io','test_each_native_register_and_store_failure'),
                        ('native-cpu','test_cpu_cache_and_every_alias_attribute'),
                        ('native-pci','test_every_native_pci_read_float_stops_before_more_io'),
                        ('native-sanitizers','test_fatal_sanitizers'),
                        ('native-controls','test_compiled_native_weakening_controls'),
                        ('native-closure','test_completed_child_and_late_logical_return_cannot_release_physical_parent'),
                        ('native-isolation','test_selected_logger_capacity_and_native_image_isolation')]:
        full.append(test(name,prefix+method))
    full.extend([dict(name='reset-core',argv=['make','test-intel-bdw-gt-bootstrap'],inputs=dependencies,outputs=[],timeout=1800),
                 test('legacy-entry','test_intel_ring_entry',timeout=1800),image])
    return dict(development=[build,quick,structural,image],qualification=full)
