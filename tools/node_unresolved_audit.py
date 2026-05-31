#!/usr/bin/env python3
import argparse
import re
from collections import Counter, defaultdict
from pathlib import Path


BUCKETS = [
    ("host-libstdc++", lambda s: s.startswith("std::__cxx11::") or s.startswith("std::__detail::") or
        s.startswith("std::_Rb_tree") or s.startswith("std::__basic_file") or
        s.startswith("std::basic_filebuf") or s.startswith("std::basic_ios") or
        s.startswith("std::basic_istream") or s.startswith("std::basic_ostream") or
        s.startswith("std::ctype<") or s.startswith("std::ios_base") or
        s.startswith("std::istream") or s.startswith("std::locale") or
        s.startswith("std::ostream") or s.startswith("vtable for std::") or
        s.startswith("VTT for std::") or s.startswith("std::cerr") or
        s.startswith("std::cout") or s.startswith("std::chrono::_V2") or
        s.startswith("std::_Sp_make_shared_tag")),
    ("runtime-shim", lambda s: s.startswith("std::__throw_") or
        s.startswith("std::_Hash_bytes") or
        s.startswith("std::thread::hardware_concurrency")),
    ("libc++", lambda s: "std::_LIBCPP" in s or "std::__1" in s or "std::__ndk1" in s),
    ("v8-provider", lambda s: s.startswith("v8::") or s.startswith("T<v8::") or
        s.startswith("void v8::")),
    ("node-other", lambda s: True),
]


def bucket_for(symbol):
    for name, predicate in BUCKETS:
        if predicate(symbol):
            return name
    return "node-other"


def parse_referrers(log_path):
    current_symbol = None
    current_bucket = None
    current_source = ""
    referrers = []
    symbol_re = re.compile(r"undefined symbol: (.+)$")
    ref_re = re.compile(r">>>\s+referenced by (.+)$")
    object_re = re.compile(r">>>\s+(.+?\.o)(?::|\()")
    archive_re = re.compile(r"in archive (.+)$")

    for raw_line in log_path.read_text(errors="replace").splitlines():
        line = raw_line.strip()
        match = symbol_re.search(line)
        if match:
            current_symbol = match.group(1).strip()
            current_bucket = bucket_for(current_symbol)
            current_source = ""
            continue
        if current_symbol is None:
            continue
        match = ref_re.search(line)
        if match:
            current_source = match.group(1).strip()
            continue
        match = object_re.search(line)
        if match:
            object_name = Path(match.group(1).replace("\\", "/")).name
            if object_name.endswith(".srvros.o"):
                continue
            archive_match = archive_re.search(line)
            archive = archive_match.group(1).strip() if archive_match else ""
            referrers.append((current_bucket, current_symbol, current_source, object_name, archive))
    return referrers


def infer_object_path(source, object_name, archive, source_root):
    normalized_source = source.replace("\\\\", "/").replace("\\", "/")
    if "deps/v8/src/" in normalized_source:
        relative = normalized_source.split("deps/v8/src/", 1)[1].split(":", 1)[0]
        directory = str(Path("obj/deps/v8/src") / Path(relative).parent).replace("\\", "/")
        return f"{directory}/{object_name}"
    source_name = source.split(":", 1)[0]
    if source_name.endswith((".cc", ".cpp", ".cxx")) and source_root.exists():
        matches = list(source_root.rglob(Path(source_name).name))
        if len(matches) > 1:
            object_tokens = set(re.split(r"[^A-Za-z0-9]+", object_name))
            archive_text = archive.replace("\\", "/")

            def score(match):
                relative = match.relative_to(source_root).as_posix()
                parts = set(Path(relative).parts)
                value = 0
                if relative.startswith("deps/v8/src/"):
                    value += 8
                if relative.startswith("deps/v8/third_party/"):
                    value += 4
                if "v8_base_without_compiler" in object_name and "deps/v8/src/" in relative:
                    value += 4
                if "v8_base_without_compiler" in object_name and "deps/v8/src/compiler/" in relative:
                    value -= 4
                if "v8_compiler" in object_name and "deps/v8/src/compiler/" in relative:
                    value += 6
                if "abseil" in object_name and "abseil-cpp" in relative:
                    value += 6
                if "libabseil" in archive_text and "abseil-cpp" in relative:
                    value += 6
                value += len(object_tokens.intersection(parts))
                return value

            best = max(matches, key=score)
            if score(best) > 0:
                matches = [best]
        if len(matches) == 1:
            relative = matches[0].relative_to(source_root).as_posix()
            if relative.startswith("deps/v8/src/"):
                directory = str(Path("obj/deps/v8/src") / Path(relative).relative_to("deps/v8/src").parent).replace("\\", "/")
                return f"{directory}/{object_name}"
            if relative.startswith("src/"):
                directory = str(Path("obj/src") / Path(relative).relative_to("src").parent).replace("\\", "/")
                return f"{directory}/{object_name}"
            if relative.startswith("deps/"):
                directory = str(Path("obj") / Path(relative).parent).replace("\\", "/")
                return f"{directory}/{object_name}"
    if "deps/ada/" in archive.replace("\\", "/") or object_name.startswith("ada."):
        return f"obj/deps/ada/{object_name}"
    if "deps/merve/" in archive.replace("\\", "/") or object_name.startswith("merve."):
        return f"obj/deps/merve/{object_name}"
    if "src/" in source and "node" in object_name:
        relative = source.split("src/", 1)[1]
        directory = str(Path("obj/src") / Path(relative).parent).replace("\\", "/")
        return f"{directory}/{object_name}"
    return ""


def infer_provider_object(symbol):
    if "v8::internal::TorqueGenerated" in symbol and "Print(" in symbol:
        return "obj/tools/v8_gypfiles/gen/torque-generated/v8_base_without_compiler.objects-printer.o"
    provider_map = {
        "v8::internal::BigIntBase::BigIntBasePrint": "obj/deps/v8/src/objects/v8_base_without_compiler.bigint.o",
        "v8::internal::BigInt::BigIntShortPrint": "obj/deps/v8/src/objects/v8_base_without_compiler.bigint.o",
        "v8::internal::BytecodeArray::Disassemble": "obj/deps/v8/src/objects/v8_base_without_compiler.bytecode-array.o",
        "v8::internal::Code::Disassemble": "obj/deps/v8/src/diagnostics/v8_base_without_compiler.disassembler.o",
        "v8::internal::String::PrintUC16": "obj/deps/v8/src/objects/v8_base_without_compiler.string.o",
        "v8::internal::String::ToNumber": "obj/deps/v8/src/objects/v8_base_without_compiler.string.o",
        "v8::internal::String::SlowShare": "obj/deps/v8/src/objects/v8_base_without_compiler.string.o",
        "v8::internal::SourcePosition::Print": "obj/deps/v8/src/codegen/v8_base_without_compiler.source-position.o",
        "v8::internal::compiler::turboshaft::Type::PrintTo": "obj/deps/v8/src/compiler/turboshaft/v8_compiler.types.o",
        "v8::internal::compiler::turboshaft::operator<<": "obj/deps/v8/src/compiler/turboshaft/v8_compiler.graph.o",
        "v8::internal::compiler::Node::Print": "obj/deps/v8/src/compiler/v8_compiler.node.o",
        "v8::internal::compiler::operator<<": "obj/deps/v8/src/compiler/v8_compiler.heap-refs.o",
        "v8::internal::JSReceiver::ToPrimitive": "obj/deps/v8/src/objects/v8_base_without_compiler.js-objects.o",
        "v8::internal::FixedArray::RightTrimOrEmpty": "obj/deps/v8/src/objects/v8_base_without_compiler.fixed-array.o",
        "v8::internal::OrderedHashMap::Rehash": "obj/deps/v8/src/objects/v8_base_without_compiler.ordered-hash-table.o",
        "v8::internal::OrderedHashSet::Rehash": "obj/deps/v8/src/objects/v8_base_without_compiler.ordered-hash-table.o",
        "v8::internal::SwissNameDictionary": "obj/tools/v8_gypfiles/gen/torque-generated/src/objects/v8_base_without_compiler.swiss-name-dictionary-tq.o",
    }
    for prefix, object_path in provider_map.items():
        if symbol.startswith(prefix):
            return object_path
    return ""


def main():
    parser = argparse.ArgumentParser(description="Bucket srvros Node link unresolved symbols.")
    parser.add_argument("path", nargs="?", default="build/node-srvros-link-probe/unresolved-symbols.txt")
    parser.add_argument("--show", type=int, default=8, help="example symbols to show per bucket")
    parser.add_argument("--fail-runtime", action="store_true",
        help="fail if runtime-shim bucket still has unresolved symbols")
    parser.add_argument("--log", default="build/node-srvros-link-probe/node-srvros-link.stderr.log")
    parser.add_argument("--referrers", action="store_true", help="rank referrer objects by unresolved-symbol bucket")
    parser.add_argument("--write-object-list", default="", help="write inferred object paths for top referrers")
    parser.add_argument("--top", type=int, default=20, help="number of top referrers to include")
    parser.add_argument("--source-root", default="ports/upstream/node")
    args = parser.parse_args()

    path = Path(args.path)
    if path.exists():
        symbols = [line.strip() for line in path.read_text().splitlines() if line.strip()]
    else:
        symbols = []
    buckets = {name: [] for name, _ in BUCKETS}
    for symbol in symbols:
        buckets[bucket_for(symbol)].append(symbol)

    print(f"node-unresolved-audit: total {len(symbols)}")
    for name, _ in BUCKETS:
        values = buckets[name]
        print(f"{name}: {len(values)}")
        for symbol in values[:args.show]:
            print(f"  {symbol}")
        if len(values) > args.show:
            print(f"  ... {len(values) - args.show} more")

    if args.fail_runtime and buckets["runtime-shim"]:
        return 2
    if args.referrers or args.write_object_list:
        log_path = Path(args.log)
        refs = parse_referrers(log_path)
        counts = Counter((bucket, object_name, archive) for bucket, _, _, object_name, archive in refs)
        print("node-unresolved-audit: top referrers")
        inferred = []
        provider_inferred = []
        source_root = Path(args.source_root)
        for (bucket, object_name, archive), count in counts.most_common(args.top):
            examples = [r for r in refs if r[0] == bucket and r[3] == object_name and r[4] == archive]
            source = examples[0][2] if examples else ""
            object_path = infer_object_path(source, object_name, archive, source_root)
            if object_path:
                inferred.append(object_path)
            print(f"{count:4d} {bucket:15s} {object_name}")
            if object_path:
                print(f"     object: {object_path}")
            if source:
                print(f"     source: {source}")
        for symbol in symbols:
            object_path = infer_provider_object(symbol)
            if object_path:
                provider_inferred.append(object_path)
        if provider_inferred:
            print("node-unresolved-audit: inferred provider objects")
            for object_path, count in Counter(provider_inferred).most_common(args.top):
                inferred.append(object_path)
                print(f"{count:4d} {object_path}")
        if args.write_object_list:
            values = list(dict.fromkeys(inferred))
            Path(args.write_object_list).write_text("\n".join(values) + ("\n" if values else ""))
            print(f"node-unresolved-audit: wrote {len(values)} inferred objects to {args.write_object_list}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
