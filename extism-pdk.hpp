#pragma once

void wasi_log_unavailability(const char* msg);
extern "C" void abort();
#define unavailable(...) (wasi_log_unavailability(__FUNCTION__), abort(), __VA_ARGS__)

#ifdef XX_USE_LIBCPP_ONLY
  #pragma message("XX_LIBCPP_ONLY")
  #include "wip/x-wasi-constants.h"
  #include "wip/x-wasi-static-only.h"
  #define fprintf wasifprintf
  #define printf wasiprintf
  #include <cstdio>
  #undef fprintf
  #undef printf
#else
  #pragma message("ASSUMING FULL WASI SUPPORT (note: consider using extism/c-pdk if neither WASI nor LIBCPP are needed)")
  #include <wasi/api.h>
  #ifdef using_llvm_clang_toolchain
    extern "C" void* __cxa_allocate_exception(unsigned long int thrown_size) throw() { return unavailable(nullptr); }
    extern "C" void  __cxa_throw(void* ptr, void* type, void* destructor) { return unavailable(void()); }
  #endif
#endif

// make __xx_toolchain__ available as string a literal __toolchain__
#define xx_stringize___(s) #s
#define xx_stringize__(s) xx_stringize___(s)
#define __toolchain__ xx_stringize__(__xx_toolchain__)
//#pragma message(__toolchain__)

// ------------------------
// include underlying C-PDK
// ------------------------
#include "extism-pdk.h"

// ------------------------
void wasi_log_unavailability(const char* msg) { extism_log_sz(msg, ExtismLogError); }



#include <string>

// an experimental API factoring
struct HostedPlugin {
  struct String : std::string {
    explicit String(ExtismHandle&& _handle)  {
      resize(extism_length(_handle));
      extism_load_from_handle(_handle, 0, data(), size());
      extism_free(_handle);
    }
  };

  struct Var {
    template <typename T = String> T get(const char* key) { return T{ extism_var_get_buf_from_sz(key) }; }
    template <typename T = String> T operator()(const char* key) { return T{ extism_var_get_buf_from_sz(key) }; }
    template <typename T> void set(const char* key, T const& value ) { extism_var_set_buf_from_sz(key, value); }
    
  } var;
  String config(const char* key) { return String{ extism_config_get_buf_from_sz(key) }; }

  struct Input {
    const uint64_t length{ extism_input_length() };
    template <typename T> bool load(T& t) const {
      //assert(sizeof(T) == extism_input_length());
      return extism_load_input(0, &t, sizeof(T));
    }
    template <typename T> T const& get() const {
      static T t;
      if (!load(t)) return t={};
      return t;
    }
  } _input;
  template <typename T> T const& input() const { return _input.get<T>(); } 

  template <typename T> constexpr void output(T const& t, size_t sz = sizeof(T)) {
      //assert(sizeof(T) == sz);
      extism_output_buf((uint8_t*)&t, sz);
  }
  void formatOutput(const char* fmt, ...);

  struct Error {
    void format(const char* fmt, ...);
  } error;

  struct Console {
    void __log(ExtismLog level, const char* fmt, __builtin_va_list ap);
    void _log(ExtismLog level, const char* fmt, ...) { __builtin_va_list ap; __builtin_va_start(ap, fmt); __log(level, fmt, ap); __builtin_va_end(ap); }
    template<typename ...Args> constexpr void info(const char* fmt, Args ...args) { _log(ExtismLogInfo, fmt, args...); }
    template<typename ...Args> constexpr void debug(const char* fmt, Args ...args) { _log(ExtismLogDebug, fmt, args...); }
    template<typename ...Args> constexpr void warn(const char* fmt, Args ...args) { _log(ExtismLogWarn, fmt, args...); }
    template<typename ...Args> constexpr void error(const char* fmt, Args ...args) { _log(ExtismLogError, fmt, args...); }
    template<typename ...Args> constexpr void log(const char* fmt, Args ...args) { _log(ExtismLogDebug, fmt, args...); }
  } console;

};

extern HostedPlugin self;

namespace nowasi {
  inline int fprintf(void* s, const char* fmt, ...);
  inline int printf(const char* fmt, ...);
}

// ----------------------------------------------------------------------------
#ifdef EXTISM_IMPLEMENTATION
// ----------------------------------------------------------------------------

HostedPlugin self;

#include <stdarg.h> // for va_list, va_start, etc.
#include <stdio.h>  // for vsnprintf, stderr
#include <string>

#define __fmt_from_ap(outBuf, ap) vsnprintf(outBuf, sizeof(outBuf)-1, fmt, ap);

void HostedPlugin::Error::format(const char* fmt, ...) {
  char outBuf[1024]{0}; va_list ap; va_start(ap, fmt); int sz = __fmt_from_ap(outBuf, ap); va_end(ap);
  extism_error_set_buf_from_sz(outBuf);
}

void HostedPlugin::formatOutput(const char* fmt, ...) {
  char outBuf[1024]{0}; va_list ap; va_start(ap, fmt); int sz = __fmt_from_ap(outBuf, ap); va_end(ap);
  extism_output_buf(outBuf, sz);
}

void HostedPlugin::Console::__log(ExtismLog level, const char* fmt, __builtin_va_list ap) {
  char outBuf[1024]={0};
  __fmt_from_ap(outBuf, ap);
  static auto const& strncat = [](char *dest, const char *src, size_t n) -> char* {
    char *p = dest; while (*dest) dest++;
    while (n-- && *src) *dest++ = *src++;
    *dest = '\0'; return p;
  };
  char buf[2048]{0};
  {switch(level) {
    case ExtismLogInfo: strncat(buf, "[info] ", sizeof(buf)); break;
    case ExtismLogDebug: strncat(buf, "[debug] ", sizeof(buf)); break;
    case ExtismLogWarn: strncat(buf, "[warn] ", sizeof(buf)); break;
    case ExtismLogError: strncat(buf, "[error] ", sizeof(buf)); break;
    default: strncat(buf, "[unknown] ", sizeof(buf)); break;
    }};
  strncat(buf, outBuf, sizeof(buf));
  extism_log_sz(buf, level);
}

namespace nowasi {
  inline int fprintf(void* s, const char* fmt, ...) {
    char outBuf[1024]{0}; va_list ap; va_start(ap, fmt); int sz = __fmt_from_ap(outBuf, ap); va_end(ap);
    extism_log_sz(outBuf, s == stdout ? ExtismLogDebug : (s == stderr ? ExtismLogInfo : ExtismLogWarn));
    return 0;
  }
  inline int printf(const char* fmt, ...) {
    char outBuf[1024]{0}; va_list ap; va_start(ap, fmt); int sz = __fmt_from_ap(outBuf, ap); va_end(ap);
    extism_log_sz(outBuf, ExtismLogDebug);
    return 0;
  }
} // ns

//using namespace nowasi;  

#undef __fmt_from_va
#undef __fmt_from_ap

#ifdef XX_USE_LIBCPP_ONLY
  #define fprintf nowasi::fprintf
  #define printf nowasi::printf
#endif

#endif // EXTISM_IMPLEMENTATION
