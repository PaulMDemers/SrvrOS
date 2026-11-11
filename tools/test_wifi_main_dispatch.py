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
    start=source.index('sampled=(!wifi_http_reconnect5_requested || wifi_http_reconnect5_trial) && (!cmdline_has_token(cmdline,"srvros.wifi-stack5=1")')
    return source[start:source.index(';',start)+1]

class MainDispatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source=(ROOT/'kernel/src/main.c').read_text();expression=selection(source)
        begin=source.index('                if(sampled && cmdline_has_token(cmdline,"srvros.wifi-cal5-cleanup=1"))')
        setup=source[begin:source.index('                if(sampled && cmdline_has_token(cmdline,"srvros.wifi-bbp-table=1"))',begin)]
        names=sorted(set(re.findall(r'\b(xhci_wifi_capture_\w+)\(\)',expression)))
        declarations=[]
        for name in ('wifi_provider_trial','http_child_trial','wifi_stack5_trial','wifi_reconnect5_requested','wifi_reconnect5_trial','wifi_http_reconnect5_requested','wifi_http_reconnect5_trial','wifi_reconnect5_capture_attempted','wifi_http5_requested','wifi_http5_trial','wifi_5ghz_prereq','wifi_channel_diagnosis','wifi_desktop_trial','return_on_failure','wifi_log_isolation'):
            match=re.search(r'\bbool '+name+r'\s*=[^;]+;',source)
            if not match:raise AssertionError('missing native declaration '+name)
            declarations.append(match.group())
        stubs='\n'.join('static bool '+name+'(void){calls++;chosen="'+name+'";return capture_ok;}' for name in names)
        program='\n'.join(('#include <stdbool.h>','#include <stddef.h>','#include <stdint.h>',
            function(source,'char_is_separator'),function(source,'cmdline_has_token'),function(source,'cmdline_tokens_unique'),function(source,'wifi_reconnect5_recipe'),
            'static unsigned calls,setup_calls,last_reconnect_attempted,last_network_probe;static const char *chosen;static bool capture_ok;',stubs,
            'static bool xhci_wifi_cal5_cleanup_enable(void){setup_calls++;return capture_ok;}',
            'unsigned dispatch_setup_calls(void){return setup_calls;}',
            'unsigned dispatch_reconnect_attempted(void){return last_reconnect_attempted;}',
            'unsigned dispatch_network_probe(void){return last_network_probe;}',
            'const char *dispatch(const char *cmdline,unsigned parked,unsigned successful,unsigned *sample,unsigned *count){',
            'calls=setup_calls=0;chosen="none";capture_ok=successful!=0;bool sampled=true;bool nop_logger_parked=parked!=0;bool stream_logger_parked=parked!=0;',
            *declarations,setup,'if(sampled){',expression,'}','last_reconnect_attempted=wifi_reconnect5_capture_attempted;last_network_probe=(http_child_trial || wifi_http5_trial || wifi_http_reconnect5_trial);*sample=sampled;*count=calls;return chosen;}'))
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
        expected={124:'handoff5',125:'peer5',126:'tx5',127:'join5',128:'eapol5',129:'wpa5',130:'wpa5',131:'data5',132:'provider5',133:'reuse5',134:'nop5',135:'stream5',136:'stream5_dhcp',137:'stack5_probe',138:'stack5_probe',139:'http5',140:'reconnect5_probe',143:'http_reconnect5'}
        for version,child in expected.items():
            with self.subTest(version=version):self.assertEqual(self.dispatch(self.recipe(version)),(True,'xhci_wifi_capture_'+child,1))

    def test_http_reconnect_native_dispatch_is_explicit(self):
        recipe=self.recipe(143)
        self.assertEqual(self.dispatch(recipe),(True,'xhci_wifi_capture_http_reconnect5',1))
        self.assertEqual(self.library.dispatch_network_probe(),1)
        for version in (138,139,140):
            changed=self.recipe(version)+' srvros.wifi-reconnect5-http=1'
            self.assertEqual(self.dispatch(changed),(False,'none',0))
        for token in ('srvros.wifi-reconnect5=1','srvros.wifi-stack5=1','srvros.wifi-cal5-cleanup=1'):
            self.assertEqual(self.dispatch(recipe.replace(token,'')),(False,'none',0))
        self.assertEqual(self.dispatch(recipe+' srvros.wifi-http5=1'),(False,'none',0))

    def test_cleanup_setup_gate(self):
        self.assertEqual(self.dispatch(self.recipe(138)),(True,'xhci_wifi_capture_stack5_probe',1))
        self.assertEqual(self.library.dispatch_setup_calls(),1)
        self.assertEqual(self.dispatch(self.recipe(138),successful=False),(False,'none',0))
        for version in (128,135,136):
            self.assertEqual(self.dispatch(self.recipe(version)+' srvros.wifi-cal5-cleanup=1'),(False,'none',0))
            self.assertEqual(self.library.dispatch_setup_calls(),0)
        for token in ('srvros.wifi-cal5=1','srvros.capture-failure=reboot','srvros.wifi-stack5=1'):
            tokens=self.recipe(138).split();tokens.remove(token)
            self.assertEqual(self.dispatch(' '.join(tokens)),(False,'none',0))
            self.assertEqual(self.library.dispatch_setup_calls(),0)
        for token in self.recipe(138).split():
            self.assertEqual(self.dispatch(self.recipe(138)+' '+token),(False,'none',0))

    def test_capture_failure_propagates(self):
        for version in (126,130,131,132,133,134,135,136,137):
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

    def test_stack5_profile_refusals(self):
        self.assertEqual(self.dispatch(self.recipe(137),parked=False),(False,'none',0))
        for flag in ('stream5-dhcp','wpa5','data5','provider5','reuse5','nop5','desktop-trial'):
            with self.subTest(flag=flag):self.assertEqual(self.dispatch(self.recipe(137)+' srvros.wifi-'+flag+'=1'),(False,'none',0))
        for token in ('srvros.wifi-stream5=1','srvros.wifi-eapol5=1','srvros.wifi-join5=1','srvros.wifi-peer5=1','srvros.capture-failure=reboot'):
            tokens=self.recipe(137).split();tokens.remove(token)
            with self.subTest(token=token):self.assertEqual(self.dispatch(' '.join(tokens)),(False,'none',0))
        for token in self.recipe(137).split():
            with self.subTest(duplicate=token):self.assertEqual(self.dispatch(self.recipe(137)+' '+token),(False,'none',0))
        self.assertEqual(self.dispatch(self.recipe(128)+' srvros.wifi-stack5=1'),(False,'none',0))
        self.assertEqual(self.dispatch(self.recipe(135)+' srvros.wifi-stack5=10'),(True,'xhci_wifi_capture_stream5',1))
        self.assertEqual(self.dispatch('\t\r\n'.join(self.recipe(137).split())),(True,'xhci_wifi_capture_stack5_probe',1))

    def test_reconnect5_complete_recipe(self):
        recipe=self.recipe(140)
        self.assertEqual(self.dispatch(recipe),(True,'xhci_wifi_capture_reconnect5_probe',1))
        self.assertEqual(self.library.dispatch_reconnect_attempted(),1)
        self.assertEqual(self.library.dispatch_network_probe(),0)
        self.assertEqual(self.library.dispatch_setup_calls(),1)
        self.assertEqual(self.dispatch(recipe,parked=False),(False,'none',0))
        self.assertEqual(self.library.dispatch_reconnect_attempted(),0)
        self.assertEqual(self.dispatch('\t\r\n'.join(recipe.split())),(True,'xhci_wifi_capture_reconnect5_probe',1))
        # Exact opt-in: absent/new value10 retain the old v138 dispatch.
        self.assertEqual(self.dispatch(recipe.replace('srvros.wifi-reconnect5=1','srvros.wifi-reconnect5=10')),(True,'xhci_wifi_capture_stack5_probe',1))

    def test_reconnect5_missing_parents_and_duplicate_tokens(self):
        recipe=self.recipe(140)
        for token in recipe.split():
            if token.startswith(('srvros.workspace-volume=','srvros.desktop-release=','srvros.wifi-reconnect5=')):
                continue
            with self.subTest(missing=token):
                words=recipe.split();words.remove(token)
                self.assertEqual(self.dispatch(' '.join(words)),(False,'none',0))
        for token in recipe.split():
            with self.subTest(duplicate=token):self.assertEqual(self.dispatch(recipe+' '+token),(False,'none',0))

    def test_reconnect5_foreign_owner_refusals(self):
        recipe=self.recipe(140)
        for token in ('srvros.wifi-http5=1','srvros.http-child-preflight=1','srvros.wifi-stream5-dhcp=1',
                      'srvros.wifi-provider5=1','srvros.wifi-wpa5=1','srvros.wifi-data5=1',
                      'srvros.wifi-reuse5=1','srvros.wifi-nop5=1','srvros.wifi-desktop-trial=1',
                      'srvros.wifi-scan=1','srvros.wifi-band2g=1','srvros.desktop=1',
                      'srvros.capture=wifi','srvros.storage=usb'):
            with self.subTest(extra=token):
                self.assertEqual(self.dispatch(recipe+' '+token),(False,'none',0))
                self.assertEqual(self.library.dispatch_reconnect_attempted(),0)
                self.assertEqual(self.library.dispatch_network_probe(),0)
        self.assertEqual(self.dispatch(self.recipe(139)),(True,'xhci_wifi_capture_http5',1))
        self.assertEqual(self.library.dispatch_network_probe(),1)

    def test_exact_nop_token_and_whitespace(self):
        cmdline=self.recipe(126)+' srvros.wifi-nop5=10'
        self.assertEqual(self.dispatch(cmdline),(True,'xhci_wifi_capture_tx5',1))
        cmdline='\t\r\n'.join(self.recipe(134).split())
        self.assertEqual(self.dispatch(cmdline),(True,'xhci_wifi_capture_nop5',1))

if __name__=='__main__':unittest.main()
