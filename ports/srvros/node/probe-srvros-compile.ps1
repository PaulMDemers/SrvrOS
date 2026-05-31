param(
  [string]$Distro = "",
  [string]$NativePath = "~/srvros-node-probe",
  [string]$Sysroot = "build\sysroot\srvros",
  [string]$OutDir = "build\node-srvros-compile-probe",
  [string]$Objects = "",
  [string]$ObjectList = "",
  [string]$ExtraDefines = "",
  [switch]$LinuxLibuvLayout,
  [switch]$FailOnError
)

$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -Scope Global -ErrorAction SilentlyContinue) {
  $global:PSNativeCommandUseErrorActionPreference = $false
}

function Quote-RspArg([string]$Value) {
  '"' + ($Value -replace '\\', '\\' -replace '"', '\"') + '"'
}

function Invoke-CapturedProcess(
  [string]$FilePath,
  [string[]]$ArgumentList,
  [string]$StdoutPath,
  [string]$StderrPath
) {
  $process = Start-Process `
    -FilePath $FilePath `
    -ArgumentList $ArgumentList `
    -NoNewWindow `
    -Wait `
    -PassThru `
    -RedirectStandardOutput $StdoutPath `
    -RedirectStandardError $StderrPath
  return $process.ExitCode
}

function Read-LogLines([string[]]$Paths) {
  $lines = @()
  foreach ($path in $Paths) {
    if (Test-Path -LiteralPath $path) {
      $lines += Get-Content -LiteralPath $path
    }
  }
  return $lines
}

function Write-UndefinedSymbols([string]$ObjectPath, [string]$OutputPath) {
  if (!(Test-Path -LiteralPath $ObjectPath)) {
    return
  }

  $nm = (Get-Command llvm-nm -ErrorAction SilentlyContinue)
  if ($null -eq $nm) {
    $nm = (Get-Command nm -ErrorAction SilentlyContinue)
  }
  if ($null -eq $nm) {
    return
  }

  $rawSymbols = @(& $nm.Source --undefined-only --format=posix $ObjectPath 2>$null)
  $symbols = @($rawSymbols | ForEach-Object {
    $parts = $_.Trim() -split '\s+'
    if ($parts.Count -ge 2 -and $parts[1] -eq "U") {
      $parts[0]
    }
  } | Sort-Object -Unique)

  if ($symbols.Count -gt 0) {
    $symbols | Set-Content -LiteralPath $OutputPath
  } elseif (Test-Path -LiteralPath $OutputPath) {
    Remove-Item -LiteralPath $OutputPath
  }
}

function Convert-ObjectPathToName([string]$ObjectPath) {
  if ($ObjectPath -eq "obj/src/node.node_main.o") {
    return "node.node_main"
  }
  if ($ObjectPath -eq "obj/src/node.node_snapshot_stub.o") {
    return "node.node_snapshot_stub"
  }
  return (($ObjectPath -replace '^obj/', '') -replace '[\\/]', '_' -replace '\.o$', '')
}

function Split-ObjectSpec([string]$Spec) {
  if ($Spec.Length -eq 0) {
    return @()
  }
  return @($Spec -split '[,;\s]+' | Where-Object { $_.Length -gt 0 })
}

function Resolve-NinjaObjectSource(
  [string]$ObjectPath,
  [string]$NativeRoot,
  [string]$ReleaseUnix,
  [string]$ReleaseWin,
  [string]$NodeRootUnix,
  [string]$SourceRootWin,
  [string]$GeneratedRootWin,
  [string[]]$WslArgs
) {
  $manualSources = @{
    "obj/src/libnode.node_sqlite.o" = Join-Path $SourceRootWin "src\node_sqlite.cc"
    "obj/src/libnode.node_webstorage.o" = Join-Path $SourceRootWin "src\node_webstorage.cc"
    "obj/deps/sqlite/sqlite.sqlite3.o" = Join-Path $SourceRootWin "deps\sqlite\sqlite3.c"
  }
  if ($manualSources.ContainsKey($ObjectPath)) {
    return $manualSources[$ObjectPath]
  }

  $quotedObject = $ObjectPath -replace "'", "'\''"
  $queryCommand = "build/wsl-tools/ninja-linux/ninja -C ports/upstream/node/out/out/Release -t query '$quotedObject'"
  $query = & wsl.exe @WslArgs --cd "$NativeRoot" -- bash -lc $queryCommand
  if ($LASTEXITCODE -ne 0) {
    throw "Unable to query Ninja object '$ObjectPath'"
  }

  $source = ""
  foreach ($line in $query) {
    $item = $line.Trim()
    if ($item -match '\.(c|cc|cxx|cpp)$') {
      $source = $item
      break
    }
  }
  if ($source.Length -eq 0) {
    throw "Ninja object '$ObjectPath' did not expose a C/C++ source"
  }

  if ($source.StartsWith("../../../")) {
    $relative = $source.Substring(9)
    return Join-Path $SourceRootWin ($relative -replace '/', '\')
  }
  if ($source.StartsWith("gen/")) {
    $relativeGenerated = $source -replace '/', '\'
    $candidate = Join-Path $GeneratedRootWin $relativeGenerated
    if (Test-Path -LiteralPath $candidate) {
      return $candidate
    }
    $nativeGenerated = Join-Path $ReleaseWin $relativeGenerated
    if (Test-Path -LiteralPath $nativeGenerated) {
      return $nativeGenerated
    }
    return $candidate
  }

  $quotedSource = $source -replace "'", "'\''"
  $sourceUnix = (& wsl.exe @WslArgs --cd "$ReleaseUnix" -- bash -lc "realpath '$quotedSource'").Trim()
  if ($LASTEXITCODE -ne 0 -or $sourceUnix.Length -eq 0) {
    throw "Unable to resolve source '$source' for '$ObjectPath'"
  }
  if ($sourceUnix.StartsWith("$NodeRootUnix/")) {
    $relative = $sourceUnix.Substring($NodeRootUnix.Length + 1)
    if ($relative.StartsWith("out/out/Release/gen/")) {
      $relativeGenerated = ($relative.Substring("out/out/Release/".Length)) -replace '/', '\'
      $candidate = Join-Path $GeneratedRootWin $relativeGenerated
      if (Test-Path -LiteralPath $candidate) {
        return $candidate
      }
      $nativeGenerated = Join-Path $ReleaseWin $relativeGenerated
      if (Test-Path -LiteralPath $nativeGenerated) {
        return $nativeGenerated
      }
      return $candidate
    }
    return Join-Path $SourceRootWin ($relative -replace '/', '\')
  }
  return (& wsl.exe @WslArgs --cd "$NativeRoot" -- bash -lc "wslpath -w '$sourceUnix'").Trim()
}

function Prepare-CompileSource([string]$SourcePath, [string]$Name, [string]$OutPath) {
  if (!($SourcePath.StartsWith("\\wsl.localhost\") -or $SourcePath.StartsWith("\\wsl$\"))) {
    return $SourcePath
  }

  $generatedPath = Join-Path $OutPath "generated-sources"
  New-Item -ItemType Directory -Force $generatedPath | Out-Null
  $extension = [IO.Path]::GetExtension($SourcePath)
  if ($extension.Length -eq 0) {
    $extension = ".cc"
  }
  $localPath = Join-Path $generatedPath "$Name$extension"
  Copy-Item -LiteralPath $SourcePath -Destination $localPath -Force
  return $localPath
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..\..")
$sysrootPath = Join-Path $repoRoot $Sysroot
$outPath = Join-Path $repoRoot $OutDir
$zig = Join-Path $repoRoot "build\tooling\zig\zig.exe"
$make = "make"

& $make -C $repoRoot srvros-sysroot

if (!(Test-Path -LiteralPath $zig)) {
  throw "Missing Zig toolchain at $zig"
}
if (!(Test-Path -LiteralPath $sysrootPath)) {
  throw "Missing srvros sysroot at $sysrootPath"
}

$wslArgs = @()
if ($Distro.Length -gt 0) {
  $wslArgs += @("-d", $Distro)
}

$nativeRoot = (& wsl.exe @wslArgs --cd "$repoRoot" -- bash -lc "cd $NativePath && pwd").Trim()
if ($LASTEXITCODE -ne 0 -or $nativeRoot.Length -eq 0) {
  throw "Unable to resolve WSL Node probe checkout at $NativePath"
}

$nodeRootUnix = "$nativeRoot/ports/upstream/node"
$releaseUnix = "$nodeRootUnix/out/out/Release"
$nodeRootWin = (& wsl.exe @wslArgs --cd "$nativeRoot" -- bash -lc "wslpath -w '$nodeRootUnix'").Trim()
$releaseWin = (& wsl.exe @wslArgs --cd "$nativeRoot" -- bash -lc "wslpath -w '$releaseUnix'").Trim()
$sourceRootWin = Join-Path $repoRoot "ports\upstream\node"
$localReleaseWin = Join-Path $sourceRootWin "out\out\Release"
$generatedRootWin = $releaseWin
if (Test-Path -LiteralPath (Join-Path $localReleaseWin "gen")) {
  $generatedRootWin = $localReleaseWin
}

if (!(Test-Path -LiteralPath $sourceRootWin)) {
  throw "Missing local Node source checkout at $sourceRootWin"
}
if (!(Test-Path -LiteralPath $releaseWin)) {
  throw "Missing Node release build graph at $releaseWin. Run probe-wsl-native.ps1 -ProbeTarget node first."
}

New-Item -ItemType Directory -Force $outPath | Out-Null

$zigLib = Join-Path (Split-Path -Parent $zig) "lib"
$libcxxInclude = Join-Path $zigLib "libcxx\include"
$libcxxAbiInclude = Join-Path $zigLib "libcxxabi\include"
$compilerInclude = Join-Path $zigLib "include"
$probeShimHeader = "ports/srvros/node/srvros-node-probe-shims.h"

$defines = @(
  "_GLIBCXX_USE_CXX11_ABI=1",
  "_FILE_OFFSET_BITS=64",
  "NODE_OPENSSL_CONF_NAME=nodejs_conf",
  "ICU_NO_USER_DATA_OVERRIDE",
  "__STDC_FORMAT_MACROS",
  "OPENSSL_NO_PINSHARED",
  "OPENSSL_THREADS",
  'NODE_ARCH="x64"',
  'NODE_PLATFORM="srvros"',
  "NODE_WANT_INTERNALS=1",
  "__POSIX__",
  "NODE_USE_V8_PLATFORM=1",
  "NODE_BUNDLED_ZLIB",
  "NODE_BUNDLED_ZSTD",
  "HAVE_OPENSSL=0",
  "HAVE_AMARO=1",
  "HAVE_SQLITE=0",
  "HAVE_QUIC=0",
  "XXH_NAMESPACE=ZSTD_",
  "ZSTD_MULTITHREAD",
  "ZSTD_DISABLE_ASM",
  "_LARGEFILE_SOURCE",
  "_POSIX_C_SOURCE=200112",
  "NGHTTP2_STATICLIB",
  "V8_GYP_BUILD",
  "V8_TARGET_ARCH_X64",
  "V8_OS_POSIX=1",
  'V8_OS_STRING="srvros"',
  'V8_TARGET_OS_STRING="srvros"',
  "V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP=64",
  "BUILDING_V8_SHARED",
  "BUILDING_V8_PLATFORM_SHARED",
  'V8_EMBEDDER_STRING="-node.49"',
  "ENABLE_DISASSEMBLER",
  "V8_PROMISE_INTERNAL_FIELD_COUNT=1",
  "V8_LITE_MODE",
  "V8_SHORT_BUILTIN_CALLS",
  "OBJECT_PRINT",
  "V8_ATOMIC_OBJECT_FIELD_WRITES",
  "V8_ENABLE_LAZY_SOURCE_POSITIONS",
  "V8_USE_SIPHASH",
  "V8_ENABLE_SEEDED_ARRAY_INDEX_HASH",
  "NDEBUG",
  "V8_ENABLE_REGEXP_INTERPRETER_THREADED_DISPATCH",
  "V8_USE_ZLIB",
  "V8_ENABLE_LEAPTIERING",
  "V8_ENABLE_SPARKPLUG",
  "V8_ENABLE_MAGLEV",
  "V8_ENABLE_TURBOFAN",
  "V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS",
  "V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA",
  "V8_ALLOCATION_FOLDING",
  "V8_ALLOCATION_SITE_TRACKING",
  "V8_ADVANCED_BIGINT_ALGORITHMS"
)

$cxxIncludeDirs = @(
  $libcxxInclude,
  $libcxxAbiInclude
)

$includeDirs = @(
  $compilerInclude,
  (Join-Path $sysrootPath "include"),
  (Join-Path $sourceRootWin "src"),
  (Join-Path $generatedRootWin "gen"),
  (Join-Path $generatedRootWin "gen\include"),
  (Join-Path $generatedRootWin "gen\generate-bytecode-output-root"),
  (Join-Path $sourceRootWin "deps\v8"),
  (Join-Path $sourceRootWin "deps\v8\include"),
  (Join-Path $sourceRootWin "deps\postject"),
  (Join-Path $sourceRootWin "deps\histogram\src"),
  (Join-Path $sourceRootWin "deps\histogram\include"),
  (Join-Path $sourceRootWin "deps\nbytes\include"),
  (Join-Path $sourceRootWin "deps\sqlite"),
  (Join-Path $sourceRootWin "deps\zlib"),
  (Join-Path $sourceRootWin "deps\llhttp\include"),
  (Join-Path $sourceRootWin "deps\cares\include"),
  (Join-Path $sourceRootWin "deps\uv\include"),
  (Join-Path $sourceRootWin "deps\uv\src"),
  (Join-Path $sourceRootWin "deps\googletest\include"),
  (Join-Path $sourceRootWin "deps\uvwasi\include"),
  (Join-Path $sourceRootWin "deps\nghttp2\lib\includes"),
  (Join-Path $sourceRootWin "deps\ada"),
  (Join-Path $sourceRootWin "deps\merve"),
  (Join-Path $sourceRootWin "deps\simdjson"),
  (Join-Path $sourceRootWin "deps\v8\third_party\simdutf"),
  (Join-Path $sourceRootWin "deps\v8\third_party\fp16\src\include"),
  (Join-Path $sourceRootWin "deps\v8\third_party\highway\src"),
  (Join-Path $sourceRootWin "deps\brotli\c\include"),
  (Join-Path $sourceRootWin "deps\zstd\lib"),
  (Join-Path $sourceRootWin "deps\v8\third_party\abseil-cpp")
) | Where-Object { Test-Path -LiteralPath $_ }

$commonArgs = @(
  "-target", "x86_64-freestanding-none",
  "-ffreestanding",
  "-fno-stack-protector",
  "-fno-stack-check",
  "-fno-lto",
  "-fno-PIC",
  "-ffunction-sections",
  "-fdata-sections",
  "-m64",
  "-mcpu=x86_64",
  "-mabi=sysv",
  "-mno-red-zone",
  "-O2",
  "-g",
  "-std=gnu++20",
  "-nostdinc++",
  "-fno-rtti",
  "-fno-exceptions",
  "-fno-strict-aliasing",
  "-Wall",
  "-Wextra",
  "-Wno-unused-parameter",
  "-Wno-restrict",
  "-Wno-deprecated-declarations",
  "-Wno-invalid-offsetof",
  "-include", $probeShimHeader,
  "-D_LIBCPP_HAS_NO_THREADS",
  "-D_LIBCPP_HAS_MONOTONIC_CLOCK=1",
  "-D_LIBCPP_HAS_CLOCK_GETTIME=1",
  "-D_LIBCPP_HAS_WIDE_CHARACTERS=1",
  "-D_LIBCPP_HAS_LOCALIZATION=1",
  "-D_LIBCPP_HAS_FILESYSTEM=1",
  "-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE=1",
  "-D_LIBCPP_HARDENING_MODE_DEFAULT=_LIBCPP_HARDENING_MODE_NONE",
  "-DPATH_MAX=160",
  "-DHOST_NAME_MAX=63"
)

foreach ($define in $defines) {
  $commonArgs += "-D$define"
}
foreach ($define in (Split-ObjectSpec $ExtraDefines)) {
  $commonArgs += "-D$define"
}
foreach ($include in $cxxIncludeDirs) {
  $commonArgs += "-I"
  $commonArgs += $include
}
foreach ($include in $includeDirs) {
  $commonArgs += "-isystem"
  $commonArgs += $include
}

$defaultObjects = @(
  "obj/src/node.node_main.o",
  "obj/src/node.node_snapshot_stub.o",
  "obj/src/libnode.node_options.o",
  "obj/src/libnode.node_errors.o",
  "obj/src/libnode.node_metadata.o",
  "obj/src/libnode.node_config_file.o",
  "obj/src/libnode.node_types.o",
  "obj/src/libnode.node_debug.o",
  "obj/src/libnode.node_task_queue.o",
  "obj/src/libnode.node_platform.o",
  "obj/src/libnode.debug_utils.o",
  "obj/src/libnode.util.o",
  "obj/src/api/libnode.callback.o",
  "obj/src/libnode.node_report.o",
  "obj/src/tracing/libnode.agent.o",
  "obj/src/libnode.node_process_events.o",
  "obj/src/libnode.node_process_methods.o",
  "obj/src/libnode.node_buffer.o",
  "obj/src/api/libnode.hooks.o",
  "obj/src/api/libnode.exceptions.o",
  "obj/src/api/libnode.encoding.o",
  "obj/src/libnode.async_context_frame.o",
  "obj/src/libnode.env.o",
  "obj/src/libnode.node_credentials.o",
  "obj/src/permission/libnode.permission.o",
  "obj/src/libnode.node_dotenv.o",
  "obj/src/libnode.json_utils.o",
  "obj/src/libnode.heap_utils.o",
  "obj/src/libnode.node.o",
  "obj/src/libnode.module_wrap.o",
  "obj/src/libnode.node_worker.o",
  "obj/src/libnode.node_main_instance.o",
  "obj/src/permission/libnode.fs_permission.o",
  "obj/src/libnode.tcp_wrap.o",
  "obj/src/libnode.udp_wrap.o",
  "obj/src/libnode.pipe_wrap.o",
  "obj/src/libnode.stream_base.o",
  "obj/src/libnode.stream_wrap.o",
  "obj/src/libnode.compile_cache.o",
  "obj/src/api/libnode.environment.o",
  "obj/src/libnode.node_binding.o",
  "obj/src/api/libnode.async_resource.o",
  "obj/src/libnode.async_wrap.o",
  "obj/src/libnode.node_file.o",
  "obj/src/libnode.path.o",
  "obj/src/libnode.node_report_utils.o",
  "obj/src/libnode.node_snapshotable.o",
  "obj/src/libnode.node_modules.o",
  "obj/src/libnode.node_contextify.o",
  "obj/src/libnode.node_dir.o",
  "obj/src/libnode.node_api.o",
  "obj/src/libnode.node_process_object.o",
  "obj/src/libnode.node_sea.o",
  "obj/src/libnode.node_task_runner.o",
  "obj/src/tracing/libnode.node_trace_writer.o",
  "obj/src/tracing/libnode.node_trace_buffer.o",
  "obj/src/tracing/libnode.trace_event.o",
  "obj/src/tracing/libnode.traced_value.o",
  "obj/src/libnode.node_url.o",
  "obj/src/libnode.node_perf.o",
  "obj/src/libnode.histogram.o",
  "obj/src/libnode.node_messaging.o",
  "obj/src/dataqueue/libnode.queue.o",
  "obj/src/api/libnode.embed_helpers.o",
  "obj/src/libnode.node_report_module.o",
  "obj/src/libnode.node_util.o",
  "obj/src/libnode.node_builtins.o",
  "obj/src/libnode.node_sea_bin.o",
  "obj/src/libnode.signal_wrap.o",
  "obj/src/libnode.node_wasi.o",
  "obj/src/api/libnode.utils.o",
  "obj/src/libnode.encoding_binding.o",
  "obj/src/libnode.node_blob.o",
  "obj/src/libnode.node_url_pattern.o",
  "obj/src/libnode.embedded_data.o",
  "obj/src/libnode.cares_wrap.o",
  "obj/src/libnode.node_trace_events.o",
  "obj/src/libnode.node_env_var.o",
  "obj/src/libnode.uv.o",
  "obj/src/libnode.timers.o",
  "obj/src/libnode.tty_wrap.o",
  "obj/src/libnode.stream_pipe.o",
  "obj/src/libnode.node_watchdog.o",
  "obj/src/libnode.node_v8.o",
  "obj/src/libnode.node_os.o",
  "obj/src/libnode.node_cjs_lexer.o",
  "obj/src/libnode.node_config.o",
  "obj/src/libnode.node_external_reference.o",
  "obj/src/libnode.string_bytes.o",
  "obj/src/libnode.process_wrap.o",
  "obj/src/libnode.node_sockaddr.o",
  "obj/src/libnode.node_diagnostics_channel.o",
  "obj/src/libnode.string_decoder.o",
  "obj/src/libnode.node_locks.o",
  "obj/src/libnode.internal_only_v8.o",
  "obj/src/libnode.js_native_api_v8.o",
  "obj/src/libnode.node_realm.o",
  "obj/src/libnode.node_http2.o",
  "obj/src/libnode.node_zlib.o",
  "obj/src/libnode.node_stat_watcher.o",
  "obj/src/libnode.base_object.o",
  "obj/src/libnode.connection_wrap.o",
  "obj/src/libnode.fs_event_wrap.o",
  "obj/src/libnode.handle_wrap.o",
  "obj/src/libnode.js_stream.o",
  "obj/src/libnode.js_udp_wrap.o",
  "obj/src/libnode.node_wasm_web_api.o",
  "obj/src/libnode.spawn_sync.o",
  "obj/src/libnode.connect_wrap.o",
  "obj/src/libnode.node_http_parser.o",
  "obj/src/libnode.node_serdes.o",
  "obj/deps/merve/merve.merve.o",
  "obj/deps/v8/src/libplatform/v8_libplatform.default-foreground-task-runner.o",
  "obj/deps/v8/src/libplatform/v8_libplatform.default-job.o",
  "obj/deps/v8/src/libplatform/v8_libplatform.default-platform.o",
  "obj/deps/v8/src/libplatform/v8_libplatform.default-thread-isolated-allocator.o",
  "obj/deps/v8/src/libplatform/v8_libplatform.default-worker-threads-task-runner.o",
  "obj/deps/v8/src/libplatform/v8_libplatform.delayed-task-queue.o",
  "obj/deps/v8/src/libplatform/v8_libplatform.task-queue.o",
  "obj/deps/v8/src/libplatform/tracing/v8_libplatform.trace-object.o",
  "obj/deps/v8/src/libplatform/tracing/v8_libplatform.trace-writer.o",
  "obj/deps/v8/src/libplatform/tracing/v8_libplatform.trace-buffer.o",
  "obj/deps/v8/src/libplatform/tracing/v8_libplatform.trace-config.o",
  "obj/deps/v8/src/libplatform/tracing/v8_libplatform.tracing-controller.o",
  "obj/deps/v8/src/libplatform/v8_libplatform.worker-thread.o",
  "obj/deps/v8/src/api/v8_base_without_compiler.api.o",
  "obj/deps/v8/src/heap/cppgc/v8_base_without_compiler.gc-info.o",
  "obj/deps/v8/src/heap/cppgc/v8_base_without_compiler.gc-info-table.o",
  "obj/deps/v8/src/objects/v8_base_without_compiler.templates.o",
  "obj/deps/v8/src/objects/v8_base_without_compiler.objects.o",
  "obj/deps/v8/src/heap/v8_base_without_compiler.factory-base.o",
  "obj/deps/v8/src/heap/v8_base_without_compiler.factory.o",
  "obj/deps/v8/src/heap/v8_base_without_compiler.heap.o",
  "obj/deps/v8/src/profiler/v8_base_without_compiler.cpu-profiler.o",
  "obj/deps/v8/src/profiler/v8_base_without_compiler.heap-profiler.o",
  "obj/deps/v8/src/codegen/v8_base_without_compiler.compiler.o",
  "obj/deps/v8/src/init/v8_base_without_compiler.bootstrapper.o",
  "obj/deps/v8/src/builtins/v8_base_without_compiler.builtins-number.o",
  "obj/deps/v8/src/builtins/v8_base_without_compiler.builtins-object.o",
  "obj/deps/v8/src/codegen/v8_base_without_compiler.external-reference.o",
  "obj/deps/v8/src/diagnostics/v8_base_without_compiler.objects-printer.o",
  "obj/deps/v8/src/objects/v8_base_without_compiler.js-array-buffer.o",
  "obj/deps/v8/src/profiler/v8_base_without_compiler.profile-generator.o",
  "obj/deps/v8/src/profiler/v8_base_without_compiler.tick-sample.o",
  "obj/deps/v8/src/utils/v8_base_without_compiler.ostreams.o",
  "obj/tools/v8_gypfiles/gen/torque-generated/src/objects/v8_base_without_compiler.primitive-heap-object-tq.o",
  "obj/deps/ada/ada.ada.o"
)

$requestedObjects = @()
$requestedObjects += Split-ObjectSpec $Objects
if ($ObjectList.Length -gt 0) {
  if (!(Test-Path -LiteralPath $ObjectList)) {
    throw "Missing object list at $ObjectList"
  }
  $requestedObjects += Get-Content -LiteralPath $ObjectList |
    ForEach-Object { $_.Trim() } |
    Where-Object { $_.Length -gt 0 -and !($_.StartsWith("#")) }
}
if ($requestedObjects.Count -eq 0) {
  $requestedObjects = $defaultObjects
}
$requestedObjects = @($requestedObjects | Select-Object -Unique)

$targets = @()
foreach ($objectPath in $requestedObjects) {
  $name = Convert-ObjectPathToName $objectPath
  $source = Resolve-NinjaObjectSource `
    -ObjectPath $objectPath `
    -NativeRoot $nativeRoot `
    -ReleaseUnix $releaseUnix `
    -ReleaseWin $releaseWin `
    -NodeRootUnix $nodeRootUnix `
    -SourceRootWin $sourceRootWin `
    -GeneratedRootWin $generatedRootWin `
    -WslArgs $wslArgs
  $source = Prepare-CompileSource -SourcePath $source -Name $name -OutPath $outPath
  $targets += @{
    Name = $name
    Source = $source
    ObjectPath = $objectPath
    LinuxObject = Join-Path $releaseWin ($objectPath -replace '/', '\')
    SrvrosObject = Join-Path $outPath "$name.srvros.o"
  }
}

$failed = $false
$compiled = @()
$failedTargets = @()
$manifestPath = Join-Path $outPath "replacements.tsv"
$manifestRows = @{}
if (Test-Path -LiteralPath $manifestPath) {
  foreach ($row in (Import-Csv -LiteralPath $manifestPath -Delimiter "`t")) {
    if ($row.object -and $row.name -and $row.srvros_object) {
      $manifestRows[$row.object] = "$($row.object)`t$($row.name)`t$($row.source)`t$($row.srvros_object)"
    }
  }
}

foreach ($target in $targets) {
  $name = $target.Name
  $rsp = Join-Path $outPath "$name.rsp"
  $stdoutLog = Join-Path $outPath "$name.stdout.log"
  $stderrLog = Join-Path $outPath "$name.stderr.log"
  $log = Join-Path $outPath "$name.compile.log"
  $srvrosObject = $target.SrvrosObject

  $args = @($commonArgs)
  $compilerMode = "c++"
  if ([IO.Path]::GetExtension($target.Source) -eq ".c") {
    $compilerMode = "cc"
    $filteredArgs = @()
    for ($i = 0; $i -lt $args.Count; $i++) {
      if ($args[$i] -eq "-include") {
        $i++
        continue
      }
      if ($args[$i] -eq "-std=gnu++20" -or
          $args[$i] -eq "-nostdinc++" -or
          $args[$i] -eq "-fno-rtti" -or
          $args[$i] -eq "-fno-exceptions") {
        continue
      }
      $filteredArgs += $args[$i]
    }
    $args = $filteredArgs
    $args += "-D__ros__=1"
    $args += "-std=gnu11"
  }
  if ($LinuxLibuvLayout) {
    $args += "-D__linux__=1"
  }
  if ($target.ObjectPath -eq "obj/deps/v8/src/base/v8_libbase.region-allocator.o") {
    # This constructor is currently instantiated inside V8 objects that can be
    # only 8-byte aligned, while clang emits aligned SSE zero stores for the
    # libc++ set members. Keep this one bridge object on scalar stores until
    # the broader V8 alignment contract is settled.
    $args += @("-O0", "-fno-sanitize=all")
  }
  $args += @("-c", $target.Source, "-o", $srvrosObject)
  ($args | ForEach-Object { Quote-RspArg $_ }) | Set-Content -LiteralPath $rsp

  Write-Host "Compiling $name for srvros..."
  $exitCode = Invoke-CapturedProcess -FilePath $zig -ArgumentList @($compilerMode, "@$rsp") -StdoutPath $stdoutLog -StderrPath $stderrLog
  $output = Read-LogLines @($stdoutLog, $stderrLog)
  $output | Tee-Object -FilePath $log

  if ($exitCode -eq 0) {
    $compiled += $target.ObjectPath
    $manifestRows[$target.ObjectPath] = "$($target.ObjectPath)`t$name`t$($target.Source)`t$srvrosObject"
    Write-UndefinedSymbols -ObjectPath $srvrosObject -OutputPath (Join-Path $outPath "$name.srvros.undefined.txt")
    Write-UndefinedSymbols -ObjectPath $target.LinuxObject -OutputPath (Join-Path $outPath "$name.linux.undefined.txt")
    Write-Host "  wrote $srvrosObject"
  } else {
    $failed = $true
    $failedTargets += $target.ObjectPath
    Write-Host "  stopped at compile frontier; see $log"
  }
}

$manifest = @("object`tname`tsource`tsrvros_object") + @($manifestRows.GetEnumerator() | Sort-Object Name | ForEach-Object { $_.Value })
$manifest | Set-Content -LiteralPath $manifestPath
if ($failedTargets.Count -gt 0) {
  $failedTargets | Set-Content -LiteralPath (Join-Path $outPath "failed.txt")
} elseif (Test-Path -LiteralPath (Join-Path $outPath "failed.txt")) {
  Remove-Item -LiteralPath (Join-Path $outPath "failed.txt")
}

$summary = @(
  "srvros Node compile probe",
  "nativeRoot=$nativeRoot",
  "sourceRoot=$sourceRootWin",
  "generatedRoot=$generatedRootWin",
  "release=$releaseWin",
  "requested=$($requestedObjects -join ', ')",
  "compiled=$($compiled -join ', ')",
  "failedTargets=$($failedTargets -join ', ')",
  "failed=$failed"
)
$summary | Set-Content -LiteralPath (Join-Path $outPath "summary.txt")

if ($failed) {
  Write-Host "srvros Node compile probe reached the expected compile frontier. See $outPath"
  if ($FailOnError) {
    exit 1
  }
  exit 0
}

Write-Host "srvros Node compile probe completed. See $outPath"
exit 0
