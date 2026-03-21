#include "scene_registry.h"
#include <iostream>

std::map<std::string, SceneBuilder> &SceneRegistry::registry() {
  static std::map<std::string, SceneBuilder> reg;
  return reg;
}

void SceneRegistry::add(const std::string &name, SceneBuilder builder) {
  registry()[name] = builder;
}

SceneDesc SceneRegistry::build(const std::string &name) {
  auto &reg = registry();
  auto it = reg.find(name);
  if (it != reg.end()) {
    return it->second();
  }
  std::cerr << "Scene '" << name << "' not found. Available scenes:\n";
  for (auto &kv : reg) {
    std::cerr << "  " << kv.first << "\n";
  }
  return SceneDesc{}; // empty fallback
}

std::vector<std::string> SceneRegistry::list() {
  std::vector<std::string> names;
  for (auto &kv : registry()) {
    names.push_back(kv.first);
  }
  return names;
}