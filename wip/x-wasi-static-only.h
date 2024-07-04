#pragma once

#include "x-wasi-constants.h"
#include <stdint.h>

// log and abort if encountering any lingering WASI invocations at runtime
void wasi_log_unavailability(const char*);
extern "C" void abort();
//#include <assert.h>

//#if weaken_wasi_sdk
  #pragma weak __wasi_fd_seek
  #pragma weak __wasi_fd_read
  #pragma weak __wasi_fd_write
  #pragma weak __wasi_fd_close
  #pragma weak __wasi_fd_fdstat_get
  #pragma weak __wasi_fd_fdstat_set
  #pragma weak __wasi_fd_fdstat_set_flags
  #pragma weak __wasi_fd_prestat_get
  #pragma weak __wasi_fd_prestat_dir_name
  #pragma weak __wasi_path_open
  #pragma weak __wasi_path_create_directory
  #pragma weak __wasi_environ_sizes_get
  #pragma weak __wasi_environ_get
  #pragma weak __wasi_proc_exit
//#endif

// not all wasi toolchains emerge the same injected WASI symbols, but these
// three variations seem to cover Emscripten, WASI-SDK and stock LLVM-Clang
// when using wasm32-wasi or wasm32-emscripten etc. targets
#define inline_override(ret, ...) \
  extern "C" ret __imported_wasi_snapshot_preview1_##__VA_ARGS__ { return unavailable(ret()); } \
  extern "C" ret __wasi_##__VA_ARGS__ { return unavailable(ret()); } \
  extern "C" ret __VA_ARGS__ { return unavailable(ret()); } 


using i32 = int32_t;
using i64 = int64_t;
#if 1
  inline_override(int, fd_read(int, int, int, int))
  inline_override(int, fd_write(int, int, int, int))
  inline_override(int, fd_seek(int, long long, int, int))
  inline_override(int, fd_close(int))
  inline_override(int, fd_fdstat_get(int,int))
  inline_override(int, fd_fdstat_set(int,int))
  inline_override(int, fd_fdstat_set_flags(int,int))
  inline_override(int, fd_prestat_dir_name(int,int,int))
  inline_override(int, fd_prestat_get(int,int))
  inline_override(int, path_open(i32, i32, i32, i32, i64, i64, i32, i32))
  inline_override(int, path_create_directory(i32, i32, i32))
  inline_override(int, environ_get(int,int))
  inline_override(int, environ_sizes_get(int,int))
  inline_override(void, proc_exit(int))
#endif

// prevent typical "accidental" reliance on WASI by trigger compile errors if
// things like <ios> or <cstdio> are naively included as system headers...
namespace std {
  int cin = 0;
  int cout = 1;
  int cerr = 2;
}

namespace {
  int fopen;
  int fclose;
  int fwrite;
}

#pragma weak __cxa_throw
#pragma weak __cxa_begin_catch
#pragma weak __cxa_end_catch
#pragma weak __cxa_allocate_exception
extern "C" void* __cxa_allocate_exception(unsigned long int thrown_size) throw() { return unavailable(nullptr); }
extern "C" void  __cxa_throw(void* ptr, void* type, void* destructor) { return unavailable(void()); }
extern "C" void  __cxa_begin_catch(void* unwind_arg) _NOEXCEPT { return unavailable(void()); }
extern "C" void  __cxa_end_catch() { unavailable(void()); }

#ifdef __EMSCRIPTEN__
  //#pragma weak emscripten_resize_heap
  //extern "C" int emscripten_resize_heap(int) { return deported(0); }
  #pragma weak _emscripten_memcpy_js
  extern "C" void _emscripten_memcpy_js(void *dest, const void *src, int n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
  #pragma clang loop unroll(disable)
    while(n--) *d++ = *s++;
  }
#endif // __EMSCRIPTEN__

