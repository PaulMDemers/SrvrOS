#!/usr/bin/env python3
"""Compile a Make-declared host fixture with reusable, content-checked objects.

The canonical Make recipe remains the source of compiler flags and source lists.
Only ordinary C compile/link recipes are accepted. No Make output is shell-executed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import time
import contextlib

from workflow.store import atomic, encoded, executable, lock, sha, process_environment

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CACHE = Path.home() / '.cache/srvros/fixture-objects'


def recipes(targets, root=ROOT, env=None):
    result = subprocess.check_output(['make', '-Bn', '--', *targets], cwd=root, text=True, env=env)
    matches = {target: [] for target in targets}
    for line in result.splitlines():
        words = shlex.split(line)
        if '-std=c11' in words and len(words) > 3 and words[-2] == '-o' and words[-1] in matches:
            matches[words[-1]].append(words)
    declarations = {}
    for target, commands in matches.items():
        if len(commands) != 1:
            raise ValueError('Expected one C fixture recipe for ' + target)
        words = commands[0]
        sources = [word for word in words[1:-2] if word.endswith('.c')]
        flags = [word for word in words[1:-2] if not word.endswith('.c')]
        if not sources or any(word in flags for word in ('-c', '-o', '&&', ';', '|', '>', '<')):
            raise ValueError('Unsupported compile/link recipe')
        declarations[target] = (words[0], flags, sources)
    return declarations


def recipe(target, root=ROOT, env=None):
    return recipes([target], root, env)[target]


def dependencies(compiler, flags, source, root, env):
    output = subprocess.check_output([compiler, *flags, '-M', '-MT', 'fixture', source], cwd=root, text=True, env=env)
    names = shlex.split(output.replace('\\\n', '').split(':', 1)[1])
    return {str((Path(root) / name).resolve()): sha(Path(root) / name) for name in names}


def compile_fixture(target, *, cache=DEFAULT_CACHE, replacements=(), root=ROOT, output=None, declaration=None):
    root, cache = Path(root).resolve(), Path(cache).resolve()
    env = process_environment()
    compiler, flags, sources = declaration or recipe(target, root, env)
    replace = dict(item.split('=', 1) for item in replacements)
    if set(replace) - set(sources):
        raise ValueError('Replacement does not name a recipe translation unit')
    tool = executable([compiler], env)
    # Include the compiler's selected assembler and compiler backend, not just cc.
    helpers = {}
    for name in ('cc1', 'as', 'collect2', 'ld'):
        value = subprocess.check_output([compiler, '-print-prog-name=' + name], text=True, env=env).strip()
        path = Path(value) if '/' in value else Path(shutil.which(value) or value)
        if not path.is_file():
            raise ValueError('Cannot fingerprint compiler helper: ' + value)
        helpers[name] = dict(path=str(path.resolve()), sha256=sha(path))
    identity = dict(version=1, implementation=sha(__file__), tool=tool, helpers=helpers,
                    flags=flags, environment_sha256=hashlib.sha256(encoded(env)).hexdigest(), cwd=str(root))
    cache.mkdir(parents=True, exist_ok=True)
    start = time.monotonic()
    objects, hits = [], 0
    for original in sources:
        source = replace.get(original, original)
        unit_flags = flags + (['-iquote', str((root / original).parent)] if original in replace else [])
        before = dependencies(compiler, unit_flags, source, root, env)
        description = dict(identity=identity, source=source, unit_flags=unit_flags, dependencies=before)
        key = hashlib.sha256(encoded(description)).hexdigest()
        entry = cache / key
        with lock(cache / (key + '.lock')):
            obj, receipt = entry / 'unit.o', entry / 'receipt.json'
            saved = json.loads(receipt.read_text()) if receipt.is_file() else {}
            valid = (obj.is_file() and saved.get('description') == description
                     and saved.get('sha256') == sha(obj))
            if valid:
                hits += 1
            else:
                entry.mkdir(exist_ok=True)
                temporary = entry / 'unit.tmp.o'
                subprocess.run([compiler, *unit_flags, '-c', source, '-o', str(temporary)], cwd=root, env=env, check=True)
                if dependencies(compiler, unit_flags, source, root, env) != before:
                    temporary.unlink(missing_ok=True)
                    raise ValueError('Compiler dependencies changed: ' + source)
                temporary.replace(obj)
                with obj.open('rb') as stream:
                    os.fsync(stream.fileno())
                atomic(receipt, dict(description=description, sha256=sha(obj)))
            objects.append(obj)
    destination = (root / (output or target)).resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Link into a temporary output so an interrupted link cannot replace a valid binary.
    temporary = destination.with_name(destination.name + '.linking')
    subprocess.run([compiler, *flags, *(str(path) for path in objects), '-o', str(temporary)], cwd=root, env=env, check=True)
    temporary.replace(destination)
    result = dict(target=target, output=str(destination), sha256=sha(destination),
                  objects=len(objects), reused=hits, compiled=len(objects)-hits,
                  seconds=time.monotonic()-start, cache=str(cache))
    atomic(destination.with_name(destination.name + '.compile.json'), result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', nargs='+')
    parser.add_argument('--cache', type=Path, default=DEFAULT_CACHE)
    parser.add_argument('--source', action='append', default=[], help='replace a translation unit: original.c=mutation.c')
    parser.add_argument('--output')
    parser.add_argument('--under-workflow-lock', action='store_true',
                        help='caller already holds build/device-workflow.lock')
    args = parser.parse_args()
    try:
        # Cooperates with device_workflow and every fast fixture invocation.
        if len(args.target) > 1 and (args.output or args.source):
            raise ValueError('Output/source overrides require one target')
        with contextlib.nullcontext() if args.under_workflow_lock else lock(ROOT / 'build/device-workflow.lock'):
            declarations = recipes(args.target, ROOT, process_environment())
            results = [compile_fixture(target, cache=args.cache, replacements=args.source,
                                       output=args.output, declaration=declarations[target]) for target in args.target]
            print(json.dumps(results[0] if len(results) == 1 else results, indent=2))
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'fixture-compile: {error}\n')


if __name__ == '__main__':
    main()
