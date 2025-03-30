#ifndef MATH_H
#define MATH_H

#define PI 3.14159265358979323846

double sqrt(double n);
double pow(double base, double exp);
double root(double n, double r);
double abs(double n);
double ceil(double n);
double floor(double n);
double round(double n);
double max(double a, double b);
double min(double a, double b);
double clamp(double n, double min_value, double max_value);
double lerp(double a, double b, double t);
double map(double n, double start1, double stop1, double start2, double stop2);
int sign(double n);
double sin(double n);
double cos(double n);
double tan(double n);
double asin(double n);
double acos(double n);
double atan(double n);
double atan2(double y, double x);
double exp(double n);
double ln(double n);
double log10(double n);
double log(double base, double n);
double deg_to_rad(double deg);
double rad_to_deg(double rad);
double hypot(double x, double y);
double rand(void);
float expf(float x);

#endif
