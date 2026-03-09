#include <iostream>
#include <cmath>
#include "../core/vec3.h"
#include "../core/ray.h"
#include "../core/hittable.h"
#include "../objects/sphere.h"
#include "../objects/hittable_list.h"
#include "../materials/material.h"
#include "../materials/lambertian.h"


Vec3 ray_color(const Ray &r, const Hittable &world, int depth);

int main() {
  // Making the scene
  HittableList world;
  // Materials
  Lambertian ground_mat(Vec3(0.8, 0.8, 0.0));
  Lambertian sphere1_mat(Vec3(0.7, 0.3, 0.3));
  Sphere sphere1(Vec3(0,0,-1), 0.5, &sphere1_mat);
  Sphere ground(Vec3(0,-100.5,-1), 100, &ground_mat); // we can make the ground using another sphere for now
  world.add(&sphere1);
  world.add(&ground);

  int width = 400;
  int height = 200;
  std::cout << "P3\n" << width << " " << height << "\n255\n";
  
  Vec3 origin(0,0,0);
  double aspect_ratio = double(width)/height;

  double viewport_height = 2.0; // 2n - 1 mapping 0,1 to -1,1
  double viewport_width = aspect_ratio * viewport_height;
  double focal_length = 1.0; // the -1 

  Vec3 horizontal(viewport_width, 0, 0);
  Vec3 vertical(0, viewport_height, 0);

  Vec3 lower_left_corner = origin - horizontal/2 - vertical/2 - Vec3(0, 0, focal_length);

  for (int j = height - 1; j >= 0; --j) {
    for (int i = 0; i < width; ++i) {
      double u = double(i) / (width - 1);
      double v = double(j) / (height - 1);
      
      Vec3 direction = lower_left_corner + horizontal*u + vertical*v - origin;
      Ray ray(origin, direction);
      Vec3 color = ray_color(ray, world, 50);
      
      int ir = int(255.99 * color.x);
      int ig = int(255.99 * color.y);
      int ib = int(255.99 * color.z);

      std::cout << ir << " " << ig << " " << ib << "\n";
    }
  }
}


Vec3 ray_color(const Ray &r, const Hittable &world, int depth) {
  if (depth <= 0) return Vec3(0,0,0);
  HitRecord rec;

  if (world.hit(r, 0.001, 1000, rec)) {
    Ray scattered;
    Vec3 attenuation;

    if (rec.mat->scatter(r, rec, attenuation, scattered)) {
      return attenuation * (scattered, world, depth-1);
    }

    return Vec3(0,0,0);
  }


  Vec3 unit = r.direction.normalize();
  double t = 0.5*(unit.y + 1.0);

  return Vec3(1,1,1)*(1.0-t) + Vec3(0.5,0.7,1.0)*t; // sky gradient
}