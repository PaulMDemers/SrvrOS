#!/usr/bin/env python3
"""Check installer rollback/retention without root or the host EFI filesystem."""
import contextlib
import io
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import hardware_boot as boot


class InstallTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.stage = self.root / "stage"
        self.esp = self.root / "efi"
        self.base = self.esp / "EFI/srvros"
        self.snippet = self.root / "42_srvros"
        self.cfg = self.root / "grub.cfg"
        self.stage.mkdir()
        self.esp.mkdir()
        self.cfg.write_text("original Ubuntu config\n")
        self.fail_update = False
        self.stack = contextlib.ExitStack()
        self.addCleanup(self.stack.close)
        for name, value in {"STAGE": self.stage, "ESP": self.esp, "BASE": self.base,
                            "SNIPPET": self.snippet, "GRUB_CFG": self.cfg}.items():
            self.stack.enter_context(patch.object(boot, name, value))
        self.stack.enter_context(patch.object(boot, "host_uuid", return_value="7000-79A2"))
        self.stack.enter_context(patch.object(boot.os, "geteuid", return_value=0))
        self.stack.enter_context(patch.object(boot.os, "sync"))
        self.stack.enter_context(patch.object(boot.shutil, "which", return_value="/mock/program"))
        self.stack.enter_context(patch.object(boot.subprocess, "run", side_effect=self.run_command))
        self.stack.enter_context(contextlib.redirect_stdout(io.StringIO()))

    def run_command(self, command, **kwargs):
        if command[0] == "update-grub":
            self.cfg.write_text("generated config\n")
            if self.fail_update:
                raise subprocess.CalledProcessError(1, command)
        return subprocess.CompletedProcess(command, 0)

    def payload(self, version, config_version=boot.CONFIG_VERSION):
        hashes = {name: hashlib.sha256((name + version).encode()).hexdigest()
                  for name in boot.FILES if not name.endswith("limine.conf")}
        release = boot.release_id(hashes, config_version)
        directory = self.stage / release
        directory.mkdir()
        for name in boot.FILES:
            path = directory / name
            path.parent.mkdir(exist_ok=True)
            path.write_text(boot.limine_config(release, config_version) if name.endswith("limine.conf") else name + version)
            hashes[name] = boot.digest(path)
        (directory / "manifest.json").write_text(json.dumps({"release": release, "sha256": hashes}))
        (self.stage / "latest").write_text(release)
        return release

    def test_native_desktop_profile(self):
        version="ram-usb-v17-desktop-37d8c5d0"
        release=self.payload("desktop", version)
        path=self.stage/release
        manifest=json.loads((path/"manifest.json").read_text());manifest["config_version"]=version
        (path/"manifest.json").write_text(json.dumps(manifest))
        self.assertEqual(boot.verify(path)["release"],release)
        config=(path/"limine.conf").read_text()
        for option in ("srvros.storage=ram","srvros.workspace-volume=37d8c5d0","srvros.trackpad=gestures","srvros.desktop=trial"):
            self.assertIn(option,config)
        self.assertNotIn("srvros.autotest",config)
        self.assertNotIn("srvros.trackpad-run",config)
        boot.install(release)
        self.assertEqual(json.loads((self.base/"state.json").read_text())["current"],release)

    def test_desktop_logging_profile(self):
        cfg=boot.limine_config("0123456789abcdef","ram-usb-v18-desktop-37d8c5d0")
        self.assertIn("srvros.desktop-release=0123456789abcdef",cfg)
        self.assertIn("srvros.laptop-services=probe",cfg)
        self.assertNotIn("laptop-services",boot.limine_config("0123456789abcdef","ram-usb-v17-desktop-37d8c5d0"))

    def test_audio_profile(self):
        version="ram-usb-v21-desktop-37d8c5d0"
        cfg=boot.limine_config("0123456789abcdef",version)
        self.assertIn('srvros.audio=hda',cfg)
        for flag in ('srvros.aml=probe','srvros.usb-network=ecm','srvros.sensors=cpu','srvros.smc=read-only'):self.assertIn(flag,cfg)
        self.assertIn('srvros.laptop-services=brightness',cfg)
        self.assertIn('srvros.desktop-release=0123456789abcdef',cfg)
        self.assertIsNotNone(boot.DESKTOP_RE.fullmatch(version))
        self.assertNotIn('srvros.audio=',boot.limine_config("0123456789abcdef","ram-usb-v20-desktop-37d8c5d0"))

    def test_session_workload_verifier(self):
        from desktop_session_workload import files, verify, SEED, ROUNDS
        run='0123456789abcdef'
        contents=files(run)
        prefix='session-'+run+'/'
        contents.update({prefix+'payload.bin':SEED,
                         prefix+'rounds.txt':''.join(f'{n}\n' for n in range(1,ROUNDS+1)).encode(),
                         prefix+'complete.txt':f'COMPLETE {run} {ROUNDS}\n'.encode()})
        self.assertEqual(verify(contents.__getitem__,run)['rounds'],60)
        for key in tuple(contents):
            saved=contents[key];contents[key]=b'incomplete'
            with self.assertRaises(ValueError): verify(contents.__getitem__,run)
            contents[key]=saved
        for bad in ('../bad','',run+'0'):
            with self.assertRaises(ValueError): files(bad)

    def test_basic_desktop_profile(self):
        cfg=boot.limine_config("0123456789abcdef","ram-usb-v53-desktop-6ada85bb")
        self.assertIn('srvros.desktop-release=0123456789abcdef',cfg)
        self.assertIn('srvros.desktop=trial',cfg)
        self.assertIn('srvros.trackpad=gestures',cfg)
        for option in ('laptop-services','aml','audio','smc','power','capture','sensors','usb-network'):
            self.assertNotIn('srvros.'+option+'=',cfg)
        for kwargs in ({}, {'desktop_scale':2}, {'desktop_audio':True}):
            with self.assertRaises(ValueError):
                boot.stage(desktop_basic=True, **kwargs)

    def test_basic_brightness_profile(self):
        version='ram-usb-v56-desktop-6ada85bb'
        self.assertIsNotNone(boot.DESKTOP_RE.fullmatch(version))
        cfg=boot.limine_config('0123456789abcdef',version)
        for token in ('srvros.laptop-services=brightness','srvros.desktop=trial','srvros.trackpad=gestures','srvros.desktop-release=0123456789abcdef'):
            self.assertIn(token,cfg)
        for option in ('aml','audio','smc','power','capture','sensors','usb-network','battery','desktop-scale'):
            self.assertNotIn('srvros.'+option+'=',cfg)
        for kwargs in ({'desktop_scale':2},{'desktop_audio':True},{'desktop_battery':True}):
            with self.assertRaises(ValueError):
                boot.stage(desktop_volume='6ada85bb',desktop_basic=True,desktop_brightness=True,**kwargs)

        for name in ('limine-binary/BOOTX64.EFI','kernel/srvros.elf','initramfs.tar'):
            path=self.root/'build'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(name)
        with patch.object(boot,'ROOT',self.root), patch.object(boot.subprocess,'check_output',side_effect=['test-revision\n','']):
            release=boot.stage(desktop_volume='6ada85bb',desktop_basic=True,desktop_brightness=True)
        self.assertEqual(boot.verify(self.stage/release)['config_version'],version)
        self.assertEqual((self.stage/release/'srvros.elf').read_text(),'kernel/srvros.elf')

    def test_battery_desktop_profile(self):
        version="ram-usb-v54-desktop-37d8c5d0"
        cfg=boot.limine_config("0123456789abcdef",version)
        self.assertIsNotNone(boot.DESKTOP_RE.fullmatch(version))
        self.assertIn('srvros.desktop=session',cfg)
        self.assertIn('srvros.battery=on',cfg)
        self.assertNotIn('srvros.desktop=trial',cfg)
        for option in ('laptop-services','aml','audio','smc','power','capture','sensors','usb-network'):
            self.assertNotIn('srvros.'+option+'=',cfg)
        for kwargs in ({}, {'desktop_volume':'37d8c5d0','desktop_basic':True},
                       {'desktop_volume':'37d8c5d0','desktop_scale':2},
                       {'desktop_volume':'37d8c5d0','desktop_audio':True}):
            with self.assertRaises(ValueError):boot.stage(desktop_battery=True, **kwargs)
        for name in ('limine-binary/BOOTX64.EFI','acpi-battery/srvros.elf','initramfs.tar'):
            path=self.root/'build'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(name)
        with patch.object(boot,'ROOT',self.root), patch.object(boot.subprocess,'check_output',side_effect=['test-revision\n','']):
            release=boot.stage(desktop_volume='37d8c5d0',desktop_battery=True)
        manifest=boot.verify(self.stage/release)
        self.assertEqual(manifest['config_version'],version)
        self.assertEqual((self.stage/release/'srvros.elf').read_text(),'acpi-battery/srvros.elf')

    def test_brightness_profile(self):
        cfg=boot.limine_config("0123456789abcdef","ram-usb-v20-desktop-37d8c5d0")
        self.assertIn('srvros.laptop-services=brightness',cfg)
        self.assertIn('srvros.desktop=trial',cfg)
        self.assertNotIn('probe-autoreboot',cfg)

    def test_desktop_excludes_optional_aml_probe(self):
        release="0123456789abcdef"
        old=boot.limine_config(release,"ram-usb-v21-desktop-37d8c5d0")
        new=boot.limine_config(release,"ram-usb-v22-desktop-37d8c5d0")
        self.assertEqual(new,old.replace(" srvros.aml=probe",""))
        for name in ('limine-binary/BOOTX64.EFI','kernel/srvros.elf','initramfs.tar'):
            path=self.root/'build'/name
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_bytes(b'test desktop payload')
        with patch.object(boot,'ROOT',self.root), patch.object(boot.subprocess,'check_output',side_effect=['test-revision\n','']):
            staged=boot.stage(desktop_volume="37d8c5d0")
        manifest=boot.verify(self.stage/staged)
        self.assertEqual(manifest['config_version'],'ram-usb-v22-desktop-37d8c5d0')
        self.assertNotIn('srvros.aml=',(self.stage/staged/'limine.conf').read_text())

    def test_automatic_laptop_probe_profile(self):
        version="ram-usb-v19-laptop-probe-37d8c5d0"
        release=self.payload("probe",version)
        path=self.stage/release/'manifest.json'
        m=json.loads(path.read_text());m['config_version']=version;path.write_text(json.dumps(m))
        boot.install(release)
        cfg=boot.limine_config(release,version)
        self.assertIn('srvros.laptop-services=probe-autoreboot',cfg)
        self.assertIn('srvros.workspace-volume=37d8c5d0',cfg)
        self.assertNotIn('srvros.desktop=trial',cfg)
        for options in ({'diagnostic':True},{'desktop_volume':'37d8c5d0'},{'workspace_volume':'37d8c5d0'}):
            with self.assertRaises(ValueError): boot.stage(laptop_probe_volume='37d8c5d0',**options)
        with self.assertRaises(ValueError): boot.stage(laptop_probe_volume='bad')

    def test_cli_accepts_standard_exfat_uuid(self):
        self.assertEqual(boot.compact_volume_uuid("37D8-C5D0"),"37d8c5d0")
        self.assertEqual(boot.compact_volume_uuid("37d8c5d0"),"37d8c5d0")
        for bad in ("-37d8c5d0","37d8--c5d0","37d8c5d00","not-a-uuid"):
            with self.assertRaises(boot.argparse.ArgumentTypeError): boot.compact_volume_uuid(bad)

    def test_desktop_idle_profile_is_separate(self):
        version="ram-usb-v25-laptop-probe-37d8c5d0"
        self.assertIsNotNone(boot.LAPTOP_PROBE_RE.fullmatch(version))
        config=boot.limine_config("0123456789abcdef",version)
        self.assertIn("srvros.capture=desktop-idle",config)
        self.assertIn("srvros.smc=read-only",config)
        self.assertNotIn("srvros.aml=probe",config)
        self.assertNotIn("srvros.desktop=trial",config)
        with self.assertRaises(ValueError): boot.stage(desktop_idle_capture=True)
        with self.assertRaises(ValueError): boot.stage(laptop_probe_volume='37d8c5d0',desktop_idle_capture=True,idle_capture=True)

    def test_power_telemetry_requires_explicit_desktop_idle(self):
        version='ram-usb-v26-laptop-probe-37d8c5d0'
        release=self.payload('power',version)
        path=self.stage/release/'manifest.json'
        manifest=json.loads(path.read_text());manifest['config_version']=version;path.write_text(json.dumps(manifest))
        self.assertEqual(boot.verify(path.parent)['release'],release)
        config=boot.limine_config(release,version)
        for flag in ('srvros.capture=desktop-idle','srvros.power=read-only','srvros.smc=read-only'):self.assertIn(flag,config)
        self.assertNotIn('srvros.aml=',config)
        for v in (23,24,25):self.assertNotIn('srvros.power=',boot.limine_config(release,f'ram-usb-v{v}-laptop-probe-37d8c5d0'))
        for options in ({},{'idle_capture':True},{'aml_capture':True},{'desktop_volume':'37d8c5d0'}):
            with self.assertRaises(ValueError):boot.stage(power_telemetry=True,**options)
        with self.assertRaises(ValueError):boot.stage(power_telemetry=True,desktop_idle_capture=True)

    def test_network_sensor_desktop_profile(self):
        cfg=boot.limine_config('0123456789abcdef','ram-usb-v37-desktop-0e535900')
        for flag in ('resolution: 2880x1800','srvros.desktop-scale=2','srvros.usb-network=ecm','srvros.smc=read-only','srvros.audio=hda'):
            self.assertIn(flag,cfg)
        self.assertNotIn('srvros.aml',cfg)
        self.assertNotIn('srvros.usb-network',boot.limine_config('0123456789abcdef','ram-usb-v36-desktop-0e535900'))
        with self.assertRaises(ValueError): boot.stage(desktop_network=True)

    def test_scaled_desktop_profile(self):
        cfg=boot.limine_config("0123456789abcdef","ram-usb-v29-desktop-37d8c5d0")
        self.assertIn("srvros.desktop-scale=2",cfg)
        self.assertIn("resolution: 2880x1800",cfg)
        self.assertIn("srvros.trackpad=gestures",cfg)
        self.assertNotIn("srvros.aml",cfg)
        with self.assertRaises(ValueError): boot.stage(desktop_scale=2)
        with self.assertRaises(ValueError): boot.stage(desktop_scale=3,desktop_volume="37d8c5d0")

    def test_scaled_audio_profile(self):
        cfg=boot.limine_config("0123456789abcdef","ram-usb-v31-desktop-37d8c5d0")
        for token in ("srvros.audio=hda","srvros.desktop-scale=2","srvros.laptop-services=brightness","resolution: 2880x1800"):
            self.assertIn(token,cfg)
        for token in ("srvros.aml=","srvros.smc=","srvros.usb-network="):
            self.assertNotIn(token,cfg)
        with self.assertRaises(ValueError): boot.stage(desktop_audio=True)

    def test_scaled_brightness_profile(self):
        cfg=boot.limine_config("0123456789abcdef","ram-usb-v30-desktop-37d8c5d0")
        self.assertIn("srvros.desktop-scale=2",cfg)
        self.assertIn("srvros.laptop-services=brightness",cfg)
        self.assertIn("resolution: 2880x1800",cfg)
        for forbidden in ("srvros.audio=", "srvros.aml=", "srvros.smc="):
            self.assertNotIn(forbidden,cfg)
        with self.assertRaises(ValueError): boot.stage(desktop_brightness=True)
        with self.assertRaises(ValueError): boot.stage(desktop_volume="37d8c5d0",desktop_brightness=True)

    def test_keyboard_capture_profile(self):
        version="ram-usb-v28-laptop-probe-37d8c5d0"
        self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(version))
        cfg=boot.limine_config("0123456789abcdef",version)
        self.assertIn("srvros.capture=keyboard",cfg)
        self.assertIn("srvros.laptop-services=probe-autoreboot",cfg)
        self.assertNotIn("srvros.aml",cfg)
        self.assertNotIn("srvros.capture=keyboard",boot.limine_config("0123456789abcdef","ram-usb-v19-laptop-probe-37d8c5d0"))
        with self.assertRaises(ValueError): boot.stage(keyboard_capture=True)
        with self.assertRaises(ValueError): boot.stage(keyboard_capture=True,idle_capture=True,laptop_probe_volume="37d8c5d0")

    def test_idle_capture_profile_is_separate_and_has_no_aml(self):
        version="ram-usb-v23-laptop-probe-37d8c5d0"
        release=self.payload("idle",version)
        path=self.stage/release/"manifest.json"
        manifest=json.loads(path.read_text());manifest["config_version"]=version
        path.write_text(json.dumps(manifest))
        boot.install(release)
        cfg=boot.limine_config(release,version)
        for flag in ('srvros.capture=idle','srvros.sensors=cpu','srvros.smc=read-only','srvros.audio=hda'):
            self.assertIn(flag,cfg)
        self.assertNotIn('srvros.aml=probe',cfg)
        self.assertNotIn('srvros.desktop=trial',cfg)
        self.assertNotIn('srvros.capture=idle',boot.limine_config(release,'ram-usb-v19-laptop-probe-37d8c5d0'))
        with self.assertRaises(ValueError): boot.stage(idle_capture=True)
        with self.assertRaises(ValueError): boot.stage(idle_capture=True,laptop_probe_volume='37d8c5d0',desktop_volume='37d8c5d0')

    def test_native_battery_is_explicit_separate_kernel(self):
        version = "ram-usb-v31-laptop-probe-37d8c5d0"
        release = self.payload("battery", version)
        cfg = boot.limine_config(release, version)
        self.assertIn('srvros.capture=battery', cfg)
        for flag in ('srvros.aml=', 'srvros.desktop=trial', 'srvros.laptop-services=', 'srvros.smc='):
            self.assertNotIn(flag, cfg)
        path = self.stage/release/"manifest.json"
        manifest = json.loads(path.read_text()); manifest["config_version"] = version
        path.write_text(json.dumps(manifest))
        boot.verify(self.stage/release)
        with self.assertRaises(ValueError): boot.stage(battery_capture=True)
        for other in ('idle_capture', 'aml_capture', 'desktop_idle_capture'):
            with self.assertRaises(ValueError):
                boot.stage(battery_capture=True, laptop_probe_volume='37d8c5d0', **{other:True})
        # Real staging with temporary payloads proves the special profile cannot
        # silently take the ordinary kernel (or the synthetic VM binding).
        for name, data in {'build/limine-binary/BOOTX64.EFI':b'loader',
                           'build/kernel/srvros.elf':b'normal',
                           'build/acpi-battery/srvros.elf':b'native',
                           'build/acpi-battery/vm.elf':b'model',
                           'build/initramfs.tar':b'initramfs'}.items():
            path = self.root/name; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root), patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged = boot.stage(battery_capture=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertTrue(boot.verify(self.stage/staged)['config_version'].endswith('-auto-return'))
        self.assertIn('srvros.capture-failure=reboot',(self.stage/staged/'limine.conf').read_text())

    def test_unattended_recipe_keeps_existing_and_guided_profiles(self):
        old='ram-usb-v33-laptop-probe-37d8c5d0'
        self.assertNotIn('srvros.capture-failure',boot.limine_config('0'*16,old))
        new=old+'-auto-return'
        release=self.payload('unattended',new)
        path=self.stage/release/'manifest.json'
        manifest=json.loads(path.read_text());manifest['config_version']=new
        path.write_text(json.dumps(manifest))
        self.assertEqual(boot.verify(path.parent)['config_version'],new)
        self.assertIn('srvros.capture-failure=reboot',boot.limine_config(release,new))
        self.assertNotIn('srvros.capture-failure',boot.limine_config('0'*16,'ram-usb-v32-laptop-probe-37d8c5d0'))
        with self.assertRaises(ValueError):boot.stage(hold_on_failure=True)

    def test_battery_transitions_preserve_v31_recipe(self):
        v31='ram-usb-v31-laptop-probe-37d8c5d0';v32='ram-usb-v32-laptop-probe-37d8c5d0'
        a=self.payload('battery',v31);b=self.payload('battery',v32)
        for release,version in ((a,v31),(b,v32)):
            path=self.stage/release/'manifest.json'
            manifest=json.loads(path.read_text());manifest['config_version']=version
            path.write_text(json.dumps(manifest))
        self.assertNotEqual(a,b)
        self.assertIn('srvros.capture=battery-transitions',boot.limine_config(b,v32))
        self.assertNotIn('battery-transitions',boot.limine_config(a,v31))
        boot.verify(self.stage/a);boot.verify(self.stage/b)
        with self.assertRaises(ValueError):boot.stage(battery_transitions=True)
        for other in ('battery_capture','idle_capture','aml_capture','desktop_idle_capture'):
            with self.assertRaises(ValueError):boot.stage(laptop_probe_volume='37d8c5d0',battery_transitions=True,**{other:True})
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/acpi-battery/vm.elf':b'model','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_transitions=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')

    def test_battery_service_is_a_separate_native_recipe(self):
        version='ram-usb-v33-laptop-probe-37d8c5d0'
        release=self.payload('service',version);path=self.stage/release/'manifest.json'
        manifest=json.loads(path.read_text());manifest['config_version']=version
        path.write_text(json.dumps(manifest));boot.verify(path.parent)
        self.assertIn('srvros.capture=battery-service',boot.limine_config(release,version))
        for old in ('31','32'):
            self.assertNotIn('battery-service',boot.limine_config(release,f'ram-usb-v{old}-laptop-probe-37d8c5d0'))
        with self.assertRaises(ValueError):boot.stage(battery_service=True)
        for other in ('battery_capture','battery_transitions','idle_capture','aml_capture','desktop_idle_capture'):
            with self.assertRaises(ValueError):boot.stage(laptop_probe_volume='37d8c5d0',battery_service=True,**{other:True})
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/acpi-battery/vm.elf':b'model','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_service=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')

    def test_battery_usb_read_is_automatic_and_native(self):
        version='ram-usb-v34-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-usb-read',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_usb_read=True)
        with self.assertRaises(ValueError):boot.stage(battery_usb_read=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_usb_read=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_usb_read=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_battery_usb_write_is_automatic_and_native(self):
        version='ram-usb-v36-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-usb-write',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_usb_write=True)
        with self.assertRaises(ValueError):boot.stage(battery_usb_write=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_usb_write=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_usb_write=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_battery_filesystem_is_automatic_and_native(self):
        version='ram-usb-v38-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-filesystem',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_filesystem=True)
        with self.assertRaises(ValueError):boot.stage(battery_filesystem=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_filesystem=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_filesystem=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_battery_desktop_is_automatic_and_native(self):
        version='ram-usb-v40-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-desktop',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_desktop=True)
        with self.assertRaises(ValueError):boot.stage(battery_desktop=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_desktop=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_desktop=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_battery_text_is_automatic_and_native(self):
        version='ram-usb-v41-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-text',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_text=True)
        with self.assertRaises(ValueError):boot.stage(battery_text=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_text=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        with self.assertRaises(ValueError):boot.stage(battery_text=True,laptop_probe_volume='37d8c5d0',battery_desktop=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_text=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_battery_load_is_automatic_and_native(self):
        version='ram-usb-v42-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-load',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_load=True)
        with self.assertRaises(ValueError):boot.stage(battery_load=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_load=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        with self.assertRaises(ValueError):boot.stage(battery_load=True,laptop_probe_volume='37d8c5d0',battery_desktop=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_load=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_battery_app_files_is_automatic_and_native(self):
        version='ram-usb-v43-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-app-files',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_app_files=True)
        with self.assertRaises(ValueError):boot.stage(battery_app_files=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_app_files=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        with self.assertRaises(ValueError):boot.stage(battery_app_files=True,laptop_probe_volume='37d8c5d0',battery_desktop=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_app_files=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_battery_home_desktop_is_automatic_and_native(self):
        version='ram-usb-v51-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-home-desktop',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_home_desktop=True)
        with self.assertRaises(ValueError):boot.stage(battery_home_desktop=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_home_desktop=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        with self.assertRaises(ValueError):boot.stage(battery_home_desktop=True,laptop_probe_volume='37d8c5d0',battery_desktop=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_home_desktop=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_battery_home_transitions_is_automatic_and_native(self):
        version='ram-usb-v52-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-home-transitions',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_home_transitions=True)
        with self.assertRaises(ValueError):boot.stage(battery_home_transitions=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_home_transitions=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        with self.assertRaises(ValueError):boot.stage(battery_home_transitions=True,laptop_probe_volume='37d8c5d0',battery_desktop=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_home_transitions=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_battery_home_session_is_automatic_and_native(self):
        version='ram-usb-v49-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-home-session',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_home_session=True)
        with self.assertRaises(ValueError):boot.stage(battery_home_session=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_home_session=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        with self.assertRaises(ValueError):boot.stage(battery_home_session=True,laptop_probe_volume='37d8c5d0',battery_desktop=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_home_session=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_battery_home_is_automatic_and_native(self):
        version='ram-usb-v48-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-home',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_home=True)
        with self.assertRaises(ValueError):boot.stage(battery_home=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_home=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        with self.assertRaises(ValueError):boot.stage(battery_home=True,laptop_probe_volume='37d8c5d0',battery_desktop=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_home=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_battery_smbus_files_is_automatic_and_native(self):
        version='ram-usb-v47-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-smbus-files',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_smbus_files=True)
        with self.assertRaises(ValueError):boot.stage(battery_smbus_files=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_smbus_files=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        with self.assertRaises(ValueError):boot.stage(battery_smbus_files=True,laptop_probe_volume='37d8c5d0',battery_desktop=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_smbus_files=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_battery_smbus_is_automatic_and_native(self):
        version='ram-usb-v46-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-smbus',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_smbus=True)
        with self.assertRaises(ValueError):boot.stage(battery_smbus=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_smbus=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        with self.assertRaises(ValueError):boot.stage(battery_smbus=True,laptop_probe_volume='37d8c5d0',battery_desktop=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_smbus=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_battery_query_is_automatic_and_native(self):
        version='ram-usb-v45-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-query',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_query=True)
        with self.assertRaises(ValueError):boot.stage(battery_query=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_query=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        with self.assertRaises(ValueError):boot.stage(battery_query=True,laptop_probe_volume='37d8c5d0',battery_desktop=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_query=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_battery_controls_is_automatic_and_native(self):
        version='ram-usb-v44-laptop-probe-37d8c5d0-auto-return'
        cfg=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=battery-controls',cfg)
        self.assertIn('srvros.capture-failure=reboot',cfg)
        with self.assertRaises(ValueError):boot.stage(battery_controls=True)
        with self.assertRaises(ValueError):boot.stage(battery_controls=True,laptop_probe_volume='37d8c5d0',hold_on_failure=True)
        with self.assertRaises(ValueError):boot.stage(battery_controls=True,laptop_probe_volume='37d8c5d0',battery_service=True)
        with self.assertRaises(ValueError):boot.stage(battery_controls=True,laptop_probe_volume='37d8c5d0',battery_desktop=True)
        for name,data in {'build/limine-binary/BOOTX64.EFI':b'loader','build/kernel/srvros.elf':b'normal',
                          'build/acpi-battery/srvros.elf':b'native','build/initramfs.tar':b'init'}.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        with patch.object(boot,'ROOT',self.root),patch.object(boot.subprocess,'check_output',side_effect=['abc',b'']):
            staged=boot.stage(battery_controls=True,laptop_probe_volume='37d8c5d0')
        self.assertEqual((self.stage/staged/'srvros.elf').read_bytes(),b'native')
        self.assertEqual(boot.verify(self.stage/staged)['config_version'],version)

    def test_aml_capture_is_explicit_and_separate(self):
        version="ram-usb-v24-laptop-probe-37d8c5d0"
        release=self.payload("aml",version)
        path=self.stage/release/"manifest.json"
        manifest=json.loads(path.read_text());manifest["config_version"]=version
        path.write_text(json.dumps(manifest));boot.install(release)
        cfg=boot.limine_config(release,version)
        self.assertIn('srvros.capture=aml',cfg);self.assertIn('srvros.aml=probe',cfg)
        self.assertNotIn('srvros.desktop=trial',cfg);self.assertNotIn('srvros.audio=hda',cfg)
        with self.assertRaises(ValueError): boot.stage(aml_capture=True)
        with self.assertRaises(ValueError): boot.stage(laptop_probe_volume='37d8c5d0',aml_capture=True,idle_capture=True)

    def test_three_updates_keep_current_and_previous(self):
        releases = []
        for version in ("one", "two", "three"):
            releases.append(self.payload(version))
            boot.install()
        state = json.loads((self.base / "state.json").read_text())
        self.assertEqual(state["current"], releases[2])
        self.assertEqual(state["previous"], releases[1])
        self.assertFalse((self.base / releases[0]).exists())
        self.assertTrue((self.base / releases[1]).is_dir())
        self.assertIn("--id srvros-previous", self.snippet.read_text())
        # An identical redeploy must retain the previous release.
        boot.install()
        self.assertEqual(json.loads((self.base / "state.json").read_text()), state)

    def test_failed_update_restores_grub_and_old_selection(self):
        first = self.payload("one")
        boot.install()
        before = {path: path.read_bytes() for path in (self.snippet, self.cfg, self.base / "state.json")}
        self.payload("two")
        self.fail_update = True
        with self.assertRaises(subprocess.CalledProcessError):
            boot.install()
        for path, content in before.items():
            self.assertEqual(path.read_bytes(), content)
        self.assertTrue((self.base / first).is_dir())

    def test_failed_first_install_can_be_retried(self):
        self.payload("one")
        self.fail_update = True
        with self.assertRaises(subprocess.CalledProcessError):
            boot.install()
        self.assertFalse(self.snippet.exists())
        self.assertEqual(self.cfg.read_text(), "original Ubuntu config\n")
        self.fail_update = False
        boot.install()
        self.assertTrue(self.snippet.is_file())

    def test_upgrade_retains_old_configuration_release(self):
        old = self.payload("old", "ram-usb-v3-inputdiag")
        boot.install()
        current = self.payload("new")
        boot.install()
        state = json.loads((self.base / "state.json").read_text())
        self.assertEqual((state["current"], state["previous"]), (current, old))
        self.assertIn("srvros.inputdiag", (self.base / old / "limine.conf").read_text())
        self.assertNotIn("srvros.inputdiag", (self.base / current / "limine.conf").read_text())
        self.assertNotIn("srvros.log=debug", (self.base / current / "limine.conf").read_text())

    def test_all_configuration_versions_verify(self):
        for version in boot.CONFIG_VERSIONS:
            release = self.payload(version, version)
            boot.verify(self.stage / release)
        debug = boot.limine_config("0" * 16, boot.DIAGNOSTIC_VERSION)
        self.assertIn("srvros.inputprobe=1", debug)

    def test_hash_corruption_rejected_before_install(self):
        release = self.payload("one")
        (self.stage / release / "srvros.elf").write_text("corrupt")
        with self.assertRaises(ValueError):
            boot.install()
        self.assertFalse(self.base.exists())
        self.assertFalse(self.snippet.exists())

    def test_foreign_efi_directory_is_preserved(self):
        self.payload("one")
        self.base.mkdir(parents=True)
        foreign = self.base / "important.txt"
        foreign.write_text("keep me")
        with self.assertRaises(RuntimeError):
            boot.install()
        self.assertEqual(foreign.read_text(), "keep me")
        self.assertFalse(self.snippet.exists())

    def test_foreign_grub_snippet_is_preserved(self):
        self.payload("one")
        self.snippet.write_text("custom config\n")
        with self.assertRaises(RuntimeError):
            boot.install()
        self.assertEqual(self.snippet.read_text(), "custom config\n")
        self.assertFalse(self.base.exists())

    def test_keep_verified_previous_recovery(self):
        first=self.payload('accepted');boot.install(first)
        second=self.payload('trial');boot.install(second)
        third=self.payload('next')
        boot.install(third, keep_previous=True)
        state=json.loads((self.base/'state.json').read_text())
        self.assertEqual((state['current'],state['previous']),(third,first))
        self.assertTrue((self.base/first/'manifest.json').is_file())
        self.assertIn(first,self.snippet.read_text())
        with self.assertRaises(ValueError): boot.install(first, keep_previous=True)
        (self.base/first/'srvros.elf').write_bytes(b'bad')
        with self.assertRaises(ValueError): boot.install(second, keep_previous=True)
        self.assertEqual(json.loads((self.base/'state.json').read_text()),state)

    def test_autotest_profile_roundtrip_and_exact_install(self):
        version = 'ram-usb-v6-autotest-37d8c5d0-0123456789abcdef'
        release = self.payload('auto', version)
        path = self.stage / release / 'manifest.json'
        manifest = json.loads(path.read_text())
        manifest['config_version'] = version
        path.write_text(json.dumps(manifest))
        self.payload('different latest')
        boot.install(release)
        self.assertEqual(json.loads((self.base / 'state.json').read_text())['current'], release)
        cfg = (self.base / release / 'limine.conf').read_text()
        self.assertIn('srvros.autotest-volume=37d8c5d0', cfg)
        self.assertIn('srvros.autotest-run=0123456789abcdef', cfg)
        self.assertIn('srvros.autotest-release='+release, cfg)
        self.assertNotIn('srvros.usb-storage=rw', cfg)

    def test_workspace_profile_roundtrip_and_install(self):
        version = 'ram-usb-v7-workspace-37d8c5d0'
        release = self.payload('workspace', version)
        path = self.stage / release / 'manifest.json'
        manifest = json.loads(path.read_text())
        manifest['config_version'] = version
        path.write_text(json.dumps(manifest))
        boot.install(release)
        cfg = (self.base / release / 'limine.conf').read_text()
        self.assertIn('srvros.storage=ram srvros.workspace-volume=37d8c5d0', cfg)
        self.assertNotIn('srvros.usb-storage=rw', cfg)
        self.assertNotIn('srvros.autotest', cfg)

    def test_workspace_profile_conflicts_refused(self):
        for options in ({'diagnostic':True}, {'autotest_volume':'37d8c5d0'},
                        {'run_id':'0123456789abcdef'}):
            with self.assertRaises(ValueError): boot.stage(workspace_volume='37d8c5d0', **options)
        with self.assertRaises(ValueError): boot.stage(workspace_volume='bad')

    def test_gesture_profile(self):
        version='ram-usb-v12-trackpad-37d8c5d0-0123456789abcdef'
        release=self.payload('gestures',version)
        cfg=boot.limine_config(release,version)
        self.assertIn('srvros.trackpad=gestures',cfg)
        self.assertNotIn('srvros.trackpad=pointer',cfg)
        with self.assertRaises(ValueError): boot.stage(gestures=True)
    def test_tap_typing_profile(self):
        version='ram-usb-v50-trackpad-37d8c5d0-0123456789abcdef'
        release=self.payload('tap-typing',version)
        path=self.stage/release/'manifest.json'
        manifest=json.loads(path.read_text());manifest['config_version']=version
        path.write_text(json.dumps(manifest))
        cfg=boot.limine_config(release,version)
        self.assertIn('srvros.trackpad=gestures',cfg)
        self.assertIn('srvros.trackpad-policy=0123456789abcdef',cfg)
        self.assertNotIn('srvros.desktop-io=',cfg)
        boot.verify(self.stage/release)
        for args in ({}, {'desktop_io':True,'trackpad_run':'0123456789abcdef'}):
            with self.assertRaises(ValueError): boot.stage(tap_typing=True,**args)

    def test_pointer_profile(self):
        version='ram-usb-v11-trackpad-37d8c5d0-0123456789abcdef'
        release=self.payload('pointer',version)
        cfg=boot.limine_config(release,version)
        self.assertIn('srvros.trackpad=pointer',cfg)
        self.assertNotIn('srvros.trackpad=capture',cfg)
        with self.assertRaises(ValueError): boot.stage(pointer=True)

    def test_desktop_io_profile(self):
        for v in (14,15,16):
            version=f'ram-usb-v{v}-trackpad-37d8c5d0-0123456789abcdef'
            release=self.payload(f'desktop-io-{v}',version)
            cfg=boot.limine_config(release,version)
            self.assertIn('srvros.trackpad=gestures',cfg)
            self.assertIn('srvros.desktop-io=0123456789abcdef',cfg)
        with self.assertRaises(ValueError): boot.stage(desktop_io=True)
    def test_trackpad_profile_roundtrip_and_conflicts(self):
        version='ram-usb-v10-trackpad-37d8c5d0-0123456789abcdef'
        release=self.payload('trackpad',version)
        path=self.stage/release/'manifest.json'
        manifest=json.loads(path.read_text()); manifest['config_version']=version
        path.write_text(json.dumps(manifest)); boot.install(release)
        cfg=(self.base/release/'limine.conf').read_text()
        self.assertIn('srvros.trackpad=capture',cfg)
        self.assertIn('srvros.workspace-volume=37d8c5d0',cfg)
        self.assertIn('srvros.trackpad-release='+release,cfg)
        self.assertNotIn('srvros.usb-storage=rw',cfg)
        for args in ({'workspace_volume':'bad'},{'diagnostic':True},
                     {'workspace_volume':'37d8c5d0','autotest_volume':'37d8c5d0'},{}):
            with self.assertRaises(ValueError): boot.stage(trackpad_run='0123456789abcdef',**args)

    def test_unrecognized_profile_rejected(self):
        version = 'ram-usb-v5-autotest-37d8c5d0-0123456789abcdeg'
        release = self.payload('bad', version)
        path = self.stage / release / 'manifest.json'
        manifest = json.loads(path.read_text()); manifest['config_version'] = version
        path.write_text(json.dumps(manifest))
        with self.assertRaises(ValueError): boot.install(release)


class CombinedProfileTests(unittest.TestCase):
    def test_shared_version_numbers_keep_profile_specific_resolution(self):
        for version in (31, 36):
            probe=boot.limine_config("a"*16,f"ram-usb-v{version}-laptop-probe-12345678-auto-return")
            desktop=boot.limine_config("a"*16,f"ram-usb-v{version}-desktop-12345678")
            self.assertNotIn("resolution:",probe)
            self.assertIn("srvros.capture=battery",probe)
            self.assertIn("srvros.capture-failure=reboot",probe)
            self.assertIn("resolution: 2880x1800",desktop)
            self.assertNotIn("srvros.capture=battery",desktop)

    def test_load_and_battery_modes_are_exclusive(self):
        for mode in ("keyboard_capture", "cpu_load_capture"):
            with self.assertRaises(ValueError):
                boot.stage(laptop_probe_volume="12345678",battery_service=True,**{mode:True})


if __name__ == "__main__":
    unittest.main()
