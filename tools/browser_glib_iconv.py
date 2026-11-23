#!/usr/bin/env python3
"""Build pinned SrvrOS dependencies and run one target GLib configure."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile

from browser_storage import StorageGuard
from signal_public_guest import sdk_headers

ROOT = Path(__file__).resolve().parents[1]
ICONV = 'libiconv-1.19'
GLIB = 'glib-2.90.1'
PCRE2 = 'pcre2-10.49'
LIBFFI = 'libffi-3.8.0'
ZLIB = 'zlib-1.4'
PROXY_INTL = 'proxy-libintl-0.5'
PINS = {
    ICONV: ('https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.19.tar.gz',
            '88dd96a8c0464eca144fc791ae60cd31cd8ee78321e67397e25fc095c4a19aa6'),
    GLIB: ('https://download.gnome.org/sources/glib/2.90/glib-2.90.1.tar.xz',
           '93c941aa17d5eb1d53fe838365f29a8b4e539c222a256d974ec8f30fc413e396'),
    PCRE2: ('https://github.com/PCRE2Project/pcre2/releases/download/pcre2-10.49/pcre2-10.49.tar.bz2',
            '53c156e1ba416a20da8e65395daa132da0d80e76910424caca3fcdae7831d384'),
    LIBFFI: ('https://github.com/libffi/libffi/releases/download/v3.8.0/libffi-3.8.0.tar.gz',
             '7da3e2d9a171eb0a038f592ecad3ff2bb2550f3496d87b3b29ad0cf4430c0db4'),
    ZLIB: ('https://github.com/madler/zlib/releases/download/v1.4/zlib-1.4.tar.gz',
           '02dd159783677a749c04adbdd02b55b8a2bcc7e054f51b76b549199a916ad584'),
    PROXY_INTL: ('https://github.com/frida/proxy-libintl/archive/refs/tags/0.5.tar.gz',
                 'f7a1cbd7579baaf575c66f9d99fb6295e9b0684a28b095967cfda17857595303'),
}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def extract_archive(path, name, run):
    if digest(path) != PINS[name][1]:
        raise ValueError(f'{name} archive checksum mismatch')
    with tarfile.open(path) as archive:
        archive.extractall(run, filter='data')
    return run / name


def run_logged(args, cwd, env, log, timeout, *, check=True):
    with log.open('xb') as stream:
        return subprocess.run(args, cwd=cwd, env=env, stdout=stream, stderr=stream,
                              timeout=timeout, check=check).returncode


def stage_provider(prefix, files):
    for local, destination in files:
        target = prefix / destination
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(local, target)
    return prefix


def build_pcre2(source, out, gnu_target, tools, make):
    build = out / 'pcre2-build'
    build.mkdir()
    prefix = out / 'target-pcre2'
    configure = [source / 'configure', *gnu_target, f'--prefix={prefix}',
                 '--libdir=' + str(prefix / 'lib'),
                 '--enable-pcre2-8', '--disable-pcre2-16', '--disable-pcre2-32',
                 '--enable-unicode', '--disable-jit', '--disable-dependency-tracking']
    run_logged(configure, build, tools, out / 'pcre2-configure.log', 300)
    run_logged([make, '-j2', 'libpcre2-8.la'], build, tools, out / 'pcre2-build.log', 180)
    return stage_provider(prefix, [
        (build / 'src/pcre2.h', 'include/pcre2.h'),
        (build / '.libs/libpcre2-8.a', 'lib/libpcre2-8.a'),
        (build / 'libpcre2-8.pc', 'lib/pkgconfig/libpcre2-8.pc'),
        (source / 'LICENCE.md', 'share/LICENCE.md'),
    ])


def build_libffi(source, out, gnu_target, tools, make, patch):
    run_logged(['patch', '--batch', '-p1', '-i', patch], source, tools,
               out / 'libffi-patch.log', 15)
    build = out / 'libffi-build'
    build.mkdir()
    prefix = out / 'target-libffi'
    configure = [source / 'configure', *gnu_target, f'--prefix={prefix}',
                 '--disable-docs', '--disable-multi-os-directory',
                 '--disable-exec-static-tramp']
    run_logged(configure, build, tools, out / 'libffi-configure.log', 300)
    run_logged([make, '-j2', 'libffi.la'], build, tools, out / 'libffi-build.log', 180)
    return stage_provider(prefix, [
        (build / 'include/ffi.h', 'include/ffi.h'),
        (build / 'include/ffitarget.h', 'include/ffitarget.h'),
        (build / '.libs/libffi.a', 'lib/libffi.a'),
        (build / 'libffi.pc', 'lib/pkgconfig/libffi.pc'),
        (source / 'LICENSE', 'share/LICENSE'),
    ])


def build_zlib(source, out, tools, make):
    build = out / 'zlib-build'
    build.mkdir()
    prefix = out / 'target-zlib'
    zlib_tools = dict(tools, CHOST='x86_64-srvros')
    configure = [source / 'configure', '--static', f'--prefix={prefix}']
    run_logged(configure, build, zlib_tools, out / 'zlib-configure.log', 120)
    run_logged([make, '-j2', 'libz.a'], build, zlib_tools, out / 'zlib-build.log', 120)
    return stage_provider(prefix, [
        (source / 'zlib.h', 'include/zlib.h'),
        (build / 'zconf.h', 'include/zconf.h'),
        (build / 'libz.a', 'lib/libz.a'),
        (build / 'zlib.pc', 'lib/pkgconfig/zlib.pc'),
        (source / 'LICENSE', 'share/LICENSE'),
    ])


def build_proxy_intl(source, out, meson, cross, env):
    build = out / 'proxy-intl-build'
    run_logged([meson, 'setup', build, source, '--cross-file', cross,
                '--wrap-mode=nodownload', '--default-library=static'],
               out, env, out / 'proxy-intl-configure.log', 60)
    run_logged([meson, 'compile', '-C', build, 'intl'],
               out, env, out / 'proxy-intl-build.log', 60)
    return stage_provider(out / 'target-proxy-intl', [
        (source / 'libintl.h', 'include/libintl.h'),
        (build / 'libintl.a', 'lib/libintl.a'),
        (source / 'COPYING', 'share/COPYING'),
    ])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iconv-archive', type=Path, required=True)
    parser.add_argument('--glib-archive', type=Path, required=True)
    parser.add_argument('--pcre2-archive', type=Path, help='also build and expose the pinned PCRE2 8-bit port')
    parser.add_argument('--libffi-archive', type=Path, help='also build and expose the pinned libffi port')
    parser.add_argument('--zlib-archive', type=Path, help='also build and expose the pinned zlib port')
    parser.add_argument('--proxy-intl-archive', type=Path, help='build and supply the pinned GLib intl fallback')
    parser.add_argument('--meson', type=Path, required=True)
    args = parser.parse_args()
    base = ROOT / 'build/browser-glib-iconv'
    base.mkdir(parents=True, exist_ok=True)
    StorageGuard(base).preflight()
    out = Path(tempfile.mkdtemp(prefix='run-', dir=base))
    print(f'GLib/iconv evidence: {out}', flush=True)

    compiler = ROOT / 'ports/srvros/webkit/cc'
    zig = ROOT / 'build/tooling/zig/zig'
    sdk = ROOT / 'build/sysroot/srvros'
    glib_patches = (
        (ROOT / 'tools/glib-srvros-poll.patch', 'glib-patch.log'),
        (ROOT / 'tools/glib-srvros-unix-watch.patch', 'glib-unix-watch-patch.log'),
        (ROOT / 'tools/glib-srvros-unix-abi.patch', 'glib-unix-abi-patch.log'),
        (ROOT / 'tools/glib-srvros-file-monitor.patch', 'glib-file-monitor-patch.log'),
        (ROOT / 'tools/glib-srvros-ip-tos.patch', 'glib-ip-tos-patch.log'),
        (ROOT / 'tools/glib-srvros-ipv6-tclass.patch', 'glib-ipv6-tclass-patch.log'),
        (ROOT / 'tools/glib-srvros-resolver-address.patch', 'glib-resolver-address-patch.log'),
        (ROOT / 'tools/glib-srvros-socket-probes.patch', 'glib-socket-probes-patch.log'),
    )
    meson = args.meson.resolve()
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('PKG_CONFIG', 'MESON', 'ac_cv_', 'gl_cv_', 'ZIG_'))
           and key not in ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS',
                           'LIBS', 'CPATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH', 'SDKROOT',
                           'CONFIG_SITE', 'MAKEFLAGS', 'MFLAGS')}
    env.update({'CONFIG_SITE': '/dev/null', 'LC_ALL': 'C', 'PYTHONUNBUFFERED': '1'})
    dependencies = [(PCRE2, args.pcre2_archive), (LIBFFI, args.libffi_archive),
                    (ZLIB, args.zlib_archive), (PROXY_INTL, args.proxy_intl_archive)]
    pins = [ICONV, GLIB, *(name for name, archive in dependencies if archive)]
    receipt = {'result': 'failed', 'pins': {name: PINS[name] for name in pins}}
    try:
        receipt['source'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        receipt['inputs_sha256'] = {str(path.relative_to(ROOT)): digest(path) for path in (
            Path(__file__), compiler, compiler.with_name('compiler.py'), zig,
            sdk / 'lib/libsrvros.a', sdk / 'lib/crt0.o', sdk / 'lib/app.ld')}
        receipt['sdk_headers_sha256'] = sdk_headers(sdk)
        libffi_patch = ROOT / 'tools/libffi-srvros-closures.patch'
        if args.libffi_archive:
            libffi_patch_copy = out / libffi_patch.name
            shutil.copyfile(libffi_patch, libffi_patch_copy)
            receipt['inputs_sha256'][str(libffi_patch.relative_to(ROOT))] = digest(libffi_patch_copy)
        receipt['meson'] = subprocess.check_output([meson, '--version'], env=env, text=True, timeout=15).strip()
        iconv = extract_archive(args.iconv_archive.resolve(), ICONV, out)
        glib = extract_archive(args.glib_archive.resolve(), GLIB, out)
        for patch, log_name in glib_patches:
            patch_copy = out / patch.name
            shutil.copyfile(patch, patch_copy)
            receipt['inputs_sha256'][str(patch.relative_to(ROOT))] = digest(patch_copy)
            run_logged(['patch', '--batch', '-p0', '-F0', '-i', patch_copy],
                       glib, env, out / log_name, 15)
        build = out / 'iconv-build'
        build.mkdir()
        # This is the generic target accepted by GNU config.sub. The compiler
        # still links executable checks with the actual SrvrOS CRT and libc.
        host_cpu = {'arm64': 'aarch64'}.get(platform.machine(), platform.machine())
        build_triplet = f'{host_cpu}-apple-darwin' if platform.system() == 'Darwin' else f'{host_cpu}-pc-linux-gnu'
        gnu_target = [f'--build={build_triplet}', '--host=x86_64-unknown-none',
                      '--disable-shared', '--enable-static']
        tools = dict(env, CC=str(compiler), CFLAGS='-O2', AR=f'{zig} ar', RANLIB=f'{zig} ranlib')
        # These SDK calls link, but do not provide the multibyte locale
        # semantics required by libiconv's WCHAR_T fallback.
        iconv_tools = dict(tools, ac_cv_func_mbrtowc='no', ac_cv_func_wcrtomb='no')
        configure = [iconv / 'configure', *gnu_target, '--disable-nls', '--prefix=/srvros-iconv']
        run_logged(configure, build, iconv_tools, out / 'iconv-configure.log', 300)
        make = shutil.which('gmake') or shutil.which('make')
        run_logged([make, '-j2', 'lib/localcharset.h'], build, iconv_tools, out / 'iconv-charset.log', 120)
        run_logged([make, '-j2', '-C', 'lib'], build, iconv_tools, out / 'iconv-build.log', 120)

        prefix = out / 'target-iconv'
        for kind, source in [('include', build / 'include/iconv.h.inst'),
                             ('lib', build / 'lib/.libs/libiconv.a'),
                             ('share', iconv / 'COPYING.LIB')]:
            target = prefix / kind / source.name.removesuffix('.inst')
            target.parent.mkdir(parents=True)
            shutil.copyfile(source, target)
        receipt['provider_sha256'] = {str(path.relative_to(out)): digest(path)
                                      for path in sorted(prefix.rglob('*')) if path.is_file()}

        pc_dirs = []
        if args.pcre2_archive:
            pcre2 = extract_archive(args.pcre2_archive.resolve(), PCRE2, out)
            pcre2_prefix = build_pcre2(pcre2, out, gnu_target, tools, make)
            receipt['pcre2_sha256'] = {str(path.relative_to(out)): digest(path)
                                      for path in sorted(pcre2_prefix.rglob('*')) if path.is_file()}
            pc_dirs.append(str(pcre2_prefix / 'lib/pkgconfig'))
            print(f'Target PCRE2 prefix: {pcre2_prefix}', flush=True)
        if args.libffi_archive:
            libffi = extract_archive(args.libffi_archive.resolve(), LIBFFI, out)
            ffi_prefix = build_libffi(libffi, out, gnu_target, tools, make, libffi_patch_copy)
            receipt['libffi_sha256'] = {str(path.relative_to(out)): digest(path)
                                       for path in sorted(ffi_prefix.rglob('*')) if path.is_file()}
            pc_dirs.append(str(ffi_prefix / 'lib/pkgconfig'))
            print(f'Target libffi prefix: {ffi_prefix}', flush=True)
        if args.zlib_archive:
            zlib = extract_archive(args.zlib_archive.resolve(), ZLIB, out)
            zlib_prefix = build_zlib(zlib, out, tools, make)
            receipt['zlib_sha256'] = {str(path.relative_to(out)): digest(path)
                                     for path in sorted(zlib_prefix.rglob('*')) if path.is_file()}
            pc_dirs.append(str(zlib_prefix / 'lib/pkgconfig'))
            print(f'Target zlib prefix: {zlib_prefix}', flush=True)
        if not pc_dirs:
            pc = out / 'empty-pkgconfig'
            pc.mkdir()
            pc_dirs.append(str(pc))
        iconv_c_args = ['-I' + str(prefix / 'include')]
        cross = out / 'srvros.ini'
        cross.write_text(f'''[binaries]
c = {str(compiler)!r}
ar = [{str(zig)!r}, 'ar']
pkg-config = {shutil.which('pkg-config')!r}

[host_machine]
system = 'srvros'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'

[properties]
needs_exe_wrapper = true
pkg_config_libdir = {pc_dirs!r}

[built-in options]
c_args = {iconv_c_args!r}
c_link_args = [{'-L' + str(prefix / 'lib')!r}]
''')
        if args.proxy_intl_archive:
            # Keep this in GLib's wrap directory so it is selected as the
            # internal dependency. The isolated build tests the same source.
            proxy = extract_archive(args.proxy_intl_archive.resolve(), PROXY_INTL, glib / 'subprojects')
            proxy_prefix = build_proxy_intl(proxy, out, meson, cross, env)
            receipt['proxy_intl_sha256'] = {str(path.relative_to(out)): digest(path)
                                          for path in sorted(proxy_prefix.rglob('*')) if path.is_file()}
            print(f'Target proxy-libintl prefix: {proxy_prefix}', flush=True)
        # Zig's freestanding limits.h does not include the SDK's POSIX limits.
        # Supply the SDK header to GLib's C build, where hostname limits are used.
        glib_c_args = [*iconv_c_args, '-include', str(sdk / 'include/limits.h')]
        receipt['glib_c_args'] = glib_c_args
        meson_args = [meson, 'setup', out / 'glib-build', glib, '--cross-file', cross,
                      f'-Dc_args={glib_c_args!r}',
                      '--wrap-mode=nodownload', '--default-library=static', '-Dtests=false',
                      '-Dintrospection=disabled', '-Dnls=disabled', '-Dlibmount=disabled',
                      '-Dselinux=disabled', '-Dxattr=false', '-Dsysprof=disabled']
        status = run_logged(meson_args, out, env, out / 'glib-configure.log', 300, check=False)
        receipt.update(result='configured' if status == 0 else 'configure stopped', glib_exit=status)
        print(f'Target iconv prefix: {prefix}\nGLib configure exit {status}')
        return status
    except Exception as error:
        receipt['error'] = str(error)
        raise
    finally:
        logs = sorted(out.glob('*.log'))
        meson_log = out / 'glib-build/meson-logs/meson-log.txt'
        if meson_log.is_file():
            logs.append(meson_log)
        proxy_log = out / 'proxy-intl-build/meson-logs/meson-log.txt'
        if proxy_log.is_file():
            logs.append(proxy_log)
        receipt['logs_sha256'] = {str(log.relative_to(out)): digest(log) for log in logs}
        (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')


if __name__ == '__main__':
    raise SystemExit(main())
