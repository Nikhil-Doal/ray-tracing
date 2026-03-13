#pragma once
#include <iostream>
#include "mesh.h"
#include "../external/tinyobjloader/tiny_obj_loader.h"
#include <memory>

std::shared_ptr<Mesh> load_obj(const std::string &filename, std::shared_ptr<Material> mat, double scale, Vec3 offset);
std::vector <std::shared_ptr<Triangle>> load_obj_triangle(const std::string &path, std::shared_ptr<Material> mat, double scale=1.0, Vec3 offset=Vec3(0,0,0));