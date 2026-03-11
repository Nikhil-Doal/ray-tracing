#pragma once
#include <random>
#include <cmath>

constexpr double PI = 3.1415926535897932385;
inline double degrees_to_radians(double degrees) {
    return degrees * PI / 180.0;
}

inline double radians_to_degrees(double radians) {
    return radians * 180.0 / PI;
}

inline double random_double() {
    thread_local std::mt19937 generator(std::random_device{}());
    thread_local std::uniform_real_distribution<double> distribution(0.0,1.0);
    return distribution(generator);
}

inline double random_double(double lower, double upper) {
  return lower + (upper - lower) * random_double();
}