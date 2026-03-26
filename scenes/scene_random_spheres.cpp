#include "scene_registry.h"
#include "../objects/sphere.h"
#include "../materials/lambertian.h"
#include "../materials/metal.h"
#include "../materials/dielectric.h"
#include "../materials/diffuse_light.h"
#include "../textures/solid_color.h"
#include "../lights/directional_light.h"
#include "../lights/area_light.h"
#include "../utils/math_utils.h"

static SceneDesc build_random_spheres() {
  SceneDesc scene;

  scene.width  = 1920;
  scene.height = 1080;
  scene.samples_per_pixel = 500;
  scene.max_depth = 24;

  scene.lookfrom = Vec3(13, 2, 3);
  scene.lookat   = Vec3(0, 0, 0);
  scene.vfov     = 20.0;
  scene.aperture = 1;

  // HDR sky for environment lighting (gradient fallback if file missing)
  scene.sky_hdr_path  = "../../assets/sky.hdr";
  scene.sky_intensity = 1.0f;

  // Sun-like directional light for clean shadows
  scene.lights.push_back(std::make_shared<DirectionalLight>(
      Vec3(-1, -1, -0.5), Vec3(1.0, 0.95, 0.9), 2.0));

  // Ground
  scene.world.add(std::make_shared<Sphere>(
      Vec3(0, -1000, 0), 1000,
      std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(0.5, 0.5, 0.5)))));

  for (int a = -11; a < 11; ++a) {
    for (int b = -11; b < 11; ++b) {
      double choose_mat = random_double();
      Vec3 center(a + 0.9 * random_double(), 0.2, b + 0.9 * random_double());

      if ((center - Vec3(4, 0.2, 0)).norm() > 0.9) {
        std::shared_ptr<Material> mat;
        if (choose_mat < 0.8) {
          Vec3 albedo = Vec3::random() * Vec3::random();
          mat = std::make_shared<Lambertian>(std::make_shared<SolidColor>(albedo));
        } else if (choose_mat < 0.95) {
          Vec3 albedo = Vec3::random(0.5, 1);
          double fuzz = random_double() * 0.5;
          mat = std::make_shared<Metal>(std::make_shared<SolidColor>(albedo), fuzz);
        } else {
          mat = std::make_shared<Dielectric>(1.5);
        }
        scene.world.add(std::make_shared<Sphere>(center, 0.2, mat));
      }
    }
  }

  // Three hero spheres
  scene.world.add(std::make_shared<Sphere>(
      Vec3(0, 1, 0), 1.0, std::make_shared<Dielectric>(1.5)));
  scene.world.add(std::make_shared<Sphere>(
      Vec3(-4, 1, 0), 1.0,
      std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(0.4, 0.2, 0.1)))));
  scene.world.add(std::make_shared<Sphere>(
      Vec3(4, 1, 0), 1.0,
      std::make_shared<Metal>(std::make_shared<SolidColor>(Vec3(0.7, 0.6, 0.5)), 0.0)));

  return scene;
}

REGISTER_SCENE("random_spheres", build_random_spheres);