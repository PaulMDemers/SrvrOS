"""Read-only translation census; deliberately no reset/write-profile inheritance."""
import sys

def jobs(dependencies, *, aperture=False):
    outputs=['build/tests/'+name for name in ('intel-bdw-stolen-scanout','intel-stolen-probe',
        'boot-stolen','intel-display-probe','intel-cache-probe','intel-handoff-probe','exfat-driver','workspace-log-host','bootlog-host','vmm-prepare-host','vmm-diagnostic-range','vmm-kernel-alias','vmm-physical-unmapped','vmm-reserved-guard')]
    if aperture:outputs.append('build/tests/scanout-transfer')
    build=dict(name='fixtures',argv=[sys.executable,'tools/fixture_compile.py','--under-workflow-lock',*outputs],
               cache=False,inputs=dependencies,outputs=outputs,timeout=1200)
    image=dict(name='stolen-image',argv=['make','build/intel-stolen/srvros.elf'],cache=False,
               inputs=dependencies,outputs=['build/intel-stolen/srvros.elf','build/intel-stolen/layout.o','build/intel-bcs-context/context.o','build/intel-stolen/probe.o','build/intel-stolen/main.o'],timeout=1800)
    def tests(name,modules):
        return dict(name=name,argv=[sys.executable,'-m','unittest','-v',*modules],
                    env={'PYTHONPATH':'tools','SRVROS_FIXTURE_BATCH':'1'},inputs=dependencies+outputs,outputs=[],timeout=1200)
    quick=tests('census-development',['test_intel_stolen_probe.StolenProbe.test_actual_adapter_fault_prefixes',
        'test_intel_bdw_stolen_scanout.StolenScanout.test_every_translation_and_reserved_range',
        'test_intel_stolen_receipts'])
    full=tests('census-qualification',['test_intel_bdw_stolen_scanout','test_intel_stolen_probe','test_intel_stolen_receipts','test_vmm_physical_unmapped','test_vmm_reserved_guard'])
    if aperture:full['argv'].append('test_scanout_transfer')
    prior=tests('source-and-return-regression',['test_intel_display_capture','test_intel_display_backing',
        'test_intel_ggtt_window','test_intel_cache','test_intel_handoff','test_device_workflow','test_hardware_boot'])
    return dict(development=[build,image,quick],qualification=[build,image,full,prior])
