#include <iostream>
#include "vec3.h"
#include "ray.h"

int main() {
  Vec3 a(1, 2, 3);
  Vec3 b(4, 5, 6);

  Vec3 c = a + b;

  std::cout << c.x << " " << c.y << " " << c.z << std::endl;

  Vec3 origin(0, 0, 0);
  Vec3 direction(1, 0, 0);
  Ray ray(origin, direction);
  Vec3 point = ray.at(5);

  std::cout << point.x << " " << point.y << " " << point.z << std::endl;

}