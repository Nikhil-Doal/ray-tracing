#pragma once
#include <iostream>
#include "mesh.h"
#include "../external/tinyobjloader/tiny_obj_loader.h"
#include <memory>
#include "../textures/image_texture.h"
#include "../textures/solid_color.h"
#include "../materials/lambertian.h"
#include "../materials/metal.h"
#include "../materials/dielectric.h"

std::vector <std::shared_ptr<Triangle>> load_obj_triangle(const std::string &path, std::shared_ptr<Material> mat, double scale=1.0, Vec3 offset=Vec3(0,0,0));