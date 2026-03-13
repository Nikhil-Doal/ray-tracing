#pragma once 
#include "texture.h"
#include "../external/stbimagewrite/stb_image.h"
#include "../core/vec3.h"
#include <iostream>

class ImageTexture : public Texture {
public:

  struct MipLevel {
    unsigned char *data;
    int width;
    int height;
  };
  void generate_mipmaps();
  std::vector <MipLevel> mipmaps;

  unsigned char *data;
  int width, height, channels;

  ImageTexture(const std::string &filename);
  ~ImageTexture();
  Vec3 value(double u, double v, const Vec3 &point, double ray_t = 0.0) const override;
};
