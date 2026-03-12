#ifndef TEXTURE_H
#define TEXTURE_H
#include "../core/vec3.h"

class Texture {
public:
  virtual Vec3 value(double u, double v, const Vec3 &point) const = 0;
};

#endif