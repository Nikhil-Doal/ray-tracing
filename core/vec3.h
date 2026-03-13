// define a 3d vector class
#pragma once
#include <cmath>
#include "../utils/math_utils.h"

class Vec3 {
public:
  double x, y, z;
  Vec3();
  Vec3(double x, double y, double z);

  Vec3 operator+(const Vec3 &v) const;
  Vec3 operator-(const Vec3 &v) const;
  Vec3 operator*(double t) const;
  Vec3 operator/(double t) const;
  Vec3 operator*(const Vec3 &v) const;

  double dot(Vec3 const &v) const;
  Vec3 cross(Vec3 const &v) const;

  double norm() const;
  Vec3 normalize() const;
  
  static Vec3 random();
  static Vec3 random(double lower, double upper);

  double operator[](int i) const;
  double &operator[](int i);
};

// helper declarations
Vec3 reflect(const Vec3 &v, const Vec3 &n);
Vec3 refract(const Vec3 &uv, const Vec3 &n, double etai_over_etat);
Vec3 random_in_unit_disk();
