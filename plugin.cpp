#define EXTISM_IMPLEMENTATION
#include "extism-pdk.hpp"

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

static constexpr const auto Greeting = "Hello";

int32_t EXTISM_EXPORTED_FUNCTION(greet) {
  std::string input = self.input<std::string>();
  std::string config = self.config("user");
  std::string var = self.var.get("var");
  self.console.warn("input=%s config.user=%s var.var=%s // var.var=%s\n", input.c_str(), config.c_str(), var.c_str());
  self.formatOutput("%s, %s", Greeting, input.c_str());
  return 0; 
}

glm::vec3 glm_vec3_test(glm::vec3 const& vec3) {
  self.console.warn("glm_vec3_test() vec3=%s\n", glm::to_string(vec3).c_str());
  return vec3 * 10.0f;
}

extern "C" int glm_vec3_test() {
  glm::vec3 vec3;
  fprintf(stderr, "TOOLCHAIN: %s\n", __toolchain__);
  self.console.warn("var.foo=='%s' @ %s\n", self.var.get("foo").c_str(), __FUNCTION__);
  self.console.warn("config.foo=='%s' @ %s\n", self.config("foo").c_str(), __FUNCTION__);
  if (extism_input_length() == sizeof(glm::vec3)) {
    self.console.warn("extism_input_length == %d\n", extism_input_length());
    self.output<glm::vec3>(
      glm_vec3_test(self.input<glm::vec3>())
    );
    return 0;
  }
  self.error.format("unrecognized sizeof(input) %d\n", extism_input_length());
  return 1;
}

extern "C" int32_t hello() {
  fprintf(stderr, "TOOLCHAIN: %s\n", __toolchain__);

  std::string tmp{ typeid(glm::vec3).name() };
  self.console.warn("hello! %p %s\n", hello, tmp.c_str());
  glm::vec3 vec3;
  if (extism_input_length() == sizeof(glm::vec3)) {
    self.output<glm::vec3>(glm_vec3_test(self.input<glm::vec3>()));
    return 0;
  }
  glm_vec3_test({1,2,3});
  return 0;
}


