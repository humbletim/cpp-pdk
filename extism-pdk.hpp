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

// make __xx_toolchain__ available as a string literal __toolchain__
#define xx_stringize___(s) #s
#define xx_stringize__(s) xx_stringize___(s)
#define __toolchain__ xx_stringize__(__xx_toolchain__)
//#pragma message(__toolchain__)

// ------------------------
// include underlying C-PDK
// ------------------------
#include "extism-pdk.h"

#define va_forward_ap(var, ...)   \
  __builtin_va_list ap;           \
  __builtin_va_start(ap, var);    \
  __VA_ARGS__;                    \
  __builtin_va_end(ap);

#define va_format_zeroalloc(tmpbuffer, fmt) \
  { tmpbuffer[0] = 0; va_forward_ap(fmt, vsnprintf(tmpbuffer, sizeof(tmpbuffer)-1, fmt, ap)); }

// pure c++11 direct char[] appender
template <typename T> inline char* append(T&& buf, const char *src) {
  auto total = sizeof(T);
  char* dest = buf;
  while (total-- && *dest) dest++;
  while (total-- && *src) *dest++ = *src++;
  *dest = '\0';
  return dest;
};

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
    bool load(String& s) const {
      s.resize(length);
      return extism_load_input(0, s.data(), s.size());
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
  constexpr void output(String const& s) const {
    extism_output_buf((uint8_t*)s.data(), s.size());
  }
  void formatOutput(const char* fmt, ...);

  struct Error {
    void format(const char* fmt, ...);
  } error;

  struct Console {
    static void _log  (ExtismLog level, const char* fmt, __builtin_va_list ap);
    static void info  (const char* fmt, ...) { va_forward_ap(fmt, _log(ExtismLogInfo , fmt, ap) ); }
    static void debug (const char* fmt, ...) { va_forward_ap(fmt, _log(ExtismLogDebug, fmt, ap) ); }
    static void warn  (const char* fmt, ...) { va_forward_ap(fmt, _log(ExtismLogWarn , fmt, ap) ); }
    static void error (const char* fmt, ...) { va_forward_ap(fmt, _log(ExtismLogError, fmt, ap) ); }
    static void log   (const char* fmt, ...) { va_forward_ap(fmt, _log(ExtismLogDebug, fmt, ap) ); }
  } console;

  static char tmpbuffer[1024];
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
char HostedPlugin::tmpbuffer[1024];

#include <stdio.h>  // for vsnprintf, stderr
#include <string>

void HostedPlugin::Error::format(const char* fmt, ...) {
  va_format_zeroalloc(HostedPlugin::tmpbuffer, fmt);
  extism_error_set_buf_from_sz(HostedPlugin::tmpbuffer);
}

void HostedPlugin::formatOutput(const char* fmt, ...) {
  va_format_zeroalloc(HostedPlugin::tmpbuffer, fmt);
  extism_output_buf_from_sz(HostedPlugin::tmpbuffer);
}

void HostedPlugin::Console::_log(ExtismLog level, const char* fmt, __builtin_va_list ap) {
  static constexpr auto prefix = [](ExtismLog level) { switch(level) {
    case ExtismLogInfo : return "[pdk-info]   ";
    case ExtismLogDebug: return "[pdk-debug]  ";
    case ExtismLogWarn : return "[pdk-warn]   ";
    case ExtismLogError: return "[pdk-error]  ";
                default: return "[pdk-unknown]";
  }};
  static char formatted[1024];
  {
    formatted[0] = 0;
    vsnprintf(formatted, sizeof(formatted)-1, fmt, ap);
    char buf[1024]{""};
    append(buf, prefix(level));
    append(buf, formatted);
    extism_log_sz(buf, level);
  }
}

namespace nowasi {
  inline int fprintf(void* s, const char* fmt, ...) {
    va_format_zeroalloc(HostedPlugin::tmpbuffer, fmt);
    extism_log_sz(HostedPlugin::tmpbuffer, s == stdout ? ExtismLogDebug : (s == stderr ? ExtismLogInfo : ExtismLogWarn));
    return 0;
  }
  inline int printf(const char* fmt, ...) {
    va_format_zeroalloc(HostedPlugin::tmpbuffer, fmt);
    extism_log_sz(HostedPlugin::tmpbuffer, ExtismLogDebug);
    return 0;
  }
} // ns

//using namespace nowasi;  

#ifdef XX_USE_LIBCPP_ONLY
  #define fprintf nowasi::fprintf
  #define printf nowasi::printf
#endif

#endif // EXTISM_IMPLEMENTATION
