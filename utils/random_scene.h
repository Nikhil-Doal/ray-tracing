#include "../materials/metal.h"
#include "../materials/lambertian.h"
#include "../materials/dielectric.h"
#include "../objects/hittable_list.h"
#include "../objects/sphere.h"
#include "../utils/math_utils.h"

HittableList random_scene() {
  HittableList world;

  Lambertian *ground_mat = new Lambertian(Vec3(0.5, 0.5, 0.5));
  world.add(new Sphere(Vec3(0, -1000, 0), 1000, ground_mat));

  for (int a = -11; a < 11; ++a) {
    for (int b = -11; b < 11; ++b) {
      double choose_mat = random_double();
      Vec3 center(a + 0.9 * random_double(), 0.2, b + 0.9*random_double());
      
      if ((center - Vec3(4, 0.2, 0)).norm() > 0.9) {
        Material *sphere_mat;

        if (choose_mat < 0.8) {
          Vec3 albedo = Vec3::random() * Vec3::random();
          sphere_mat = new Lambertian(albedo);
        } else if (choose_mat < 0.95){
          Vec3 albedo = Vec3::random(0.5, 1);
          double fuzz = random_double() * 0.5;
          sphere_mat = new Metal(albedo, fuzz);
        } else {
          sphere_mat = new Dielectric(1.5);
        }

        world.add(new Sphere(center, 0.2, sphere_mat));
      }
    }
  }

  Material *mat1 = new Dielectric(1.5);
  world.add(new Sphere(Vec3(0, 1, 0), 1.0, mat1));
  Material *mat2 = new Lambertian(Vec3(0.4, 0.2, 0.1));
  world.add(new Sphere(Vec3(-4, 1, 0), 1.0, mat2));
  Material *mat3 = new Metal(Vec3(0.7, 0.6, 0.5), 0.0);
  world.add(new Sphere(Vec3(4, 1, 0), 1.0, mat3));

  return world;
}
