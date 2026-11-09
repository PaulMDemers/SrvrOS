#!/usr/bin/env python3
"""Compile main.c's actual Wi-Fi selection with real staged command lines.

Capture functions are recording stubs;this tests boot dispatch,not hardware.
No staging,credentials,kernel build or filesystem-image mutation is performed.
"""
import ctypes
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

import hardware_boot

ROOT=Path(__file__).resolve().parents[1]

def function(source,name):
    start=source.index('static bool '+name+'(')
    opening=source.index('{',start);depth=1;end=opening+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

def selection(source):
    # The statement is extracted verbatim;do not implement a second dispatcher.
    start=source.index('sampled=(!cmdline_has_token(cmdline,"srvros.wifi-nop5=1")')
    return source[start:source.index(';',start)+1]

class MainDispatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source=(ROOT/'kernel/src/main.c').read_text();expression=selection(source)
        names=sorted(set(re.findall(r'\b(xhci_wifi_capture_\w+)\(\)',expression)))
        declarations=[]
        for name in ('wifi_provider_trial','wifi_5ghz_prereq','wifi_channel_diagnosis','wifi_desktop_trial','return_on_failure','wifi_log_isolation'):
            match=re.search(r'\bbool '+name+r'\s*=[^;]+;',source)
            if not match:raise AssertionError('missing native declaration '+name)
            declarations.append(match.group())
        stubs='\n'.join('static bool '+name+'(void){calls++;chosen="'+name+'";return capture_ok;}' for name in names)
        program='\n'.join(('#include <stdbool.h>','#include <stddef.h>','#include <stdint.h>',
            function(source,'char_is_separator'),function(source,'cmdline_has_token'),
            'static unsigned calls;static const char *chosen;static bool capture_ok;',stubs,
            'const char *dispatch(const char *cmdline,unsigned parked,unsigned successful,unsigned *sample,unsigned *count){',
            'calls=0;chosen="none";capture_ok=successful!=0;bool sampled=false;bool nop_logger_parked=parked!=0;bool stream_logger_parked=parked!=0;',
            *declarations,expression,'*sample=sampled;*count=calls;return chosen;}'))
        cls.tmp=tempfile.TemporaryDirectory(prefix='srvros-main-dispatch-');cls.addClassCleanup(cls.tmp.cleanup)
        directory=Path(cls.tmp.name);(directory/'dispatch.c').write_text(program)
        command=shlex.split(os.environ.get('WIFI_DISPATCH_CC','cc'))+['-std=c11','-O2','-Wall','-Wextra','-Werror','-fPIC','-shared',str(directory/'dispatch.c'),'-o',str(directory/'dispatch.so')]
        subprocess.run(command,check=True,capture_output=True,text=True)
        cls.library=ctypes.CDLL(str(directory/'dispatch.so'))
        cls.library.dispatch.argtypes=(ctypes.c_char_p,ctypes.c_uint,ctypes.c_uint,ctypes.POINTER(ctypes.c_uint),ctypes.POINTER(ctypes.c_uint))
        cls.library.dispatch.restype=ctypes.c_char_p

    @staticmethod
    def recipe(version):
        config=hardware_boot.limine_config('0123456789abcdef',f'ram-usb-wifi-fw-v{version}-laptop-probe-0e535900-auto-return')
        return next(line.split(':',1)[1].strip() for line in config.splitlines() if line.strip().startswith('cmdline:'))

    def dispatch(self,cmdline,parked=True,successful=True):
        sample=ctypes.c_uint();count=ctypes.c_uint()
        name=self.library.dispatch(cmdline.encode(),parked,successful,ctypes.byref(sample),ctypes.byref(count)).decode()
        return bool(sample.value),name,count.value

    def test_actual_staged_profiles(self):
        expected={124:'handoff5',125:'peer5',126:'tx5',127:'join5',128:'eapol5',129:'wpa5',130:'wpa5',131:'data5',132:'provider5',133:'reuse5',134:'nop5',135:'stream5',136:'stream5_dhcp'}
        for version,child in expected.items():
            with self.subTest(version=version):self.assertEqual(self.dispatch(self.recipe(version)),(True,'xhci_wifi_capture_'+child,1))

    def test_capture_failure_propagates(self):
        for version in (126,130,131,132,133,134,135,136):
            with self.subTest(version=version):
                ok,name,count=self.dispatch(self.recipe(version),successful=False)
                self.assertFalse(ok);self.assertNotEqual(name,'none');self.assertEqual(count,1)

    def test_nop5_needs_parked_logger(self):
        self.assertEqual(self.dispatch(self.recipe(134),parked=False),(False,'none',0))
        self.assertEqual(self.dispatch(self.recipe(126),parked=False),(True,'xhci_wifi_capture_tx5',1))

    def test_nop5_forbidden_profile_mixes(self):
        for flag in ('join5','eapol5','wpa5','data5','provider5','reuse5','desktop-trial'):
            with self.subTest(flag=flag):self.assertEqual(self.dispatch(self.recipe(134)+' srvros.wifi-'+flag+'=1'),(False,'none',0))

    def test_nop5_parent_and_return_refusals(self):
        for token in ('srvros.wifi-tx5-owner=1','srvros.wifi-peer5=1','srvros.wifi-handoff5=1','srvros.wifi-power5=1','srvros.wifi-discovery5=1','srvros.wifi-cal5=1','srvros.wifi-channel-vco5=1','srvros.wifi-vco5=1','srvros.wifi-dac5=1','srvros.wifi-trim5-channel=1','srvros.wifi-held-channel=1','srvros.wifi-bbp5-cycle=1','srvros.wifi-pa5-cycle=1','srvros.wifi-frontend=1','srvros.wifi-a20-cycle=1','srvros.wifi-5ghz-channel=1','srvros.wifi-5ghz-band=1','srvros.wifi-5ghz-prereq=1','srvros.capture-failure=reboot'):
            with self.subTest(token=token):
                tokens=self.recipe(134).split();self.assertIn(token,tokens);tokens.remove(token)
                self.assertEqual(self.dispatch(' '.join(tokens)),(False,'none',0))

    def test_stream5_fenced_profile_refusals(self):
        self.assertEqual(self.dispatch(self.recipe(135),parked=False),(False,'none',0))
        for flag in ('wpa5','data5','provider5','reuse5','nop5','desktop-trial'):
            with self.subTest(flag=flag):self.assertEqual(self.dispatch(self.recipe(135)+' srvros.wifi-'+flag+'=1'),(False,'none',0))
        for token in ('srvros.wifi-eapol5=1','srvros.wifi-join5=1','srvros.wifi-tx5-owner=1','srvros.wifi-peer5=1','srvros.wifi-handoff5=1','srvros.wifi-cal5=1','srvros.capture-failure=reboot'):
            with self.subTest(token=token):
                tokens=self.recipe(135).split();self.assertIn(token,tokens);tokens.remove(token)
                self.assertEqual(self.dispatch(' '.join(tokens)),(False,'none',0))
        self.assertEqual(self.dispatch(self.recipe(128)+' srvros.wifi-stream5=10'),(True,'xhci_wifi_capture_eapol5',1))
        self.assertEqual(self.dispatch('\t\r\n'.join(self.recipe(135).split())),(True,'xhci_wifi_capture_stream5',1))

    def test_stream5_dhcp_profile_refusals(self):
        self.assertEqual(self.dispatch(self.recipe(136),parked=False),(False,'none',0))
        for token in ('srvros.wifi-stream5=1','srvros.wifi-eapol5=1','srvros.wifi-join5=1','srvros.capture-failure=reboot'):
            tokens=self.recipe(136).split();tokens.remove(token)
            with self.subTest(token=token):self.assertEqual(self.dispatch(' '.join(tokens)),(False,'none',0))
        for flag in ('wpa5','data5','provider5','reuse5','nop5','desktop-trial'):
            with self.subTest(flag=flag):self.assertEqual(self.dispatch(self.recipe(136)+' srvros.wifi-'+flag+'=1'),(False,'none',0))
        self.assertEqual(self.dispatch(self.recipe(128)+' srvros.wifi-stream5-dhcp=1'),(False,'none',0))
        self.assertEqual(self.dispatch(self.recipe(135)+' srvros.wifi-stream5-dhcp=10'),(True,'xhci_wifi_capture_stream5',1))
        self.assertEqual(self.dispatch('\t\r\n'.join(self.recipe(136).split())),(True,'xhci_wifi_capture_stream5_dhcp',1))

    def test_exact_nop_token_and_whitespace(self):
        cmdline=self.recipe(126)+' srvros.wifi-nop5=10'
        self.assertEqual(self.dispatch(cmdline),(True,'xhci_wifi_capture_tx5',1))
        cmdline='\t\r\n'.join(self.recipe(134).split())
        self.assertEqual(self.dispatch(cmdline),(True,'xhci_wifi_capture_nop5',1))

if __name__=='__main__':unittest.main()
