#!/usr/bin/env python3
import argparse
import os
import random
import shutil
import socket
import subprocess
import sys
import tempfile
import time


def hidden_startup_flags():
    if os.name != "nt":
        return None, 0
    startupinfo = subprocess.STARTUPINFO()
    startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startupinfo.wShowWindow = 0
    return startupinfo, getattr(subprocess, "CREATE_NO_WINDOW", 0)


def read_for(sock, seconds):
    chunks = []
    deadline = time.time() + seconds
    while time.time() < deadline:
        try:
            chunk = sock.recv(4096)
            if not chunk:
                break
            chunks.append(chunk)
        except socket.timeout:
            pass
        except (ConnectionResetError, OSError):
            break
    return b"".join(chunks)


def read_until(sock, marker, seconds):
    data = b""
    deadline = time.time() + seconds
    while marker not in data and time.time() < deadline:
        data += read_for(sock, 0.5)
    return data


def connect_serial(port, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=1)
        except OSError:
            time.sleep(0.2)
    raise RuntimeError("serial connection failed")


def run_monitor_command(sock, command, expect, timeout):
    sock.sendall((command + "\n").encode("utf-8"))
    data = read_until(sock, expect.encode("utf-8"), timeout)
    if expect.encode("utf-8") not in data:
        raise RuntimeError(f"missing expected output {expect!r}")
    data += read_until(sock, b"srv> ", timeout)
    return data


def write_text(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text.strip() + "\n")


CORE_APP = r"""
const fs = require('fs');
const path = require('path');
const { URL } = require('url');
const events = require('events');
const querystring = require('querystring');
const crypto = require('crypto');

const configPath = '/fat/app-config.json';
fs.writeFileSync(configPath, JSON.stringify({ name: 'srvros', count: 3 }), 'utf8');
const config = JSON.parse(fs.readFileSync(configPath, 'utf8'));
const outPath = path.join('/fat', 'sync-output.txt');
setTimeout(() => {
  fs.writeFileSync(outPath, 'ready-' + config.name, 'utf8');
  const syncText = fs.readFileSync(outPath, 'utf8');
  const u = new URL('http://example.test/demo?x=42&name=srvros');
  const parsed = querystring.parse('a=1&b=two');
  const ee = new events.EventEmitter();
  let seen = 'none';
  ee.on('ready', (value) => { seen = value; });
  ee.emit('ready', 'event');
  const hmac = crypto.createHmac('sha256', 'key').update('data').digest('hex');
  console.log('APP-CORE', path.basename(outPath), u.searchParams.get('x'), parsed.b, seen, hmac.slice(0, 8), syncText);
}, 5);
"""


SQLITE_APP = r"""
const { DatabaseSync } = require('node:sqlite');
const db = new DatabaseSync('/fat/app-suite.sqlite');
db.exec('CREATE TABLE IF NOT EXISTS users(id INTEGER, name TEXT, role TEXT); DELETE FROM users;');
let first = db.prepare('INSERT INTO users(name, role) VALUES (:name, :role)').run({ name: 'Ada', role: 'admin' });
db.prepare('INSERT INTO users(name, role) VALUES (?, ?)').run('Grace', 'ops');
let changed = db.prepare('UPDATE users SET name = ? WHERE id = ?').run('Bob', first.lastInsertRowid).changes;
let rows = db.prepare('SELECT id, name FROM users ORDER BY id ASC LIMIT ?').all(2);
let count = db.prepare('SELECT COUNT(*) AS count FROM users WHERE role = :role').get({ role: 'ops' }).count;
let removed = db.prepare('DELETE FROM users WHERE name = ?').run('Grace').changes;
console.log('APP-SQLITE', rows[0].name, rows[1].name, count, changed, removed);
db.close();
"""


FSP_APP = r"""
const fsp = require('fs/promises');

(async () => {
  await fsp.mkdir('/fat/pdir', { recursive: true });
  await fsp.writeFile('/fat/pdir/a.txt', 'abc');
  const text = await fsp.readFile('/fat/pdir/a.txt', 'utf8');
  const stat = await fsp.stat('/fat/pdir/a.txt');
  const names = await fsp.readdir('/fat/pdir');
  const handle = await fsp.open('/fat/pdir/b.txt', 'w+');
  await handle.writeFile('def');
  await handle.close();
  const other = await fsp.readFile('/fat/pdir/b.txt', 'utf8');
  console.log('APP-FSP', names[0], stat.size, text, other);
})().catch((err) => {
  console.log('APP-FSP-ERR', err && err.message);
  process.exitCode = 1;
});
"""


STREAM_APP = r"""
const fs = require('fs');
const { pipeline, Readable } = require('stream');

fs.writeFileSync('/fat/stream-src.txt', 'file-stream-ok');
let readText = '';
fs.createReadStream('/fat/stream-src.txt', { encoding: 'utf8' })
  .on('data', (chunk) => { readText += chunk; })
  .on('end', () => {
    const writer = fs.createWriteStream('/fat/stream-write.txt');
    writer.write('write-');
    writer.end('stream-ok');
    writer.on('finish', () => {
      pipeline(
        fs.createReadStream('/fat/stream-write.txt'),
        fs.createWriteStream('/fat/stream-copy.txt'),
        (err) => {
          if (err) {
            console.log('APP-STREAM-ERR', err && err.message);
            process.exitCode = 1;
            return;
          }
          let from = '';
          Readable.from(['readable-', 'from-ok'])
            .on('data', (chunk) => { from += chunk.toString(); })
            .on('end', () => {
              console.log('APP-STREAM', readText,
                fs.readFileSync('/fat/stream-write.txt', 'utf8'),
                fs.readFileSync('/fat/stream-copy.txt', 'utf8'),
                from);
            });
        });
    });
  })
  .on('error', (err) => {
    console.log('APP-STREAM-ERR', err && err.message);
    process.exitCode = 1;
  });
"""


DIR_APP = r"""
const fs = require('fs');
const fsp = require('fs/promises');

function collectCallback(path) {
  return new Promise((resolve, reject) => {
    fs.opendir(path, (err, dir) => {
      if (err) {
        reject(err);
        return;
      }
      const names = [];
      function next() {
        dir.read((err, dirent) => {
          if (err) {
            reject(err);
            return;
          }
          if (dirent === null) {
            dir.close((err) => err ? reject(err) : resolve(names.sort().join(',')));
            return;
          }
          names.push(dirent.name + ':' + (dirent.isDirectory() ? 'd' : 'f'));
          next();
        });
      }
      next();
    });
  });
}

function collectSync(path) {
  const dir = fs.opendirSync(path);
  const names = [];
  while (true) {
    const dirent = dir.readSync();
    if (dirent === null)
      break;
    names.push(dirent.name + ':' + (dirent.isDirectory() ? 'd' : 'f'));
  }
  dir.closeSync();
  return names.sort().join(',');
}

async function collectAsyncIterator(dir) {
  const names = [];
  for await (const dirent of dir)
    names.push(dirent.name + ':' + (dirent.isDirectory() ? 'd' : 'f'));
  return names.sort().join(',');
}

(async () => {
  try { fs.mkdirSync('/fat/dirsuite'); } catch (err) {}
  try { fs.mkdirSync('/fat/dirsuite/sub'); } catch (err) {}
  fs.writeFileSync('/fat/dirsuite/a.txt', 'a');
  fs.writeFileSync('/fat/dirsuite/sub/b.txt', 'b');
  const typed = fs.readdirSync('/fat/dirsuite', { withFileTypes: true });
  const callbackNames = await collectCallback('/fat/dirsuite');
  const syncNames = collectSync('/fat/dirsuite');
  const promiseNames = await collectAsyncIterator(await fs.promises.opendir('/fat/dirsuite'));
  const moduleNames = await collectAsyncIterator(await fsp.opendir('/fat/dirsuite'));
  console.log('APP-DIR', callbackNames, syncNames, promiseNames, moduleNames,
    typed.some((entry) => entry.name === 'sub' && entry.isDirectory()));
})().catch((err) => {
  console.log('APP-DIR-ERR', err && err.stack || err);
  process.exitCode = 1;
});
"""


def main():
    parser = argparse.ArgumentParser(description="Run a suite of srvros Node app compatibility smokes.")
    parser.add_argument("--root", default=os.getcwd())
    parser.add_argument("--qemu", default=os.environ.get("QEMU", "qemu-system-x86_64"))
    parser.add_argument("--iso", default="build/srvros-x86_64.iso")
    parser.add_argument("--node-elf", default="build/node-srvros-runtime/node-srvros-stripped.elf")
    parser.add_argument("--memory", default="768M")
    parser.add_argument("--boot-wait", type=float, default=90)
    parser.add_argument("--runtime-wait", type=float, default=90)
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    iso = args.iso if os.path.isabs(args.iso) else os.path.join(root, args.iso)
    node_elf = args.node_elf if os.path.isabs(args.node_elf) else os.path.join(root, args.node_elf)
    if not args.skip_build:
        subprocess.check_call(["make", "node-runtime-image"], cwd=root)
    if not os.path.exists(iso):
        raise FileNotFoundError(iso)
    if not os.path.exists(node_elf):
        raise FileNotFoundError(node_elf)

    env = os.environ.copy()
    msys_ucrt = r"C:\msys64\ucrt64\bin"
    msys_usr = r"C:\msys64\usr\bin"
    if os.path.isdir(msys_ucrt):
        env["PATH"] = msys_ucrt + os.pathsep + msys_usr + os.pathsep + env.get("PATH", "")
    qemu = shutil.which(args.qemu, path=env.get("PATH")) or args.qemu

    serial_port = random.randint(24000, 29000)
    output = b""
    with tempfile.TemporaryDirectory(prefix="srvros-node-app-suite-") as temp_dir:
        core_script = os.path.join(temp_dir, "app-core.js")
        sqlite_script = os.path.join(temp_dir, "app-sqlite.js")
        fsp_script = os.path.join(temp_dir, "app-fsp.js")
        stream_script = os.path.join(temp_dir, "app-stream.js")
        dir_script = os.path.join(temp_dir, "app-dir.js")
        disk = os.path.join(temp_dir, "srvros-node.exfat")
        write_text(core_script, CORE_APP)
        write_text(sqlite_script, SQLITE_APP)
        write_text(fsp_script, FSP_APP)
        write_text(stream_script, STREAM_APP)
        write_text(dir_script, DIR_APP)
        subprocess.check_call([
            sys.executable,
            os.path.join(root, "tools", "mk_exfat_image.py"),
            disk,
            f"node={node_elf}",
            f"app-core.js={core_script}",
            f"app-sqlite.js={sqlite_script}",
            f"app-fsp.js={fsp_script}",
            f"app-stream.js={stream_script}",
            f"app-dir.js={dir_script}",
        ], cwd=root)

        command = [
            qemu,
            "-M", "q35",
            "-m", args.memory,
            "-cdrom", iso,
            "-boot", "d",
            "-serial", f"tcp:127.0.0.1:{serial_port},server,nowait",
            "-drive", f"if=none,id=exfat,file={disk},format=raw",
            "-device", "ich9-ahci,id=ahci",
            "-device", "ide-hd,drive=exfat,bus=ahci.0",
            "-monitor", "none",
            "-display", "none",
            "-no-reboot",
        ]
        startupinfo, creationflags = hidden_startup_flags()
        process = subprocess.Popen(command, cwd=root, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT,
            startupinfo=startupinfo, creationflags=creationflags)
        try:
            sock = connect_serial(serial_port, 15)
            sock.settimeout(0.3)
            output += read_until(sock, b"srv> ", args.boot_wait)
            if b"srv> " not in output:
                raise RuntimeError("monitor prompt not observed")
            output += run_monitor_command(sock, "run /fat/bin/node /fat/bin/app-core.js",
                "APP-CORE sync-output.txt 42 two event 5031fe3d ready-srvros", args.runtime_wait)
            output += run_monitor_command(sock, "run /fat/bin/node /fat/bin/app-fsp.js",
                "APP-FSP a.txt 3 abc def", args.runtime_wait)
            output += run_monitor_command(sock, "run /fat/bin/node /fat/bin/app-stream.js",
                "APP-STREAM file-stream-ok write-stream-ok write-stream-ok readable-from-ok", args.runtime_wait)
            output += run_monitor_command(sock, "run /fat/bin/node /fat/bin/app-dir.js",
                "APP-DIR a.txt:f,sub:d a.txt:f,sub:d a.txt:f,sub:d a.txt:f,sub:d true", args.runtime_wait)
            output += run_monitor_command(sock, "run /fat/bin/node /fat/bin/app-sqlite.js",
                "APP-SQLITE Bob Grace 1 1 1", args.runtime_wait)
        finally:
            try:
                process.terminate()
                process.wait(timeout=3)
            except Exception:
                process.kill()

    sys.stdout.write(output.decode("utf-8", "replace"))
    print("node-app-suite-smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
