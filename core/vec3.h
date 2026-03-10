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





// constructors
Vec3::Vec3() : x(0), y(0), z(0) {}
Vec3::Vec3(double x, double y, double z) : x(x), y(y), z(z) {}

//basic operators
Vec3 Vec3::operator+(const Vec3 &v) const {
  return Vec3(x + v.x, y + v.y, z + v.z);
}
Vec3 Vec3::operator-(const Vec3 &v) const {
  return Vec3(x - v.x, y - v.y, z - v.z);    
}
Vec3 Vec3::operator*(double t) const {
  return Vec3(x * t, y * t, z * t);
}
Vec3 Vec3::operator/(double t) const {
  return Vec3(x / t, y / t, z / t);    
}
Vec3 Vec3::operator*(const Vec3 &v) const {
  return Vec3(x * v.x, y * v.y, z * v.z); 
}

// products
double Vec3::dot(Vec3 const &v) const {
  return x*v.x + y*v.y + z*v.z;
}
Vec3 Vec3::cross(Vec3 const &v) const {
  return Vec3(
    y*v.z - z*v.y,
    z*v.x - x*v.z,
    x*v.y - y*v.x
  );
}

// normalize
double Vec3::norm() const {
  return std::sqrt(x*x + y*y + z*z);
}
Vec3 Vec3::normalize() const {
  return *this/norm();
}

// helper functions
Vec3 reflect(const Vec3 &v, const Vec3 &n) {
  return v - n * (v.dot(n) * 2);
}

Vec3 refract(const Vec3 &uv, const Vec3 &n, double etai_over_etat) {
  double cos_theta = fmin(n.dot(uv * -1), 1.0);
  Vec3 r_out_perp = (uv + n * cos_theta) * etai_over_etat;
  Vec3 r_out_parallel = n * -sqrt(fabs(1.0 - r_out_perp.dot(r_out_perp)));
  return r_out_parallel + r_out_perp;
}

Vec3 random_in_unit_disk() {
  while (true) {
    Vec3 p((rand() / (RAND_MAX + 1.0)) * 2 - 1, (rand() / (RAND_MAX + 1.0)) * 2 - 1, 0);
    if (p.dot(p) >= 1) continue;
    return p;
  } 
}

Vec3 Vec3::random() {
  return Vec3(random_double(), random_double(), random_double());
}
Vec3 Vec3::random(double lower, double upper) {
  return Vec3(random_double(lower, upper), random_double(lower, upper), random_double(lower, upper));
}

double Vec3::operator[](int i) const {
  if (i == 0) return x;
  if (i == 1) return y;
  return z;
}
double& Vec3::operator[](int i) {
  if (i == 0) return x;
  if (i == 1) return y;
  return z;
}