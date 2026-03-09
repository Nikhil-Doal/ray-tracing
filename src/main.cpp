#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include "../core/vec3.h"
#include "../core/ray.h"
#include "../core/hittable.h"
#include "../objects/sphere.h"
#include "../objects/hittable_list.h"
#include "../materials/material.h"
#include "../materials/lambertian.h"
#include "../materials/metal.h"
#include "../materials/dielectric.h"
#include "../core/camera.h"

Vec3 ray_color(const Ray &r, const Hittable &world, int depth);

int main() {
  const int samples_per_pixel = 100;
  const int max_depth = 50;
  srand(time(0));

  // Making the scene
  HittableList world;

  // Materials
  Lambertian ground_mat(Vec3(0.8, 0.8, 0.0));
  Lambertian sphere1_mat(Vec3(0.7, 0.3, 0.3));
  Metal metal_mat(Vec3(0.8,0.8,0.8), 0.8);
  Dielectric glass(1.5); 

  // Spheres
  Sphere sphere1(Vec3(0,0,-1), 0.5, &sphere1_mat);
  Sphere ground(Vec3(0,-100.5,-1), 100, &ground_mat); // we can make the ground using another sphere for now
  Sphere metal_sphere(Vec3(1, 0, -1), 0.5, &metal_mat);
  Sphere glass_sphere(Vec3(-1,0, -1), 0.5, &glass);
  Sphere glass_sphere_hollow(Vec3(-1, 0, -1), -0.4, &glass);

  // Adding Spheres
  world.add(&sphere1);
  world.add(&metal_sphere);
  world.add(&ground);
  world.add(&glass_sphere);
  world.add(&glass_sphere_hollow);

  int width = 400;
  int height = 200;
  std::cout << "P3\n" << width << " " << height << "\n255\n";
  
  // // camera settings
  // Vec3 origin(0,0,0);
  double aspect_ratio = double(width)/height;

  // double viewport_height = 2.0; // 2n - 1 mapping 0,1 to -1,1
  // double viewport_width = aspect_ratio * viewport_height;
  // double focal_length = 1.0; // the -1 

  // Vec3 horizontal(viewport_width, 0, 0);
  // Vec3 vertical(0, viewport_height, 0);

  // double aperture = 0.2;
  // double focus_dist = 1.0;

  // double lens_radius = aperture / 2;

  // Vec3 lower_left_corner = origin - horizontal/2 - vertical/2 - Vec3(0, 0, focal_length);

  Vec3 lookfrom(1, 1, 1);
  Vec3 lookat(0, 0, -1);
  Vec3 vup(0, 1, 0);
  double focus_dist = (lookfrom - lookat).norm();
  double aperture = 0.0;

  Camera camera(lookfrom, lookat, vup, 90, aspect_ratio, aperture, focus_dist);


  // Rendering pixels
  for (int j = height - 1; j >= 0; --j) {
    for (int i = 0; i < width; ++i) {
      Vec3 pixel_color(0,0,0);
      for (int s = 0; s < samples_per_pixel; ++s) {
        // Monte Carlo anti-aliasing
        double u = (i + (rand() / (RAND_MAX + 1.0))) / (width - 1);
        double v = (j + (rand() / (RAND_MAX + 1.0))) / (height - 1);

        // Vec3 direction = lower_left_corner + horizontal*u + vertical*v - origin;
        // // Ray ray(origin, direction.normalize());

        // Vec3 rd = random_in_unit_disk() * lens_radius;
        // Vec3 offset(rd.x, rd.y, 0);
        // Ray ray(origin + offset, direction - offset);
        Ray ray = camera.get_ray(u,v);

        pixel_color = pixel_color + ray_color(ray, world, max_depth);
      }

      pixel_color = pixel_color / samples_per_pixel;

      // gamma correction
      pixel_color = Vec3(
          sqrt(pixel_color.x),
          sqrt(pixel_color.y),
          sqrt(pixel_color.z)
      );
      // lambda clamping function to clip large values
      auto clamp = [](double x, double min, double max) {
        if (x < min) return min;
        if (x > max) return max;
        return x;
      };

      int ir = int(256 * clamp(pixel_color.x, 0.0, 0.999));
      int ig = int(256 * clamp(pixel_color.y, 0.0, 0.999));
      int ib = int(256 * clamp(pixel_color.z, 0.0, 0.999));

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
      return ray_color(scattered, world, depth-1) * attenuation;
    }

    return Vec3(0,0,0);
  }


  Vec3 unit = r.direction.normalize();
  double t = 0.5*(unit.y + 1.0);

  return Vec3(1,1,1)*(1.0-t) + Vec3(0.5,0.7,1.0)*t; // sky gradient
}