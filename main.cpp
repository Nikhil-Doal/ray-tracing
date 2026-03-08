#include <iostream>

int main() {
  int width = 400;
  int height = 200;
  std::cout << "P3\n" << width << " " << height << "\n255\n";

  for (int j = height - 1; j >= 0; --j) {
    for (int i = 0; i < width; ++i) {
      float r = float(i) / width;
      float g = float(j) / height;
      float b = 0.25;

      int ir = int(255.99 * r);
      int ig = int(255.99 * g);
      int ib = int(255.99 * b);
    
      std::cout << ir << " " << ig << " " << ib << "\n";
    }
  }
}