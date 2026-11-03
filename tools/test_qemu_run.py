import unittest
from pathlib import Path
from unittest.mock import patch
import qemu_run


class QemuRunTest(unittest.TestCase):
    def test_defaults_and_forwarded_arguments(self):
        args = ["--qemu", "/path with spaces/qemu", "--", "-serial", "stdio", "-drive", "file=a b", "-s", "-S"]
        with patch.object(qemu_run, "build_iso", return_value="/tmp/isolated.iso") as build, \
             patch.object(qemu_run.subprocess, "call", return_value=7) as call:
            self.assertEqual(qemu_run.main(args), 7)
        self.assertEqual(build.call_args.kwargs["cmdline"], "srvros.log=quiet srvros.virtio-gpu=present")
        command = call.call_args.args[0]
        self.assertEqual(command, ["/path with spaces/qemu", "-cdrom", "/tmp/isolated.iso", "-vga", "none",
                                   "-device", "virtio-vga", "-serial", "stdio", "-drive", "file=a b", "-s", "-S"])

    def test_standard_vga_is_explicit_and_has_no_present_token(self):
        with patch.object(qemu_run, "build_iso", return_value="/tmp/isolated.iso") as build, \
             patch.object(qemu_run.subprocess, "call", return_value=0) as call:
            self.assertEqual(qemu_run.main(["--display-device", "std", "--", "-netdev", "user,id=net0"]), 0)
        self.assertEqual(build.call_args.kwargs["cmdline"], "srvros.log=quiet")
        self.assertEqual(call.call_args.args[0][1:], ["-cdrom", "/tmp/isolated.iso", "-vga", "std", "-netdev", "user,id=net0"])
        self.assertNotIn("srvros.a1466.capture=1", build.call_args.kwargs["cmdline"])

    def test_isolated_iso_exists_until_qemu_finishes(self):
        captured = {}
        def fake_build(root, directory, width, height, env, cmdline):
            captured["directory"] = Path(directory)
            captured["root"] = Path(root)
            iso = Path(directory) / "session.iso"
            iso.write_bytes(b"isolated session")
            return str(iso)
        def fake_qemu(command, cwd):
            self.assertEqual(Path(command[2]).read_bytes(), b"isolated session")
            self.assertEqual(cwd, qemu_run.ROOT)
            return 0
        with patch.object(qemu_run, "build_iso", side_effect=fake_build), \
             patch.object(qemu_run.subprocess, "call", side_effect=fake_qemu):
            self.assertEqual(qemu_run.main([]), 0)
        self.assertEqual(captured["root"], qemu_run.ROOT)
        self.assertFalse(captured["directory"].exists())

    def test_rejects_invalid_display(self):
        with self.assertRaises(ValueError): qemu_run.boot_cmdline("other")
        with self.assertRaises(ValueError): qemu_run.qemu_command("qemu", "image", "other", [])

    def test_virtio_baseline_enables_drivers_and_forwards_disk_network(self):
        extra=["-device","virtio-blk-pci,drive=disk,disable-legacy=on"]
        with patch.object(qemu_run,"build_iso",return_value="/tmp/iso") as build, \
             patch.object(qemu_run.subprocess,"call",return_value=0) as call:
            self.assertEqual(qemu_run.main(["--virtio-devices","--",*extra]),0)
        self.assertIn("srvros.virtio=on",build.call_args.kwargs["cmdline"])
        self.assertIn("virtio-keyboard-pci",call.call_args.args[0])
        self.assertEqual(call.call_args.args[0][-2:],extra)


if __name__ == "__main__":
    unittest.main()
