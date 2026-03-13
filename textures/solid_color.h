#ifndef SOLID_COLOR_H
#define SOLID_COLOR_H

#include "texture.h"

class SolidColor : public Texture {
public: 
  Vec3 color;
  SolidColor() {}
  SolidColor(const Vec3 &c) : color(c) {}

  inline Vec3 value(double u, double v, const Vec3 &p, double ray_t = 0.0) const override {return color;}
};

#endif