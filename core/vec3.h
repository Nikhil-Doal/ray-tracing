// define a 3d vector class
#pragma once
#include <cmath>

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

  Vec3 reflect(const Vec3 &v, const Vec3 &n);
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

// helpers
Vec3 reflect(const Vec3 &v, const Vec3 &n) {
  return v - n * (v.dot(n) * 2);
}