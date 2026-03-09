#include <iostream>
#include <cmath>
#include "../core/vec3.h"
#include "../core/ray.h"

double hit_sphere(const Vec3 &center, double radius, const Ray &r);
Vec3 ray_color(const Ray &r);

int main() {
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
      // float r = float(i) / width;
      // float g = float(j) / height;
      // float b = 0.25;
      
      // int ir = int(255.99 * r);
      // int ig = int(255.99 * g);
      // int ib = int(255.99 * b);
      double u = double(i) / (width - 1);
      double v = double(j) / (height - 1);
      
      Vec3 direction = lower_left_corner + horizontal*u + vertical*v - origin;
      Ray ray(origin, direction);
      Vec3 color = ray_color(ray);
      
      int ir = int(255.99 * color.x);
      int ig = int(255.99 * color.y);
      int ib = int(255.99 * color.z);

      std::cout << ir << " " << ig << " " << ib << "\n";
    }
  }
}

double hit_sphere(const Vec3 &center, double radius, const Ray &ray) {
  // |x - C|^2 = r^2 -> |O + tD - C|^2 = r^2 
  // t^2(D⋅D) + 2t(OC⋅D) + (OC⋅OC - r^2) = 0 //now look at the discriminant
  Vec3 oc = ray.origin - center;

  double a = ray.direction.dot(ray.direction);
  double b = 2 * oc.dot(ray.direction);
  double c = oc.dot(oc) - radius*radius;

  double discriminant = b*b - 4*a*c;
  if (discriminant < 0) return -1.0;
  else return (-b - std::sqrt(discriminant))/(2*a);


}

Vec3 ray_color(const Ray &r) {
  Vec3 center(0, 0, -1);
  double t = hit_sphere(center, 0.5, r);
  if (t > 0.0) {
    // based on normalization return the color for shading
    Vec3 hit_pt = r.at(t);
    Vec3 normal = (hit_pt - center).normalize();
  
    return Vec3(normal.x + 1, normal.y + 1, normal.z + 1) * 0.5;
  }
  return Vec3(0.5, 0.7, 1); // some background color
}