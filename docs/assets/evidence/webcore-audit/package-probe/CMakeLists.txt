cmake_minimum_required(VERSION 3.20)
project(SrvrOSWebCorePackageAudit LANGUAGES NONE)

# Isolated dependency-discovery probe, NOT a WebKit/target configure. No
# compiler is enabled, no engine feature check is overridden, and host target
# libraries must not satisfy the deliberately empty target prefix.
if (NOT IS_DIRECTORY "${AUDIT_UPSTREAM}/Source/cmake")
    message(FATAL_ERROR "AUDIT_UPSTREAM must identify the verified pinned source")
endif ()
set(CMAKE_MODULE_PATH "${AUDIT_UPSTREAM}/Source/cmake")
set(_audit_target "${CMAKE_BINARY_DIR}/empty-target")
file(MAKE_DIRECTORY "${_audit_target}/pkgconfig")
set(CMAKE_FIND_ROOT_PATH "${_audit_target}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY FALSE)
set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH FALSE)
set(ENV{PKG_CONFIG_PATH} "")
set(ENV{PKG_CONFIG_LIBDIR} "${_audit_target}/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${_audit_target}")

# Exact first mandatory target lookup in pinned OptionsWPE.cmake, line12.
find_package(GLib 2.70.0 REQUIRED COMPONENTS GioUnix Thread Module)
