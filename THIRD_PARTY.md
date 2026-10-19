# Third-party materials

The root [MIT license](LICENSE) applies to original SrvrOS code. It does not
replace the notices carried by upstream components, fonts, bootloader or host
tools. Keep those notices with redistributions. This index points to the
licenses in the checked-out sources; initialize submodules before inspecting
submodule files.

| Component | Location / notice | Role |
| --- | --- | --- |
| uACPI | [MIT notice](ports/upstream/uacpi/LICENSE), [port notes](ports/srvros/uacpi/PORT.md) | Restricted probe and separate VM runtime |
| zlib / minizip | [zlib notice](ports/upstream/zlib/LICENSE), [minizip notice](ports/upstream/zlib/contrib/minizip/LICENSE.Info-Zip) and source headers | Compression and archive tools |
| Lua | [notice in lua.h](ports/upstream/lua/lua.h) | Lua source port |
| cJSON | [MIT notice](ports/upstream/cjson/LICENSE) | JSON source port |
| inih | [BSD notice](ports/upstream/inih/LICENSE.txt) | Configuration parser |
| linenoise | [BSD notice](ports/upstream/linenoise/LICENSE) | Line-editing source/adapter |
| libuv | [license](ports/upstream/libuv/LICENSE), [additional notices](ports/upstream/libuv/LICENSE-extra) | Selected port and compatibility work |
| SQLite | [source notice](ports/upstream/sqlite/sqlite3.h), [provenance](ports/upstream/sqlite/README.srvros) | SQLite port; upstream copyright disclaimer in source |
| Berkeley Yacc | [upstream statement](ports/upstream/byacc/LICENSE) | Parser generator; upstream public-domain statement |
| stb_image | [provenance and local patch](ports/upstream/stb/README.srvros.md), [license in source](ports/upstream/stb/stb_image.h) | PNG/JPEG/BMP worker; MIT option |
| DejaVu fonts | [license](userspace/lib/assets/fonts/LICENSE-DejaVu.txt), [font provenance](userspace/lib/assets/fonts/README.md) | Native desktop and terminal fonts |
| Node.js | [upstream license and bundled notices](ports/upstream/node/LICENSE), [recovery status](docs/development.md#node-port-recovery) | Optional incomplete runtime port, not part of the default build |

Submodule identities are pinned by Git; downloaded boot/build tool identities
are pinned in the Makefile. Limine binaries carry their own BSD-style notice in
the downloaded tree under `build/limine-binary/`. Zig, QEMU, OVMF, Python/Pillow
and other host tools retain their own distribution terms and are not relicensed
by SrvrOS. A source checkout and a redistributable binary bundle are different
artifacts: include the applicable original notices when preparing a binary release.

The [screenshot gallery](docs/screenshots.md) uses native guest captures,
synthetic documents and a generated color fixture. Decoder test images also have
[local provenance](tools/fixtures/images/README.md). Hardware inventories, personal
photographs, firmware dumps and credentials are not public documentation assets.
