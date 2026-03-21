#pragma once

#include "scene_desc.h"
#include <functional>
#include <map>
#include <string>
#include <vector>

// A scene builder is a function that fills a SceneDesc.
using SceneBuilder = std::function<SceneDesc()>;

class SceneRegistry {
public:
  // Register a named scene. Call from scene_*.cpp files.
  static void add(const std::string &name, SceneBuilder builder);

  // Build a scene by name. Returns nullopt-like empty SceneDesc on failure.
  static SceneDesc build(const std::string &name);

  // List all registered scene names.
  static std::vector<std::string> list();

private:
  static std::map<std::string, SceneBuilder> &registry();
};

// Convenience macro: place at file scope in a scene_*.cpp to auto-register.
// Usage:
//   REGISTER_SCENE("my_scene", my_scene_function);
#define REGISTER_SCENE(name, func) \
  static struct _SceneReg_##__LINE__ { \
    _SceneReg_##__LINE__() { SceneRegistry::add(name, func); } \
  } _scene_reg_instance_##__LINE__;