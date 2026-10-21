import tempfile
import unittest
from pathlib import Path
import hardware_desktop as desktop

class CollectionTests(unittest.TestCase):
    def test_automatic_return_does_not_clear_failure(self):
        self.assertTrue(desktop.needs_attention("laptop-capture: completed=NO"))
        self.assertTrue(desktop.needs_attention("battery: FAILED operation deadline"))
        self.assertIsNone(desktop.battery_service_result(
            "laptop-capture: returning to Ubuntu through normal firmware/GRUB boot\n"))

    def test_release_identity_requires_complete_boot_option(self):
        release="0620ce5e026b2f32"
        text=(f"cmdline: srvros.desktop-release={release}\n"
              f"desktop-trial: boot options=srvros.desktop-release={release}\n"
              "terminal: srvros.desktop-release=ffffffffffffffff\n"
              "cmdline: srvros.desktop-release=11111111111111111\n"
              "cmdline: srvros.desktop-release=2222222222222222suffix\n")
        self.assertEqual(desktop.session_releases(text),[release])
        self.assertEqual(desktop.session_releases("desktop started\n"),[])

    def test_battery_requires_five_fresh_samples_and_clean_retirement(self):
        rows=[f"battery: sample={i} ac_present=1 soc=86 voltage_uv=12000000 current_ua=-250000 capacity=4300000 full=5000000 unit=1 temp_mc=24950 valid=0x3f age_us=100 ac_us={i*1000} start_us={i*1000+1} end_us={i*1000+500}" for i in range(1,6)]
        tail=["battery: PASS samples=5 notifications=2 queries=1 attached=1 detached=1 heap=0 errors=0 denied=0; physical accuracy NOT_TESTED",
              "battery-capture: returned=yes clean=yes; physical accuracy NOT_TESTED"]
        text='\n'.join(rows+tail)
        result=desktop.battery_diagnostic(text)
        self.assertTrue(result['runtime_complete']);self.assertEqual(result['physical_accuracy'],'manual-pending')
        self.assertEqual(result['samples'][0]['capacity_unit'],'uAh')
        for bad in ('\n'.join(rows+tail[:1]),text+'\n'+tail[-1],text.replace('sample=5','sample=4'),
                    text.replace('age_us=100','age_us=1000001'),text.replace('soc=86','soc=101'),
                    text.replace('valid=0x3f','valid=0x01'),text.replace('heap=0','heap=1'),
                    text.replace('detached=1','detached=0'),text.replace('clean=yes','clean=NO'),
                    text+'\nbattery: HALTED test',text.replace('end_us=1500','end_us=999')):
            self.assertIsNone(desktop.battery_diagnostic(bad),bad)
        held='battery: foreign-event held enable=0x189c original=0x40'
        restored='battery: foreign-event restored enable=0x189c original=0x40 protected_writes=1'
        with_event='\n'.join([held]+rows+[restored]+tail)
        self.assertTrue(desktop.battery_diagnostic(with_event)['foreign_event']['restored'])
        for bad in (with_event.replace(restored,''),with_event.replace(held,''),
                    with_event.replace(restored,restored+'\n'+restored),
                    with_event.replace(restored,restored.replace('0x40','0x80')),
                    with_event.replace('protected_writes=1','protected_writes=0'),
                    with_event.replace(restored,'')+'\n'+restored):
            self.assertIsNone(desktop.battery_diagnostic(bad),bad)
        held2=held.replace('0x189c','0x189e').replace('0x40','0x04')
        restored2=restored.replace('0x189c','0x189e').replace('0x40','0x04')
        multiple='\n'.join([held,held2]+rows+[restored,restored2]+tail)
        self.assertEqual(len(desktop.battery_diagnostic(multiple)['foreign_events']),2)
        for bad in (multiple.replace(restored2,''),multiple.replace(held2,held),
                    multiple.replace(restored2,restored),multiple.replace(restored2,restored2.replace('0x04','0x40'))):
            self.assertIsNone(desktop.battery_diagnostic(bad),bad)
        self.assertTrue(desktop.needs_attention('battery: HALTED operation deadline'))
        self.assertTrue(desktop.needs_attention('battery: REFUSED platform/resource ownership preflight'))
        self.assertTrue(desktop.needs_attention('battery-transition: INCOMPLETE phases=1'))
        self.assertTrue(desktop.needs_attention('battery-transition-capture: returned=yes clean=NO'))

    def test_idle_measurement_requires_complete_exit_record(self):
        record="idle-capture: elapsed_us=60000000 sampled_ticks=6000 idle_ticks=5700 switches=12000 preemptions=25 completed=yes"
        self.assertEqual(desktop.idle_sample(record)["idle_sample_percent"],95)
        for bad in (record.replace("60000000","1000"),record.replace("sampled_ticks=6000","sampled_ticks=0"),
                    record.replace("idle_ticks=5700","idle_ticks=6001"),record.replace("completed=yes","completed=NO"),
                    "terminal: "+record, record+"\n"+record, ""):
            self.assertIsNone(desktop.idle_sample(bad))
        self.assertEqual(desktop.session_releases("laptop-capture: boot options=srvros.desktop-release=0123456789abcdef"),["0123456789abcdef"])

    def test_desktop_idle_requires_saved_capture_and_successful_return(self):
        line="desktop-idle: elapsed_us=60000000 sampled_ticks=6000 idle_ticks=5400 switches=12000 preemptions=25 frames=120 fb_bytes=9000 measured=yes"
        tail="\ndesktop-idle: completed=yes screenshot=yes\ndesktop-idle-capture: returned=yes clean=yes"
        record=line+tail
        self.assertEqual(desktop.desktop_idle_sample(record)["idle_sample_percent"],90)
        for bad in (line,record.replace("clean=yes","clean=NO"),record.replace("screenshot=yes","screenshot=NO"),
                    record.replace("60000000","1000"),record.replace("idle_ticks=5400","idle_ticks=6001"),
                    record+"\n"+line,record+"\ndeskd: FAIL", "terminal: "+record):
            self.assertIsNone(desktop.desktop_idle_sample(bad))

    def test_device_summary_order_and_attention(self):
        newer="workspace-devices: audio_state=4 error=3 started=2 completed=1 monotonic_us=200"
        older="workspace-devices: audio_state=2 error=0 started=1 completed=1 monotonic_us=100"
        self.assertEqual(desktop.device_summaries(newer+"\n"+older)["audio"]["monotonic_us"],200)
        self.assertTrue(desktop.needs_attention(newer))
        self.assertFalse(desktop.needs_attention(older))
        self.assertTrue(desktop.needs_attention("smc: state=4 flags=0 error=1"))
        self.assertFalse(desktop.needs_attention("smc: state=1 flags=0 error=0"))
        critical="workspace-devices: thermal_state=2 flags=0x0000000000000008 core_mc=-1000 monotonic_us=300"
        self.assertTrue(desktop.needs_attention("workspace-devices: pcm_state=6 underruns=0 monotonic_us=500"))
        self.assertTrue(desktop.needs_attention("workspace-devices: pcm_state=4 underruns=1 monotonic_us=500"))
        self.assertFalse(desktop.needs_attention("workspace-devices: pcm_state=4 underruns=0 monotonic_us=500"))
        self.assertTrue(desktop.needs_attention(critical))
        self.assertEqual(desktop.device_snapshot(critical)[1]["core_mc"],-1000)
        for bad in ("workspace-devices: audio_state=4 monotonic_us=oops","workspace-devices: audio_state=4","audio_state=4 monotonic_us=1"):
            self.assertIsNone(desktop.device_snapshot(bad))

    def test_power_samples_remain_diagnostic(self):
        good="workspace-devices: power_state=2 flags=3 package_mw=6010 active_mhz=1424 rejected=0 monotonic_us=200"
        stale=good.replace("flags=3","flags=4").replace("monotonic_us=200","monotonic_us=300")
        self.assertFalse(desktop.needs_attention(good))
        self.assertTrue(desktop.needs_attention(stale))
        self.assertTrue(desktop.needs_attention(good.replace("rejected=0","rejected=1")))
        self.assertEqual(desktop.device_summaries(stale+"\n"+good)["power"]["flags"],4)

    def test_power_uses_only_covered_idle_interval(self):
        header=("desktop-idle: elapsed_us=60000000 sampled_ticks=6000 idle_ticks=5400 switches=12000 preemptions=25 frames=1 fb_bytes=9000 measured=yes\n"
                "desktop-idle-window: start_us=10000000 end_us=70000000\n"
                "desktop-idle: completed=yes screenshot=yes\ndesktop-idle-capture: returned=yes clean=yes\n")
        def sample(at,energy,a,m,n):
            return (f"workspace-devices: power_state=2 flags=3 energy_shift=14 nominal_mhz=2200 sample_us={at} interval_us=1000000 "
                    f"samples={n} rejected=0 energy={energy} aperf={a} mperf={m} monotonic_us={at+100000}\n")
        one=sample(20000000,0xffffff00,100,100,20)
        two=sample(30000000,(0xffffff00+1638400)&0xffffffff,15000000100,10000000100,30)
        three=sample(40000000,(0xffffff00+3276800)&0xffffffff,30000000100,20000000100,40)
        text=header+one+two+three
        result=desktop.power_idle_sample(text)
        self.assertEqual((result['covered_us'],result['desktop_window_us'],result['average_package_mw'],result['active_cpu0_mhz']),(20000000,60000000,10000,3300))
        # Screenshot save is outside the measurement window; never include it.
        later=sample(80000000,0,0,0,80)
        self.assertEqual(desktop.power_idle_sample(text+later),result)
        self.assertEqual(desktop.power_idle_sample(text+two.replace('monotonic_us=30100000','monotonic_us=30200000')),result)
        for bad in (text.replace('clean=yes','clean=NO'),text.replace('end_us=70000000','end_us=69999999'),
                    text.replace('energy_shift=14','energy_shift=16'),text.replace('rejected=0','rejected=1'),
                    text.replace('power_state=2','power_state=1'),text.replace('flags=3','flags=4'),
                    text.replace('nominal_mhz=2200','nominal_mhz=0'),text.replace('sample_us=30000000','sample_us=26000000'),
                    header+one+three,header+one+two+three.replace('samples=40','samples=30'),
                    text+two.replace('energy=1638144','energy=3'),text.replace('mperf=20000000100','mperf=1'),
                    header+one+two+three.replace('energy=3276544','energy=0')):
            self.assertIsNone(desktop.power_idle_sample(bad),bad)

    def test_transition_summary_requires_all_phases_and_retirement(self):
        rows=[]
        for phase in range(3):
            g=phase*3+1;t=g*2000000
            rows.append(f'battery-transition: accepted={phase} stable=3 first_gen={g} last_gen={g+2} first_us={t} ac_us={t+4000000} start_us={t+4000001} end_us={t+4000100} relative_soc=86 voltage_uv=12000000 current_ua={-250000 if phase==1 else 0} capacity=4300000 full=5000000 design=6000000 unit=1 valid=0x1f')
        restore=['battery: foreign-event restored enable=0x189c original=0x40 protected_writes=1',
                 'battery: foreign-event restored enable=0x189e original=0x04 protected_writes=1']
        tail=['battery-transition: PASS phases=3 samples=9 notifications=10 queries=9 attached=1 detached=1 heap=0 errors=0 denied=0; retired=yes',
              'battery-transition-capture: returned=yes clean=yes; physical accuracy NOT_TESTED']
        text='\n'.join(restore+rows+tail)
        self.assertTrue(desktop.battery_transitions(text)['transition_complete'])
        for bad in (text.replace(rows[1],''),text.replace(restore[1],''),text.replace(restore[1],restore[0]),
                    text.replace('stable=3','stable=2'),text.replace('last_gen=6','last_gen=5'),
                    text.replace('current_ua=-250000','current_ua=0'),text.replace('first_us=8000000','first_us=1'),
                    text.replace('relative_soc=86','relative_soc=101'),text.replace('valid=0x1f','valid=0x0b'),
                    text.replace('samples=9','samples=8'),text.replace('clean=yes','clean=NO'),
                    text.replace('samples=9',f'samples={2**64}'),
                    text+'\n'+tail[1].replace('clean=yes','clean=NO'),
                    text.replace('detached=1','detached=0'),text+'\n'+tail[0],text+'\nbattery: HALTED test',
                    text.replace('PASS phases=3','INCOMPLETE phases=1'),text.replace(tail[1],'')):
            self.assertIsNone(desktop.battery_transitions(bad),bad)

    def test_service_requires_reader_sample_and_each_retired_lifetime(self):
        sample='battery-service-test: sample epoch=2 generation=5 flags=11 age_us=20 ac_us=1000 start_us=1001 end_us=1500 relative_soc=98 voltage_uv=8300000 current_ua=0 capacity=6100000 full=6300000 design=7150000 unit=1 valid=63'
        restore=['battery: foreign-event restored enable=0x189c original=0x40 protected_writes=1',
                 'battery: foreign-event restored enable=0x189e original=0x04 protected_writes=1']
        retired=lambda epoch,attempts:f'battery-service: retired epoch={epoch} attempts={attempts} rejected=0 queries=75 notifications=76 attached=1 detached=1 heap=0 errors=0 denied=0'
        summary='battery-service-test: PASS reads=66000 busy_reads=14000 max_read_us=100 stale=0 recovered=0 stop_timeout=1 epochs=3; retired=yes'
        finish='battery-service-capture: returned=yes clean=yes; physical accuracy NOT_TESTED'
        text='\n'.join([sample]+restore+[retired(2,5)]+restore+[retired(3,0),summary,finish])
        result=desktop.battery_service_result(text)
        proof='battery-usb-read: PASS reads=200 commands=200 busy_reads=50 progress_bursts=4 bursts=40 max_burst_us=270000 bytes_per_read=4096 writes=0; retired=yes'
        overlap=text.replace(summary,proof+'\n'+summary)
        self.assertTrue(desktop.battery_usb_read_result(overlap)['read_complete'])
        for bad in (overlap.replace('progress_bursts=4','progress_bursts=0'),
                    overlap.replace('commands=200','commands=199'),
                    overlap.replace('busy_reads=50','busy_reads=201'),
                    overlap.replace('writes=0','writes=1'),overlap+'\n'+proof,
                    overlap.replace(proof+'\n','')+'\n'+proof,
                    overlap.replace('max_burst_us=270000','max_burst_us=249999'),
                    overlap.replace('reads=200',f'reads={2**64}'),overlap.replace(finish,'')):
            self.assertIsNone(desktop.battery_usb_read_result(bad),bad)
        prepared='workspace-scratch: prepared path=/work/srvros-work/battery-io-0000000000001234.bin raw_offset=2097152 bytes=4096; unmounted=yes aliases=revoked'
        revoked='workspace-scratch: revoked clean=yes'
        write='battery-usb-write: PASS cycles=200 commands=600 busy_cycles=50 progress_bursts=4 bursts=40 max_burst_us=270000 bytes_per_cycle=4096; retired=yes revoked=yes'
        written=prepared+'\n'+text.replace(summary,revoked+'\n'+write+'\n'+summary)
        self.assertTrue(desktop.battery_usb_write_result(written)['write_complete'])
        for bad in (written.replace('commands=600','commands=599'),written.replace('progress_bursts=4','progress_bursts=0'),
                    written.replace('busy_cycles=50','busy_cycles=201'),written.replace('raw_offset=2097152','raw_offset=0'),
                    written.replace('raw_offset=2097152','raw_offset=1'),written.replace('clean=yes','clean=NO'),
                    written.replace('aliases=revoked','aliases=enabled'),written.replace(revoked+'\n',''),
                    written+'\n'+prepared,written+'\n'+write,written.replace(finish,''),
                    written.replace('cycles=200',f'cycles={2**64}'),written.replace(prepared+'\n','')+'\n'+prepared):
            self.assertIsNone(desktop.battery_usb_write_result(bad),bad)
        image_prepared='workspace-image: prepared path=/work/srvros-work/battery-fs.img raw_offset=2097152 bytes=16777216; unmounted=yes aliases=revoked'
        image_revoked='workspace-image: revoked clean=yes'
        image_result='battery-filesystem: PASS cycles=200 commands=600 busy_cycles=50 progress_bursts=4 bursts=40 max_burst_us=270000 elapsed_us=60000000; retired=yes revoked=yes'
        image_text=image_prepared+'\n'+text.replace(summary,image_revoked+'\n'+image_result+'\n'+summary)
        self.assertTrue(desktop.battery_filesystem_result(image_text)['isolated_complete'])
        for bad in (image_text.replace('elapsed_us=60000000','elapsed_us=59999999'),
                    image_text.replace('commands=600','commands=599'),image_text.replace('cycles=200',f'cycles={2**64}'),
                    image_text.replace('progress_bursts=4','progress_bursts=0'),image_text.replace('busy_cycles=50','busy_cycles=201'),
                    image_text.replace(image_revoked,'workspace-image: revoked clean=NO'),image_text.replace(finish,''),
                    image_text+'\n'+image_result,image_text.replace(image_prepared+'\n','')+'\n'+image_prepared):
            self.assertIsNone(desktop.battery_filesystem_result(bad),bad)
        stepped=image_text.replace('bursts=40 max_burst_us=270000 elapsed_us=60000000',
            'bursts=3600 max_burst_us=5000 elapsed_us=60000000 steps=3600 max_step_us=4900 slowest_step=5')
        self.assertEqual(desktop.battery_filesystem_result(stepped)['steps'],3600)
        for bad in (stepped.replace('steps=3600','steps=3599'),
                    stepped.replace('bursts=3600','bursts=3599'),
                    stepped.replace('max_step_us=4900','max_step_us=5001'),
                    stepped.replace('slowest_step=5','slowest_step=18'),
                    stepped.replace('steps=3600',f'steps={2**64}'),
                    stepped.replace(' steps=3600 max_step_us=4900 slowest_step=5','')):
            self.assertIsNone(desktop.battery_filesystem_result(bad),bad)
        profile_header='battery-filesystem-profile: cycle=7 step=5 elapsed_us=4800 io_us=60'
        phase=lambda n,active: (f'battery-filesystem-phase: phase={n} reads={2 if active else 0} '
            f'read_bytes={1024 if active else 0} read_us={10 if active else 0} read_max_us={5 if active else 0} '
            f'writes={2 if active else 0} write_bytes={8192 if active else 0} '
            f'write_us={20 if active else 0} write_max_us={10 if active else 0} '
            f'flushes={1 if active else 0} flush_us={30 if active else 0} flush_max_us={30 if active else 0}')
        phases=[phase(n,n==2) for n in range(4)]
        detail='\n'.join([profile_header]+phases)
        profiled=stepped.replace('slowest_step=5;', 'slowest_step=5 profile=1;').replace(summary,detail+'\n'+summary)
        profile=desktop.battery_filesystem_result(profiled)['profile']
        self.assertEqual(profile['other_us'],4740)
        self.assertEqual(profile['phases']['data']['write_bytes'],8192)
        for bad in (profiled.replace(profile_header+'\n',''),profiled.replace(phases[1]+'\n',''),
                    profiled.replace('phase=3','phase=2'),profiled.replace('cycle=7','cycle=200'),
                    profiled.replace('step=5 elapsed','step=4 elapsed'),
                    profiled.replace('elapsed_us=4800','elapsed_us=4901'),
                    profiled.replace('io_us=60','io_us=59'),
                    profiled.replace('read_us=10','read_us=11'),
                    profiled.replace('read_max_us=5','read_max_us=11'),
                    profiled.replace('read_bytes=1024','read_bytes=512'),
                    profiled.replace('read_bytes=1024','read_bytes=1025'),
                    profiled.replace('reads=2 read_bytes=1024','reads=0 read_bytes=1024'),
                    profiled.replace('write_bytes=8192',f'write_bytes={2**64}'),
                    profiled.replace('cycle=7',f'cycle={2**64}'),
                    profiled.replace(' profile=1',''),profiled+'\n'+profile_header,
                    profiled.replace(detail+'\n','')+'\n'+detail):
            self.assertIsNone(desktop.battery_filesystem_result(bad),bad)
        with tempfile.TemporaryDirectory() as temp:
            workspace=Path(temp);proof=desktop.battery_usb_write_result(written)
            scratch_path=workspace/'battery-io-0000000000001234.bin'
            with self.assertRaises(ValueError):desktop.verify_battery_scratch(workspace,proof)
            scratch_path.write_bytes(bytes((i+199)&255 for i in range(4096)))
            self.assertEqual(len(desktop.verify_battery_scratch(workspace,proof)),4096)
            scratch_path.write_bytes(bytes(4096))
            with self.assertRaises(ValueError):desktop.verify_battery_scratch(workspace,proof)
            scratch_path.write_bytes(b'bad')
            with self.assertRaises(ValueError):desktop.verify_battery_scratch(workspace,proof)
            scratch_path.unlink();scratch_path.symlink_to(workspace/'other')
            with self.assertRaises(ValueError):desktop.verify_battery_scratch(workspace,proof)
        self.assertTrue(result['reader_complete']);self.assertTrue(result['stop_wait_timed_out'])
        self.assertEqual(result['sample']['generation'],5)
        self.assertTrue(desktop.battery_service_result('\n'.join(restore+[retired(1,0)])+'\n'+text))
        for bad in (text.replace(sample,''),text.replace(restore[1],'',1),
                    text.replace(retired(3,0),''),text.replace('heap=0','heap=1'),
                    text.replace('detached=1','detached=0'),text.replace('rejected=0','rejected=1'),
                    text.replace('reads=66000','reads=999'),text.replace('busy_reads=14000','busy_reads=0'),
                    text.replace('flags=11','flags=13'),text.replace('age_us=20','age_us=6000001'),
                    text.replace('epoch=3','epoch=4'),text.replace('attempts=5','attempts=4'),
                    text.replace('end_us=1500','end_us=999'),text.replace('valid=63','valid=7'),
                    text.replace('relative_soc=98','relative_soc=101'),text.replace('unit=1','unit=0'),
                    text.replace('full=6300000','full=0'),text.replace('retired=yes','retired=NO'),
                    text.replace('protected_writes=1','protected_writes=0'),
                    text.replace(sample,'')+'\n'+sample,text+'\n'+summary,text.replace(finish,''),
                    text+'\nbattery: HALTED live state retained',
                    text+'\n'+finish.replace('clean=yes','clean=NO'),
                    text.replace('reads=66000',f'reads={2**64}')):
            self.assertIsNone(desktop.battery_service_result(bad),bad)
        self.assertTrue(desktop.needs_attention('battery-service-test: FAIL sample deadline'))
        self.assertTrue(desktop.needs_attention('battery-service-capture: returned=yes clean=NO'))

    def test_text_capture_collection_requires_complete_exit_proof(self):
        from test_battery_desktop_records import TEXT
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);mount=root/'usb';work=mount/'srvros-work'
            folder=work/'Logs/session-0000000000000001-00';folder.mkdir(parents=True)
            (work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
            boot=b'laptop-capture: boot options=srvros.desktop-release=0123456789abcdef srvros.capture=battery-text\n'
            tail=b'battery-text-capture: returned=yes clean=yes; physical accuracy NOT_TESTED\n'
            (folder/'boot.log').write_bytes(boot)
            for payload,valid in ((boot+TEXT+tail,True),(boot+TEXT,False),
                                  (boot+TEXT.replace(b'hex=4141',b'hex=4142')+tail,False),
                                  (boot+tail,False)):
                (folder/'exit.log').write_bytes(payload)
                _,report=desktop.collect(mount,'37d8c5d0',root/'out')
                item=report['sessions'][0]
                self.assertEqual(item['battery_text'] is not None,valid)
                self.assertIsNone(item['battery_desktop'])
                self.assertEqual(bool(item['attention']),not valid)
                self.assertEqual(report['hardware_acceptance'],'manual-pending')

    def test_loaded_text_requires_saved_image_verification(self):
        from test_battery_desktop_records import LOAD
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);mount=root/'usb';work=mount/'srvros-work'
            folder=work/'Logs/session-0000000000000001-00';folder.mkdir(parents=True)
            (work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
            boot=b'laptop-capture: boot options=srvros.desktop-release=0123456789abcdef srvros.capture=battery-load\n'
            tail=b'battery-load-capture: returned=yes clean=yes; physical accuracy NOT_TESTED\n'
            (folder/'boot.log').write_bytes(boot);(folder/'exit.log').write_bytes(boot+LOAD+tail)
            with patch.object(desktop,'verify_battery_image',return_value='verified-fixture-hash') as verify:
                _,report=desktop.collect(mount,'37d8c5d0',root/'out');item=report['sessions'][0]
                self.assertTrue(item['battery_load']['image_verified']);self.assertFalse(item['attention'])
                self.assertEqual(verify.call_args.args[1]['cycles'],10)
                self.assertIsNone(item['battery_text']);self.assertIsNone(item['battery_desktop'])
            with patch.object(desktop,'verify_battery_image',side_effect=ValueError('wrong saved image')):
                _,report=desktop.collect(mount,'37d8c5d0',root/'out');item=report['sessions'][0]
                self.assertIsNone(item['battery_load']);self.assertIn('collection: wrong saved image',item['attention'])
            (folder/'exit.log').write_bytes(boot+tail)
            _,report=desktop.collect(mount,'37d8c5d0',root/'out')
            self.assertIn('collection: Text/filesystem overlap completion missing or invalid',report['sessions'][0]['attention'])

    def test_boot_only_session_is_incomplete(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);mount=root/'usb';work=mount/'srvros-work'
            folder=work/'Logs/session-0000000000000001-00';folder.mkdir(parents=True)
            (work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
            (folder/'boot.log').write_text('laptop-capture: boot options=srvros.desktop-release=6a2f90f72a0e4b96 srvros.capture=battery\n')
            _,report=desktop.collect(mount,'37d8c5d0',root/'out');item=report['sessions'][0]
            self.assertFalse(item['exit_snapshot_present'])
            self.assertIsNone(item['battery_diagnostic'])
            self.assertIn('collection: missing exit snapshot; session incomplete',item['attention'])
            self.assertEqual(report['hardware_acceptance'],'manual-pending')

    def test_logs_and_unsafe_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);mount=root/'usb';work=mount/'srvros-work';folder=work/'Logs/session-0000000000000001-00'
            folder.mkdir(parents=True);(work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
            (folder/'boot.log').write_text('laptop-probe: unsupported chipset\n')
            (folder/'session-a.log').write_text('deskd: closing pid=3 reason=input-timeout\n')
            (folder/'exit.log').write_text('backlight: provider=ready initial_percent=14\ndeskd: brightness requested=50 result=-1\ndeskd: exited frames=20\n')
            out,report=desktop.collect(mount,'37d8c5d0',root/'out');item=report['sessions'][0]
            self.assertTrue(item['exit_snapshot_present'] and item['desktop_exit_seen'])
            self.assertEqual(len(item['brightness']),2);self.assertEqual(len(item['attention']),2);self.assertEqual(len(item['probe']),1)
            self.assertEqual((out/item['session']/'exit.log').read_bytes(),(folder/'exit.log').read_bytes())
            self.assertEqual(report['hardware_acceptance'],'manual-pending')
            self.assertEqual(item['releases'],[])
            (folder/'boot.log').write_text('cmdline: srvros.desktop-release=0620ce5e026b2f32\n')
            (folder/'exit.log').write_text('cmdline: srvros.desktop-release=d61e60a82a295d84\n')
            _,mixed=desktop.collect(mount,'37d8c5d0',root/'out')
            self.assertEqual(mixed['sessions'][0]['releases'],['0620ce5e026b2f32','d61e60a82a295d84'])
            self.assertIn('collection: conflicting boot release identities',mixed['sessions'][0]['attention'])
            (folder/'exit.log').unlink();(folder/'exit.log').symlink_to(folder/'boot.log')
            with self.assertRaises(ValueError): desktop.collect(mount,'37d8c5d0',root/'out')
            (folder/'exit.log').unlink();(folder/'exit.log').write_bytes(b'x'*65537)
            with self.assertRaises(ValueError): desktop.collect(mount,'37d8c5d0',root/'out')
            with self.assertRaises(ValueError): desktop.collect(mount,'12345678',root/'out')

if __name__=='__main__': unittest.main()
