# NOTE: EARLY WORK IN PROGRESS / EXPERIMENTAL

*... documenting some initial experiments and thoughts here re: C++ Plugin Development Kit*

# Extism C++ PDK
## (adapted from Extism C PDK)
This project contains a tool that can be used to create [Extism Plug-ins](https://extism.org/docs/concepts/plug-in) in C++.

## Installation

The Extism C++ PDK is a single header library ([extism-pdk.hpp](blob/main/extism-pdk.hpp)).

## Getting Started

The goal of writing an [Extism plug-in](https://extism.org/docs/concepts/plug-in) is to compile your C++ code to a Wasm module with exported functions that the host application can invoke.
The first thing you should understand is creating an export.

### Exports

Let's write a simple program that exports a `greet` function which will take a name as a string and return a greeting string. Paste this into a file `plugin.cpp`:

```c++
#define EXTISM_IMPLEMENTATION
#include "extism-pdk.hpp"

#include <string>
#include <cstdint>

constexpr const auto Greeting = "Hello";

int32_t EXTISM_EXPORTED_FUNCTION(greet) {
  // read plugin.call input
  std::string input = host.input<std::string>();
  // set plugin output
  host.output(Greeting + ", " + inputData);
  return 0; 
}
```

The `EXTISM_EXPORTED_FUNCTION` macro simplifies declaring an Extism function that will be exported to the host.

Since we don't need any system access for this, we can compile this directly with clang++:

```shell
clang++ -o plugin.wasm --target=wasm32-wasi -nostdlib -Wl,--no-entry plugin.cpp
```

The above command may fail if ran with system-installed clang++. It is currently recommended to use the version of clang++ included by [emscripten](...) or the [wasi-sdk](https://github.com/WebAssembly/wasi-sdk), which also include a libc++ implementation targeting WASI, which is necessary for plugins even if only relying on features implemented by the C++ standard library.

Let's break down the command a little:

- `--target=wasm32-wasi` configures the Webassembly target
- `-nostdlib` tells the compiler not to link the standard library
- `-Wl,--no-entry` is a linker flag to tell the linker there is no `_start` function

We can now test `plugin.wasm` using the [Extism CLI](https://github.com/extism/cli)'s `call`
command:

```bash
extism call plugin.wasm greet --input="tim"
# => Hello, tim
```

### More Exports: Error Handling

We catch any exceptions thrown and return them as errors to the host. Suppose we want to re-write our greeting module to never greet foo:

```c++
#define EXTISM_IMPLEMENTATION
#include "extism-pdk.hpp"

#include <cstdint>
#include <string>

constexpr const auto Greeting = "Hello";

namespace {
  bool is_foo(std::string const& name) {
    return name == "foo";
  }
}

int32_t EXTISM_EXPORTED_FUNCTION(greet) {
  std::string input = host.input<std::string>();
  // Check if the input matches "foo", if it does
  // return an error
  if (is_foo(input)) {
    host.error.format("ERROR: is_foo(%s)", input.c_str());
    return -1;
  }
  host.output(Greeting + ", " + inputData);
  return 0;
}
```

## Leveraging WASI libc++ polyfills... without WASI host implications.

WASI introduces several layers of complication ahead of crafting basic .wasm plugins from C++. And beyond its specification, "WASI" has regrettably become very tightly coupled by toolchains into serving a double-duty:
1. Instrumenting native features (like filesystem and io) between the host and a Wasm module.
2. Polyfilling all the missing dimensions of libc, libc++, libc++api needed to practically achieve the above!

This means using things like `<string>` from C++ technically became feasible... but only if *bracketing string support with filesystem support* (which at best makes incomplete sense).

Therefore, an intitial goal here becomes leveraging (2) *without triggering any runtime dependencies on (1)*. Later on, enabling "actual" WASI capabilities (like filesystem support) from C++ plugins could also be considered.

The current WASI one-two workaround strategy involves surgically dismantling certain libc header internals in order to:
- prevent common C++ system headers from becoming entangled on (1) assumptions
  - note: this proves counter-intuitive all over the place, depending on toolchain and sysroots being used
  - however, using stuff like std::string *without* implying filesystem access seems reasonable
- polyfill remaining holes
  - preventing truly `undefined symbols`
  - preventing "irrelevant" `wasi_snapshot_preview1.*` imports from becoming injected
  - providing best guess fallbacks for emergent dependencies like "abort()"
- apply `#pragma weak xyz` patchwork in order to turn insistent WASI feautures into no-ops 

Fortunately, none of this seems to require *actually modifying* toolchain sources or system headers. Instead, inspecting the relationships and existing headers provides a way to anticipate and rework the dependency graph.

Only where a resulting .wasm module literally imports WASI symbols from the host (`wasi_snapshot_preview1.*` etc.) can it be dependent on those features, which along with successful compilation and linking provide a reliable way to assess whether the workaround strategy has been sucecssful.

## Troubleshooting the workaround

Different C++ compiler flags and optimization passes can dramatically affect the emergent .wasm import/export surface area -- therefore compiling with -O0 and -g are recommended when trying to use other C++ system headers that suddenly lead to runtime WASI expectations. Fortunately, using `wasm-dis` makes it simple to verify whether any given `.wasm` artifact has become dependent on particular WASI features:

```sh
$ wasm-dis plugin.wasm | grep -E '[(]import'
(import "wasi_snapshot_preview1" "fd_write" (func $fimport$11 (param i32 i32 i32 i32) (result i32)))
```

(`wasm-dis` is a tool available from [binaryen](https://github.com/WebAssembly/binaryen) 

### Configs

Configs are key-value pairs that can be passed in by the host when creating a
plug-in. These can be useful to statically configure the plug-in with some data that exists across every function call. Here is a trivial example using `host.config.get`:

```c
#define EXTISM_IMPLEMENTATION
#include "extism-pdk.hpp"

#include <cstdint>
#include <string>

constexpr const auto Greeting = "Hello";

int32_t EXTISM_EXPORTED_FUNCTION(greet) {
  host.output(Greeting + ", " + host.config.get("user"));
  return 0;
}
```

To test it, the [Extism CLI](https://github.com/extism/cli) has a `--config` option that lets you pass in `key=value` pairs:


```bash
extism call plugin.wasm greet --config user=tim
# => Hello, tim
```

### Variables

Variables are another key-value mechanism but it's a mutable data store that
will persist across function calls. These variables will persist as long as the
host has loaded and not freed the plug-in. 
You can use `host.var.get`, and `host.var.set` to manipulate vars:

```c
#define EXTISM_IMPLEMENTATION
#include "extism-pdk.hpp"

#include <cstdint>
#include <string>

int32_t EXTISM_EXPORTED_FUNCTION(count) {
  uint64_t count = host.var.get<uint64_t>("count", 0 /* default value */);
  count++;
  host.var.set<uint64_t>("count", count + 1);
  host.console.log("count is now: %lu\n", count);
  return 0;
}
```

### Logging

The `host.console.*` functions can be used to emit logs:

```c
#define EXTISM_IMPLEMENTATION
#include "extism-pdk.hpp"

#include <cstdint>
#include <string>

int32_t EXTISM_EXPORTED_FUNCTION(log_stuff) {
  host.console.info("Hello!");
  host.console.debug("Hello!");
  host.console.warn("Hello!");
  host.console.error("Hello!");
  host.console.warn("Hello! input=%s", host.input<std::string>().c_str());
  return 0;
}
```

Running it, you need to pass a log-level flag:

```
extism call plugin.wasm log_stuff --log-level=info
# => 2023/10/17 14:25:00 Hello!
# => 2023/10/17 14:25:00 Hello!
# => 2023/10/17 14:25:00 Hello!
# => 2023/10/17 14:25:00 Hello!
# => 2023/10/17 14:25:00 Hello!
# => 2023/10/17 14:25:00 Hello!
```

### HTTP

HTTP calls can be made using `host.http.request`: 

```c
#define EXTISM_IMPLEMENTATION
#include "extism-pdk.hpp"

#include <cstdint>
#include <string>

int32_t EXTISM_EXPORTED_FUNCTION(call_http) {

#ifdef EXTISM_USE_LIBCPP // for std::initializer_list
  auto res = host.http.request({
    { "method", "GET" },
    { "url", "https://jsonplaceholder.typicode.com/todos/1" }
  }, {
    { "x-custom-header", "custom header value" },
  });
#else
  constexpr const auto reqStr = R"json(
    {
      "method": "GET",
      "url" : "https://jsonplaceholder.typicode.com/todos/1"
    }
  )json";
  auto res = host.http.request(reqStr, nullptr);
#endif

  if (res.status != 200) {
    return -1;
  }
  host.output(res.body);
  return 0;
}
```

To test it you will need to pass `--allow-host jsonplaceholder.typicode.com` to the `extism` CLI, otherwise the HTTP request will
be rejected.

## Imports (Host Functions)

Like any other code module, Wasm not only let's you export functions to the outside world, you can
import them too. Host Functions allow a plug-in to import functions defined in the host. For example,
if you host application is written in Python, it can pass a Python function down to your C plug-in
where you can invoke it.

This topic can get fairly complicated and we have not yet fully abstracted the Wasm knowledge you need
to do this correctly. So we recommend reading out [concept doc on Host Functions](https://extism.org/docs/concepts/host-functions) before you get started.

### A Simple Example

Host functions have a similar interface as exports. You just need to declare them as `extern` on the top of your header file. You only declare the interface as it is the host's responsibility to provide the implementation:

```c
extern ExtismHandle a_python_func(ExtismHandle);
```

A namespace may be set for an import using the `IMPORT` macro in `extism-pdk.h`:

```c
IMPORT("my_module", "a_python_func") extern ExtismHandle a_python_func(ExtismHandle);
```

> **Note**: The types we accept here are the same as the exports as the interface also uses the [convert crate](https://docs.rs/extism-convert/latest/extism_convert/).

To call this function, we pass an Extism handle and receive one back:

```c
#define EXTISM_IMPLEMENTATION
#include "extism-pdk.hpp"

#include <cstdint>
#include <string>

int32_t EXTISM_EXPORTED_FUNCTION(hello_from_python) {
  auto res = a_python_func("Hello");
  host.output(res);
  return 0;
}
```

### Testing it out

We can't really test this from the Extism CLI as something must provide the implementation. So let's
write out the Python side here. Check out the [docs for Host SDKs](https://extism.org/docs/concepts/host-sdk) to implement a host function in a language of your choice.

```python
from extism import host_fn, Plugin

@host_fn()
def a_python_func(input: str) -> str:
    # just printing this out to prove we're in Python land
    print("Hello from Python!")

    # let's just add "!" to the input string
    # but you could imagine here we could add some
    # applicaiton code like query or manipulate the database
    # or our application APIs
    return input + "!"
```

Now when we load the plug-in we pass the host function:
 
```python
manifest = {"wasm": [{"path": "/path/to/plugin.wasm"}]}
plugin = Plugin(manifest, functions=[a_python_func], wasi=True)
result = plugin.call('hello_from_python', b'').decode('utf-8')
print(result)
```

```bash
python3 app.py
# => Hello from Python!
# => An argument to send to Python!
```

## Building

One source file must contain the implementation:

```c
#define EXTISM_IMPLEMENTATION
#include "extism-pdk.hpp"
```

All other source files using the pdk must include the header without `#define EXTISM_IMPLEMENTATION`

TODO: still deciding on whether stuff like std::string makes more sense as a baseline for C++ (compared to C it may not make much sense to imagine NOT depending on at minimum libc++).
~~The C++ PDK does not require building with `libc++`, but additional functions can be enabled when `libc++` is available. `#define EXTISM_USE_LIBCPP` in each file before including the pdk (everywhere it is included) or, when compiling, pass it as a flag to clang: `-D EXTISM_USE_LIBCPP`~~

## Exports (details)

The `EXTISM_EXPORTED_FUNCTION` macro is not essential to create a plugin function and export it to the host. You may instead write a function and then export it when linking. For example, the first example may have the following signature instead:

```c++
extern "C" int32_t greet(void)
```

Then, it can be built and linked with:

```bash
$WASI_SDK_PATH/bin/clang++ -o plugin.wasm --target=wasm32-unknown-unknown -nostdlib -Wl,--no-entry -Wl,--export=greet plugin.cpp
```

Note the `-Wl,--export=greet`

Exports names do not necessarily have to match the function name either. Going back to the first example again. Try:

```c
EXTISM_EXPORT_AS("greet") int32_t internal_name_for_greet(void)
```

and build with:

```bash
$WASI_SDK_PATH/bin/clang++ -o plugin.wasm --target=wasm32-wasi -nostdlib -Wl,--no-entry plugin.cpp
```

## Reach Out!

Have a question or just want to drop in and say hi?
