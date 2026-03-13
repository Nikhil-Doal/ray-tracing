#include "image_texture.h"
#include <algorithm>
#include <cmath>
#include <iostream>

ImageTexture::ImageTexture(const std::string &filename) {
  data = stbi_load(filename.c_str(), &width, &height, &channels, 3);
  if (!data) {
    std::cerr << "Failed to load texture: " << filename << "\n";
    width = height = channels = 0;
  }
  generate_mipmaps();
}

ImageTexture::~ImageTexture() {
  for (size_t i = 1; i < mipmaps.size(); ++i) {
      delete [] mipmaps[i].data;
    }
  if (data) stbi_image_free(data);
}

Vec3 ImageTexture::value(double u, double v, const Vec3 &point, double ray_t) const {
  if (!data) return Vec3(0, 1, 1); // random debugging color
  int max_level = (int)mipmaps.size() -1 ;
  
  // we want to see where texels to pixels is approx 1:1 but we can't do the du/dv in rasterizers
  // using the following approximation, where ray_t is world-space distance, width gives texel density

  double texels_per_pixel = ray_t * 0.2;
  int level = (int)log2(std::max(texels_per_pixel, 1.0));
  level = std::clamp(level, 0, max_level);

  const MipLevel &mip = mipmaps[level];
  
  // bilinear filtering
  int w = mip.width;
  int h = mip.height;
  unsigned char* tex = mip.data;

  u = std::clamp(u, 0.0, 1.0);
  v = 1.0 - std::clamp(v, 0.0, 1.0);
  
  double i = u * (w - 1);
  double j = v * (h - 1);

  int i0 = (int) i;
  int j0 = (int) j;

  int i1 = std::min(i0 + 1, w - 1);
  int j1 = std::min(j0 + 1, h - 1);

  double tx = i - i0;
  double ty = j - j0;
  
  auto srgb_to_linear = [](double x) {
    return pow(x, 2.2);
  };

  auto get_pixel = [&](int i, int j) {
    int index = (j * w + i) * 3;
    return Vec3(srgb_to_linear(tex[index] / 255.0), srgb_to_linear(tex[index + 1] / 255.0), srgb_to_linear(tex[index + 2] / 255.0));
  };
  
  Vec3 c00 = get_pixel(i0, j0);
  Vec3 c10 = get_pixel(i1, j0);
  Vec3 c01 = get_pixel(i0, j1);
  Vec3 c11 = get_pixel(i1, j1);

  Vec3 c0 = c00*(1-tx) + c10*tx;
  Vec3 c1 = c01*(1-tx) + c11*tx;

  return c0*(1-ty) + c1*ty;
}

void ImageTexture::generate_mipmaps() {
  mipmaps.push_back({data, width, height});
  int w = width;
  int h = height;
  unsigned char *prev = data;

  while(w > 1 && h > 1 ) {
    int new_w = w/2;
    int new_h = h/2;
    unsigned char* next = new unsigned char[new_w * new_h * 3];
    for (int y = 0; y < new_h; ++y) {
      for (int x = 0; x < new_w; x++) {
        int i = ((2*y) * w + (2*x)) * 3;
        for (int c = 0; c < 3; ++c) {
          int avg = prev[i + c] + prev[i + 3 + c] + prev[i + w*3 + c] + prev[i + w*3 + 3 + c];
          next[(y * new_w + x) * 3 + c] = avg / 4;
        }
      }
    }
    mipmaps.push_back({next, new_w, new_h});
    
    prev = next; 
    w = new_w;
    h = new_h;
  }
}