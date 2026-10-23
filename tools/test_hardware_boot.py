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

    def test_v43_collision_exact_legacy_variants(self):
        version='ram-usb-v43-laptop-probe-37d8c5d0-auto-return'
        release=self.payload('legacy',version);directory=self.stage/release
        path=directory/'manifest.json';manifest=json.loads(path.read_text());manifest['config_version']=version
        for legacy in (False,True):
            config=boot.limine_config(release,version,legacy_wifi_calibration=legacy)
            for name in ('limine.conf','loader/limine.conf'):
                (directory/name).write_text(config);manifest['sha256'][name]=boot.digest(directory/name)
            path.write_text(json.dumps(manifest));self.assertEqual(boot.verify(directory)['release'],release)
        # Hash-consistent mixed copies and an unrecognized command remain invalid.
        for config in (boot.limine_config(release,version),config+'unexpected\n'):
            (directory/'loader/limine.conf').write_text(config)
            manifest['sha256']['loader/limine.conf']=boot.digest(directory/'loader/limine.conf')
            path.write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):boot.verify(directory)

    def test_v44_collision_exact_legacy_variants(self):
        version='ram-usb-v44-laptop-probe-37d8c5d0-auto-return'
        release=self.payload('legacy',version);directory=self.stage/release
        path=directory/'manifest.json';manifest=json.loads(path.read_text());manifest['config_version']=version
        for legacy in (False,True):
            config=boot.limine_config(release,version,legacy_wifi_calibration=legacy)
            for name in ('limine.conf','loader/limine.conf'):
                (directory/name).write_text(config);manifest['sha256'][name]=boot.digest(directory/name)
            path.write_text(json.dumps(manifest));self.assertEqual(boot.verify(directory)['release'],release)
        # Hash-consistent mixed copies and an unrecognized command remain invalid.
        for config in (boot.limine_config(release,version),config+'unexpected\n'):
            (directory/'loader/limine.conf').write_text(config)
            manifest['sha256']['loader/limine.conf']=boot.digest(directory/'loader/limine.conf')
            path.write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):boot.verify(directory)

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

    def test_wifi_firmware_profile(self):
        version='ram-usb-wifi-fw-v1-laptop-probe-37d8c5d0-auto-return'
        self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(version))
        config=boot.limine_config('0123456789abcdef',version)
        self.assertIn('srvros.capture=wifi-firmware',config)
        self.assertIn('srvros.capture-failure=reboot',config)
        self.assertNotIn('srvros.capture=battery',config)
        with self.assertRaises(ValueError):boot.stage(wifi_firmware_package=self.root)
        held=boot.limine_config('0123456789abcdef',version.removesuffix('-auto-return'))
        self.assertIn('srvros.capture=wifi-firmware',held)
        self.assertNotIn('srvros.capture-failure=reboot',held)
        self.assertEqual(config,held.replace('srvros.usb-wifi=probe','srvros.usb-wifi=probe srvros.capture-failure=reboot'))
        for other in ('wifi_capture','wifi_calibration','battery_app_files','battery_load'):
            with self.assertRaises(ValueError):boot.stage(wifi_firmware_package=self.root,laptop_probe_volume='37d8c5d0',**{other:True})

    def test_wifi_mac_startup_profile(self):
        old="ram-usb-wifi-fw-v1-laptop-probe-37d8c5d0"
        for suffix in ("", "-auto-return"):
            v2=old.replace("v1","v2")+suffix
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v2))
            original=boot.limine_config("0123456789abcdef",old+suffix)
            trial=boot.limine_config("0123456789abcdef",v2)
            self.assertNotIn("srvros.wifi-mac-startup=1",original)
            self.assertEqual(trial,original.replace("srvros.usb-wifi=probe","srvros.usb-wifi=probe srvros.wifi-mac-startup=1"))
        with self.assertRaises(ValueError):boot.stage(wifi_mac_startup=True,laptop_probe_volume="37d8c5d0")

    def test_wifi_mcu_profile(self):
        for suffix in ('', '-auto-return'):
            v2='ram-usb-wifi-fw-v2-laptop-probe-37d8c5d0'+suffix
            v3=v2.replace('v2','v3')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v3))
            old=boot.limine_config('0123456789abcdef',v2)
            new=boot.limine_config('0123456789abcdef',v3)
            self.assertEqual(new,old.replace('srvros.wifi-mac-startup=1',
                'srvros.wifi-mac-startup=1 srvros.wifi-mcu-probe=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_mcu_probe=True)
        with self.assertRaises(ValueError):boot.stage(wifi_mcu_probe=True,wifi_firmware_package=self.root)

    def test_wifi_radio_reset_profile(self):
        for suffix in ('', '-auto-return'):
            v3='ram-usb-wifi-fw-v3-laptop-probe-37d8c5d0'+suffix
            v4=v3.replace('v3','v4')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v4))
            old=boot.limine_config('0123456789abcdef',v3)
            new=boot.limine_config('0123456789abcdef',v4)
            self.assertEqual(new,old.replace('srvros.wifi-mcu-probe=1',
                'srvros.wifi-mcu-probe=1 srvros.wifi-radio-reset=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_radio_reset=True)

    def test_wifi_write_probe_profile(self):
        for suffix in ('', '-auto-return'):
            v4='ram-usb-wifi-fw-v4-laptop-probe-37d8c5d0'+suffix
            v5=v4.replace('v4','v5')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v5))
            self.assertEqual(boot.limine_config('0123456789abcdef',v5),
                boot.limine_config('0123456789abcdef',v4).replace(
                    'srvros.wifi-radio-reset=1',
                    'srvros.wifi-radio-reset=1 srvros.wifi-write-probe=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_write_probe=True)

    def test_wifi_pair_probe_profile(self):
        for suffix in ('', '-auto-return'):
            v5='ram-usb-wifi-fw-v5-laptop-probe-37d8c5d0'+suffix
            v6=v5.replace('v5','v6')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v6))
            self.assertEqual(boot.limine_config('0123456789abcdef',v6),
                boot.limine_config('0123456789abcdef',v5).replace(
                    'srvros.wifi-write-probe=1',
                    'srvros.wifi-write-probe=1 srvros.wifi-pair-probe=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_pair_probe=True)

    def test_wifi_timing_probe_profile(self):
        for suffix in ('', '-auto-return'):
            v6='ram-usb-wifi-fw-v6-laptop-probe-37d8c5d0'+suffix
            v7=v6.replace('v6','v7')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v7))
            self.assertEqual(boot.limine_config('0123456789abcdef',v7),
                boot.limine_config('0123456789abcdef',v6).replace(
                    'srvros.wifi-pair-probe=1',
                    'srvros.wifi-pair-probe=1 srvros.wifi-timing-probe=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_timing_probe=True)

    def test_wifi_mac_basics_profile(self):
        for suffix in ('', '-auto-return'):
            v7='ram-usb-wifi-fw-v7-laptop-probe-37d8c5d0'+suffix
            v8=v7.replace('v7','v8')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v8))
            self.assertEqual(boot.limine_config('0123456789abcdef',v8),
                boot.limine_config('0123456789abcdef',v7).replace(
                    'srvros.wifi-timing-probe=1',
                    'srvros.wifi-timing-probe=1 srvros.wifi-mac-basics=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_mac_basics=True)

    def test_wifi_mac_format_profile(self):
        for suffix in ('', '-auto-return'):
            v8='ram-usb-wifi-fw-v8-laptop-probe-37d8c5d0'+suffix
            v9=v8.replace('v8','v9')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v9))
            self.assertEqual(boot.limine_config('0123456789abcdef',v9),
                boot.limine_config('0123456789abcdef',v8).replace(
                    'srvros.wifi-mac-basics=1',
                    'srvros.wifi-mac-basics=1 srvros.wifi-mac-format=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_mac_format=True)

    def test_wifi_bbp_ready_profile(self):
        for suffix in ('', '-auto-return'):
            v9='ram-usb-wifi-fw-v9-laptop-probe-37d8c5d0'+suffix
            v10=v9.replace('v9','v10')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v10))
            self.assertEqual(boot.limine_config('0123456789abcdef',v10),
                boot.limine_config('0123456789abcdef',v9).replace(
                    'srvros.wifi-mac-format=1',
                    'srvros.wifi-mac-format=1 srvros.wifi-bbp-ready=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_bbp_ready=True)

    def test_wifi_bbp_core_profile(self):
        for suffix in ('', '-auto-return'):
            v10='ram-usb-wifi-fw-v10-laptop-probe-37d8c5d0'+suffix
            v11=v10.replace('v10','v11')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v11))
            self.assertEqual(boot.limine_config('0123456789abcdef',v11),
                boot.limine_config('0123456789abcdef',v10).replace(
                    'srvros.wifi-bbp-ready=1',
                    'srvros.wifi-bbp-ready=1 srvros.wifi-bbp-core=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_bbp_core=True)

    def test_wifi_bbp_front_profile(self):
        for suffix in ('', '-auto-return'):
            v11='ram-usb-wifi-fw-v11-laptop-probe-37d8c5d0'+suffix
            v12=v11.replace('v11','v12')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v12))
            self.assertEqual(boot.limine_config('0123456789abcdef',v12),
                boot.limine_config('0123456789abcdef',v11).replace(
                    'srvros.wifi-bbp-core=1',
                    'srvros.wifi-bbp-core=1 srvros.wifi-bbp-front=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_bbp_front=True)

    def test_wifi_bbp_agc_profile(self):
        for suffix in ('', '-auto-return'):
            v12='ram-usb-wifi-fw-v12-laptop-probe-37d8c5d0'+suffix
            v13=v12.replace('v12','v13')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v13))
            self.assertEqual(boot.limine_config('0123456789abcdef',v13),
                boot.limine_config('0123456789abcdef',v12).replace(
                    'srvros.wifi-bbp-front=1',
                    'srvros.wifi-bbp-front=1 srvros.wifi-bbp-agc=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_bbp_agc=True)

    def test_wifi_bbp_batch_profile(self):
        for suffix in ('', '-auto-return'):
            v13='ram-usb-wifi-fw-v13-laptop-probe-37d8c5d0'+suffix
            v14=v13.replace('v13','v14')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v14))
            self.assertEqual(boot.limine_config('0123456789abcdef',v14),
                boot.limine_config('0123456789abcdef',v13).replace(
                    'srvros.wifi-bbp-agc=1',
                    'srvros.wifi-bbp-agc=1 srvros.wifi-bbp-batch=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_bbp_batch=True)

    def test_wifi_compact_profile(self):
        for suffix in ('', '-auto-return'):
            v14='ram-usb-wifi-fw-v14-laptop-probe-37d8c5d0'+suffix
            v15=v14.replace('v14','v15')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v15))
            self.assertEqual(boot.limine_config('0123456789abcdef',v15),
                boot.limine_config('0123456789abcdef',v14).replace(
                    'srvros.wifi-bbp-batch=1',
                    'srvros.wifi-bbp-batch=1 srvros.wifi-compact=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_compact=True)

    def test_wifi_agc_more_profile(self):
        for suffix in ('', '-auto-return'):
            v15='ram-usb-wifi-fw-v15-laptop-probe-37d8c5d0'+suffix
            v16=v15.replace('v15','v16')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v16))
            self.assertEqual(boot.limine_config('0123456789abcdef',v16),
                boot.limine_config('0123456789abcdef',v15).replace(
                    'srvros.wifi-compact=1',
                    'srvros.wifi-compact=1 srvros.wifi-agc-more=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_agc_more=True)

    def test_wifi_agc_next_profile(self):
        for suffix in ('', '-auto-return'):
            v16='ram-usb-wifi-fw-v16-laptop-probe-37d8c5d0'+suffix
            v17=v16.replace('v16','v17')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v17))
            self.assertEqual(boot.limine_config('0123456789abcdef',v17),
                boot.limine_config('0123456789abcdef',v16).replace(
                    'srvros.wifi-agc-more=1',
                    'srvros.wifi-agc-more=1 srvros.wifi-agc-next=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_agc_next=True)

    def test_wifi_agc_tail_profile(self):
        for suffix in ('', '-auto-return'):
            v17='ram-usb-wifi-fw-v17-laptop-probe-37d8c5d0'+suffix
            v18=v17.replace('v17','v18')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v18))
            self.assertEqual(boot.limine_config('0123456789abcdef',v18),
                boot.limine_config('0123456789abcdef',v17).replace(
                    'srvros.wifi-agc-next=1',
                    'srvros.wifi-agc-next=1 srvros.wifi-agc-tail=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_agc_tail=True)

    def test_wifi_rxc_start_profile(self):
        for suffix in ('', '-auto-return'):
            v18='ram-usb-wifi-fw-v18-laptop-probe-37d8c5d0'+suffix
            v19=v18.replace('v18','v19')
            self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(v19))
            self.assertEqual(boot.limine_config('0123456789abcdef',v19),
                boot.limine_config('0123456789abcdef',v18).replace(
                    'srvros.wifi-agc-tail=1',
                    'srvros.wifi-agc-tail=1 srvros.wifi-rxc-start=1'))
        with self.assertRaises(ValueError):boot.stage(wifi_rxc_start=True)

    def test_wifi_calibration_profile(self):
        version="ram-usb-wifi-cal-v1-laptop-probe-37d8c5d0-auto-return"
        self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(version))
        cfg=boot.limine_config("0123456789abcdef",version)
        for flag in ("srvros.capture=wifi-calibration","srvros.usb-wifi=probe","srvros.capture-failure=reboot"):
            self.assertIn(flag,cfg)
        for flag in ("srvros.aml","srvros.audio","srvros.laptop-services","srvros.usb-network"):
            self.assertNotIn(flag,cfg)
        self.assertNotIn("wifi-calibration",boot.limine_config("0123456789abcdef","ram-usb-v39-laptop-probe-37d8c5d0-auto-return"))
        with self.assertRaises(ValueError): boot.stage(wifi_calibration=True)
        with self.assertRaises(ValueError): boot.stage(wifi_calibration=True,hold_on_failure=True,laptop_probe_volume="37d8c5d0")
        for other in ("wifi_capture","battery_text","battery_desktop","idle_capture","battery_controls","battery_query","battery_smbus","battery_smbus_files","battery_home","battery_home_session"):
            with self.assertRaises(ValueError): boot.stage(wifi_calibration=True,laptop_probe_volume="37d8c5d0",**{other:True})

    def test_wifi_capture_profile(self):
        version="ram-usb-v39-laptop-probe-37d8c5d0-auto-return"
        self.assertTrue(boot.LAPTOP_PROBE_RE.fullmatch(version))
        cfg=boot.limine_config("0123456789abcdef",version)
        for flag in ("srvros.capture=wifi", "srvros.usb-wifi=probe", "srvros.capture-failure=reboot"):
            self.assertIn(flag,cfg)
        for flag in ("srvros.aml", "srvros.audio", "srvros.laptop-services", "srvros.usb-network"):
            self.assertNotIn(flag,cfg)
        self.assertNotIn("srvros.usb-wifi",boot.limine_config("0123456789abcdef","ram-usb-v19-laptop-probe-37d8c5d0-auto-return"))
        with self.assertRaises(ValueError): boot.stage(wifi_capture=True)
        with self.assertRaises(ValueError): boot.stage(wifi_capture=True,hold_on_failure=True,laptop_probe_volume="37d8c5d0")
        with self.assertRaises(ValueError): boot.stage(wifi_capture=True,idle_capture=True,laptop_probe_volume="37d8c5d0")

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
