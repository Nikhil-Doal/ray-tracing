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
#include "../lights/point_light.h"
#include "../lights/area_light.h"

static SceneDesc build_san_miguel() {
  SceneDesc scene;

  scene.width  = 1920;
  scene.height = 1080;
  scene.samples_per_pixel = 500;
  scene.max_depth = 20;
  // scene.width  = 300;
  // scene.height = 200;
  // scene.samples_per_pixel = 10;
  // scene.max_depth = 10;

  // Camera — classic view down the atrium length
  scene.lookfrom = Vec3(22.51, 1.6, -2);
  scene.lookat   = Vec3(13.51, 1.6, 6.0);
  scene.vfov     = 75.0;
  scene.aperture = 0.0;

  // HDR environment — sunlit courtyard
  scene.sky_hdr_path  = "../../assets/sky.hdr";
  scene.sky_intensity = 8.5f;

  // Directional sun — angled to cast shafts through the open roof
  scene.lights.push_back(std::make_shared<DirectionalLight>(
      Vec3(-1.0, -6.0, 0.0), Vec3(1.0, 0.93, 0.82), 5.0));
  // scene.lights.push_back(std::make_shared<DirectionalLight>(
  //     Vec3(0, -8.4, 3), Vec3(1.0, 0.93, 0.82), 4.0));

  // Warm fill from the opposite side
  scene.lights.push_back(std::make_shared<DirectionalLight>(
      Vec3(-0.4, -0.6, 0.7), Vec3(0.6, 0.65, 0.8), 0.5));

  // scene.lights.push_back(std::make_shared<PointLight>(Vec3(26.0, 1.6, -2.0), Vec3(1.0, 0.93, 0.82), 2.0));
  // auto light_mat = std::make_shared<DiffuseLight>(Vec3(0.6, 0.65, 0.8) * 2.0);
  // scene.world.add(std::make_shared<Sphere>(Vec3(14.75, 3.2, -1.65), 0.5, light_mat));
  // scene.world.add(std::make_shared<Sphere>(Vec3(18.25, 3.2, -1.65), 0.5, light_mat));
  // scene.world.add(std::make_shared<Sphere>(Vec3(11.25, 3.2, -1.65), 0.5, light_mat));
  // scene.world.add(std::make_shared<Sphere>(Vec3(7.75, 3.2, -1.65),  0.5, light_mat));
  
  scene.lights.push_back(std::make_shared<SphereAreaLight>(Vec3(14.75, 3.2, -1.65), 1.0, Vec3(0.6, 0.65, 0.8), 2.0));
  scene.lights.push_back(std::make_shared<SphereAreaLight>(Vec3(18.25, 3.2, -1.65), 1.0, Vec3(0.6, 0.65, 0.8), 2.0));
  scene.lights.push_back(std::make_shared<SphereAreaLight>(Vec3(11.25, 3.2, -1.65), 1.0, Vec3(0.6, 0.65, 0.8), 2.0));
  scene.lights.push_back(std::make_shared<SphereAreaLight>(Vec3(7.75, 3.2, -1.65), 1.0, Vec3(0.6, 0.65, 0.8), 2.0));

  // Fallback material (used when MTL materials fail to load)
  auto fallback = std::make_shared<Lambertian>(
      std::make_shared<SolidColor>(Vec3(0.6, 0.55, 0.5)));

  // ---- Load Sponza ----
  // Dabrovic Sponza: ~28 units long (X), ~12 tall (Y), ~12 wide (Z), base at Y=0
  std::string sponza_path = "../../assets/models/San_Miguel/san-miguel.obj";

  auto tris = load_obj_triangle(sponza_path, fallback, 1.0, Vec3(0, 0, 0));
  std::cout << "San Miguel: " << tris.size() << " triangles loaded\n";
  for (auto &t : tris) scene.world.add(t);

  return scene;
}

REGISTER_SCENE("san_miguel", build_san_miguel);