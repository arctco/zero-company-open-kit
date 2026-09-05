# Cross-compile to Windows x86-64 with clang-cl and the xwin-fetched MSVC SDK.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(NOT DEFINED ENV{OPENKIT_WINSDK})
    set(WINSDK "$ENV{HOME}/.local/share/openkit-winsdk")
else()
    set(WINSDK "$ENV{OPENKIT_WINSDK}")
endif()

set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER lld-link)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT llvm-mt)
set(CMAKE_AR llvm-lib)
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)

# /winsysroot expects a Visual Studio layout; xwin produces crt/ and sdk/, so the
# include and library paths go in explicitly.
set(_inc "/imsvc ${WINSDK}/crt/include \
/imsvc ${WINSDK}/sdk/include/ucrt \
/imsvc ${WINSDK}/sdk/include/um \
/imsvc ${WINSDK}/sdk/include/shared")
set(CMAKE_C_FLAGS_INIT "${_inc}")
set(CMAKE_CXX_FLAGS_INIT "${_inc}")

set(_lib "/libpath:${WINSDK}/crt/lib/x86_64 \
/libpath:${WINSDK}/sdk/lib/ucrt/x86_64 \
/libpath:${WINSDK}/sdk/lib/um/x86_64")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_lib}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_lib}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_lib}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# xwin's default splat carries the release CRT import libraries only; the debug
# CRT (msvcrtd.lib) is behind --include-debug-libs. Pin every configuration to
# the release DLL runtime so nothing asks for a library we did not fetch.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
