#pragma once
#include <iostream>
#include "mesh.h"
#include "../external/tinyobjloader/tiny_obj_loader.h"

Mesh *load_obj(const std::string &filename, Material *mat, double scale, Vec3 offset);
std::vector <Triangle*> load_obj_triangle(const std::string &path, Material *mat, double scale=1.0, Vec3 offset=Vec3(0,0,0));