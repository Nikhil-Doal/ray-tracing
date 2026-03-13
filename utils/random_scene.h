#include "../materials/metal.h"
#include "../materials/lambertian.h"
#include "../materials/dielectric.h"
#include "../objects/hittable_list.h"
#include "../objects/sphere.h"
#include "../utils/math_utils.h"
#include "../textures/solid_color.h"
#include <memory>

inline HittableList random_scene() {
  HittableList world;
  world.add(std::make_shared<Sphere>(Vec3(0, -1000, 0), 1000, std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(0.5, 0.5, 0.5)))));

  for (int a = -11; a < 11; ++a) {
    for (int b = -11; b < 11; ++b) {
      double choose_mat = random_double();
      Vec3 center(a + 0.9 * random_double(), 0.2, b + 0.9*random_double());
      
      if ((center - Vec3(4, 0.2, 0)).norm() > 0.9) {
        std::shared_ptr<Material> sphere_mat;

        if (choose_mat < 0.8) {
          Vec3 albedo = Vec3::random() * Vec3::random();
          sphere_mat = std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(albedo)));
        } else if (choose_mat < 0.95){
          Vec3 albedo = Vec3::random(0.5, 1);
          double fuzz = random_double() * 0.5;
          sphere_mat = std::make_shared<Metal>(std::make_shared<SolidColor>(Vec3(albedo)), fuzz);
        } else {
          sphere_mat = std::make_shared<Dielectric>(1.5);
        }

        world.add(std::make_shared<Sphere>(center, 0.2, sphere_mat));
      }
    }
  }
  world.add(std::make_shared<Sphere>(Vec3(0, 1, 0), 1.0, std::make_shared<Dielectric>(1.5)));
  world.add(std::make_shared<Sphere>(Vec3(-4, 1, 0), 1.0, std::make_shared<Lambertian>(std::make_shared<SolidColor>(Vec3(1, 0, 0)))));
  world.add(std::make_shared<Sphere>(Vec3(4, 1, 0), 1.0, std::make_shared<Metal>(std::make_shared<SolidColor>(Vec3(0.7, 0.6, 0.5)), 0.0)));

  return world;
}
