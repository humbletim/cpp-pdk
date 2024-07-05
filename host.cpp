#include "extism.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <array>

std::vector<uint8_t> get_file_contents(const char *filename) {
  std::ifstream file(filename, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

struct AttributeSet {
  glm::vec3 position;
  glm::vec3 color;
};
using Attributes = std::array<AttributeSet,256>;
auto notify = [](extism::CurrentPlugin plugin, void*) {
  assert( plugin.memoryLength(plugin.inputVal(0).v.i32) == sizeof(Attributes) );

  Attributes& attributes = *(Attributes*)plugin.memory(plugin.inputVal(0).v.i32);
  static int counter=0;

  std::cout << "notify -- attributes[0].incoming " << glm::to_string(attributes[0].position) << std::endl;
  attributes[0].position = glm::vec3(counter++, 2, 3);
  std::cout << "notify -- attributes[0].outgoing " << glm::to_string(attributes[0].position) << std::endl;
  return 0;
};
extism::Function env_notify{ "env", "notify", {extism::ValType::ExtismValType_I64}, {}, notify};

int main(int argc, char *argv[]) {
  extism::setLogFile("/dev/stdout", "extism=debug");
  extism::Plugin plugin(get_file_contents(argv[1]), false, { env_notify });

  plugin.call("hello", "1");
  plugin.call("hello", "2");
  plugin.call("hello", "3");
  return 0;
}
