# experimental Extism cpp-pdk ideas

```c++
#define EXTISM_IMPLEMENTATION
#include "extism-pdk.hpp"

static constexpr const auto Greeting = "Hello";

int32_t EXTISM_EXPORTED_FUNCTION(greet) {
  std::string input = self.input<std::string>();
  std::string config = self.config("user");
  std::string var = self.var.get("var");
  self.console.warn("input=%s config.user=%s var.var=%s // var.var=%s\n", input.c_str(), config.c_str(), var.c_str());
  self.formatOutput("%s, %s", Greeting, input.c_str());
  return 0; 
}
```

```bash
extism call plugin.wasm greet --input="tim"
# => Hello, tim
```

- see plugin.cpp for a few additional examples
- `make fetch-linux-sdks` to summon dependencies on linux
- `make test` to compile across emscripten, wasi-sdk, and system clang++
