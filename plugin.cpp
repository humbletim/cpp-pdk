#define EXTISM_IMPLEMENTATION
#include "extism-pdk.hpp"

#include <array>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

struct AttributeSet {
  glm::vec3 position;
  glm::vec3 color;
};
using Attributes = std::array<AttributeSet, 256>;

Attributes attributes; // stack allocated
ExtismHandle _attributes = extism_alloc(sizeof(attributes)); // handle allocated
EXTISM_IMPORT("env", "notify") void env_notify(ExtismHandle); // notify host of handular updates

extern "C" int32_t hello() {
  static int N=0;

  attributes[0].position.x = N++;
  self.console.warn("+STORE a0.position=%s\n", glm::to_string(attributes[0].position).c_str());
  {
    extism_store_to_handle(_attributes, 0, &attributes[0], sizeof(attributes));
    env_notify(_attributes);
    extism_load_from_handle(_attributes, 0, &attributes[0], sizeof(attributes));
  }
  self.console.warn("-LOAD a0.position=%s\n", glm::to_string(attributes[0].position).c_str());
  return 0;
}
