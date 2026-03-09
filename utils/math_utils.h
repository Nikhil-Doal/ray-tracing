#pragma once
#include <cmath>

const double PI = 3.1415926535897932385;
double degrees_to_radians(double degrees) {
    return degrees * PI / 180.0;
}
double radians_to_degrees(double radians) {
    return radians * 180.0 / PI;
}

double random_double(){
  return rand() / (RAND_MAX + 1.0);
}

double random_double(double lower, double upper) {
  return lower + (upper - lower) * (rand() / (RAND_MAX + 1.0));
}