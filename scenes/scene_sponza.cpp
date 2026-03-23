#include "scene_registry.h"
#include "../objects/sphere.h"
#include "../objects/triangle.h"
#include "../objects/obj_loader.h"
#include "../materials/lambertian.h"
#include "../materials/glossy.h"
#include "../materials/diffuse_light.h"
#include "../textures/image_texture.h"
#include "../textures/solid_color.h"
#include "../lights/directional_light.h"
#include "../lights/area_light.h"

static SceneDesc build_sponza() {
  SceneDesc scene;

  scene.width  = 1920;
  scene.height = 1080;
  scene.samples_per_pixel = 1000;
  scene.max_depth = 60;

  // Camera — classic view down the atrium length
  scene.lookfrom = Vec3(-10.0, 2.0, 0.3);
  scene.lookat   = Vec3(0.0, 4.5, 0.0);
  scene.vfov     = 75.0;
  scene.aperture = 0.0;

  // HDR environment — sunlit courtyard
  scene.sky_hdr_path  = "../../assets/sky.hdr";
  scene.sky_intensity = 1.5f;

  // Directional sun — angled to cast shafts through the open roof
  scene.lights.push_back(std::make_shared<DirectionalLight>(
      Vec3(0.3, -2.0, -0.5), Vec3(1.0, 0.93, 0.82), 3.0));

  // Warm fill from the opposite side
  scene.lights.push_back(std::make_shared<DirectionalLight>(
      Vec3(-0.4, -0.6, 0.7), Vec3(0.6, 0.65, 0.8), 0.5));

  // Fallback material (used when MTL materials fail to load)
  auto fallback = std::make_shared<Lambertian>(
      std::make_shared<SolidColor>(Vec3(0.6, 0.55, 0.5)));

  // ---- Load Sponza ----
  // Dabrovic Sponza: ~28 units long (X), ~12 tall (Y), ~12 wide (Z), base at Y=0
  std::string sponza_path = "../../assets/models/sponza/sponza.obj";

  auto tris = load_obj_triangle(sponza_path, fallback, 1.0, Vec3(0, 0, 0));
  std::cout << "Sponza: " << tris.size() << " triangles loaded\n";
  for (auto &t : tris) scene.world.add(t);

  return scene;
}

REGISTER_SCENE("sponza", build_sponza);