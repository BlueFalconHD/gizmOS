#include "math.h"
#include "device/rtc.h"
#include <stdint.h>

/**
 * sqrt(n)
 * Calculate the square root of a number n using the Babylonian method.
 * @param n The number to calculate the square root of.
 * @return The square root of n.
 * @example sqrt(16) returns 4.
 */
double sqrt(double n) {
    if (n < 0) {
        return -1; // Undefined for negative numbers
    }
    if (n == 0) {
        return 0;
    }

    double x = n;
    double y = 1.0;
    double epsilon = 0.000001; // Desired precision

    while (x - y > epsilon) {
        x = (x + y) / 2;
        y = n / x;
    }
    return x;
}

/**
 * pow(base, exp)
 * Calculate the power of a base raised to an exponent.
 * @param base The base number.
 * @param exp The exponent to raise the base to.
 * @return The result of base raised to the power of exp.
 * @example pow(2, 3) returns 8.
 */
double pow(double base, double exp) {
    if (base == 0.0) {
        if (exp == 0.0) {
            return -1; // Undefined (0^0)
        } else {
            return 0.0;
        }
    }
    if (exp == 0.0) {
        return 1.0;
    }

    // General case using exp and ln functions
    return expf(exp * ln(base));
}

/**
 * root(n, r)
 * Calculate the r-th root of a number n.
 * @param n The number to calculate the root of.
 * @param r The root to calculate.
 * @return The r-th root of n.
 * @example root(27, 3) returns 3.
 */
double root(double n, double r) {
    if (r == 0) {
        return -1; // Undefined
    }

    return pow(n, 1.0 / r);
}

/**
 * abs(n)
 * Calculate the absolute value of a number n.
 * @param n The number to calculate the absolute value of.
 * @return The absolute value of n.
 * @example abs(-5) returns 5.
 */
double abs(double n) {
    return n < 0 ? -n : n;
}

/**
 * ceil(n)
 * Calculate the smallest integer greater than or equal to n.
 * @param n The number to calculate the ceiling of.
 * @return The smallest integer greater than or equal to n.
 * @example ceil(4.3) returns 5.
 */
double ceil(double n) {
    int in = (int)n;
    if (n > 0 && n > (double)in) {
        return (double)(in + 1);
    } else {
        return (double)in;
    }
}

/**
 * floor(n)
 * Calculate the largest integer less than or equal to n.
 * @param n The number to calculate the floor of.
 * @return The largest integer less than or equal to n.
 * @example floor(4.7) returns 4.
 */
double floor(double n) {
    int in = (int)n;
    if (n < 0 && n != (double)in) {
        return (double)(in - 1);
    }
    return (double)in;
}

/**
 * round(n)
 * Calculate the nearest integer to n.
 * @param n The number to round.
 * @return The nearest integer to n.
 * @example round(4.3) returns 4.
 */
double round(double n) {
    if (n >= 0) {
        return floor(n + 0.5);
    } else {
        return ceil(n - 0.5);
    }
}

/**
 * max(a, b)
 * Calculate the maximum of two numbers a and b.
 * @param a The first number.
 * @param b The second number.
 * @return The maximum of a and b.
 * @example max(3, 5) returns 5.
 */
double max(double a, double b) {
    return a > b ? a : b;
}

/**
 * min(a, b)
 * Calculate the minimum of two numbers a and b.
 * @param a The first number.
 * @param b The second number.
 * @return The minimum of a and b.
 * @example min(3, 5) returns 3.
 */
double min(double a, double b) {
    return a < b ? a : b;
}

/**
 * clamp(n, min, max)
 * Clamp a number n to be within the range [min, max].
 * @param n The number to clamp.
 * @param min_value The minimum value.
 * @param max_value The maximum value.
 * @return The clamped value of n within the range [min, max].
 * @example clamp(5, 0, 10) returns 5.
 */
double clamp(double n, double min_value, double max_value) {
    return max(min_value, min(n, max_value));
}

/**
 * lerp(a, b, t)
 * Linearly interpolate between two values a and b by a factor t.
 * @param a The start value.
 * @param b The end value.
 * @param t The interpolation factor.
 * @return The interpolated value between a and b.
 * @example lerp(0, 10, 0.5) returns 5.
 */
double lerp(double a, double b, double t) {
    return a + t * (b - a);
}

/**
 * map(n, start1, stop1, start2, stop2)
 * Map a value n from the range [start1, stop1] to the range [start2, stop2].
 * @param n The value to map.
 * @param start1 The start of the input range.
 * @param stop1 The end of the input range.
 * @param start2 The start of the output range.
 * @param stop2 The end of the output range.
 * @return The mapped value of n from the input range to the output range.
 * @example map(5, 0, 10, 0, 100) returns 50.
 */
double map(double n, double start1, double stop1, double start2, double stop2) {
    if (stop1 == start1) {
        return -1; // Undefined
    }
    double ratio = (n - start1) / (stop1 - start1);
    return start2 + ratio * (stop2 - start2);
}

/**
 * sign(n)
 * Get the sign of a number n.
 * @param n The number to get the sign of.
 * @return -1 if n is negative, 0 if n is zero, 1 if n is positive.
 * @example sign(-5) returns -1.
 */
int sign(double n) {
    if (n > 0) {
        return 1;
    } else if (n < 0) {
        return -1;
    } else {
        return 0;
    }
}

/**
 * sin(n)
 * Calculate the sine of an angle n in radians.
 * @param n The angle in radians.
 * @return The sine of the angle n.
 * @example sin(0) returns 0.
 */
double sin(double x) {
    double term = x;
    double sum = x;
    int n = 1;

    const double epsilon = 0.0000001;
    while (abs(term) > epsilon) {
        term = -term * x * x / ((2 * n) * (2 * n + 1));
        sum += term;
        n++;
    }
    return sum;
}

/**
 * cos(n)
 * Calculate the cosine of an angle n in radians.
 * @param n The angle in radians.
 * @return The cosine of the angle n.
 * @example cos(0) returns 1.
 */
double cos(double x) {
    double term = 1.0;
    double sum = 1.0;
    int n = 1;

    const double epsilon = 0.0000001;
    while (abs(term) > epsilon) {
        term = -term * x * x / ((2 * n - 1) * (2 * n));
        sum += term;
        n++;
    }
    return sum;
}

/**
 * tan(n)
 * Calculate the tangent of an angle n in radians.
 * @param n The angle in radians.
 * @return The tangent of the angle n.
 * @example tan(0) returns 0.
 */
double tan(double x) {
    return sin(x) / cos(x);
}

/**
 * asin(n)
 * Calculate the arcsine of a value n.
 * @param n The value.
 * @return The arcsine of n in radians.
 * @example asin(0) returns 0.
 */
double asin(double x) {
    if (x < -1.0 || x > 1.0) {
        return -1; // Undefined
    }

    double term = x;
    double sum = x;
    int n = 1;

    const double epsilon = 0.0000001;
    while (abs(term) > epsilon) {
        term *= x * x * (2 * n - 1) * (2 * n - 1) / (2 * n * (2 * n + 1));
        sum += term;
        n++;
        if (n > 100) break;
    }
    return sum;
}

/**
 * acos(n)
 * Calculate the arccosine of a value n.
 * @param n The value.
 * @return The arccosine of n in radians.
 * @example acos(1) returns 0.
 */
double acos(double x) {
    return (PI / 2) - asin(x);
}

/**
 * atan(n)
 * Calculate the arctangent of a value n.
 * @param n The value.
 * @return The arctangent of n in radians.
 * @example atan(0) returns 0.
 */
double atan(double x) {
    double term = x;
    double sum = x;
    int n = 1;

    const double epsilon = 0.0000001;
    while (abs(term) > epsilon) {
        term = -term * x * x * (2 * n - 1) / (2 * n + 1);
        sum += term;
        n++;
        if (n > 100) break;
    }
    return sum;
}

/**
 * atan2(y, x)
 * Calculate the arctangent of y/x in radians, using the signs of both arguments to determine the quadrant.
 * @param y The y-coordinate.
 * @param x The x-coordinate.
 * @return The arctangent of y/x in radians.
 * @example atan2(1, 1) returns π/4.
 */
double atan2(double y, double x) {
    if (x > 0) {
        return atan(y / x);
    } else if (x < 0 && y >= 0) {
        return atan(y / x) + PI;
    } else if (x < 0 && y < 0) {
        return atan(y / x) - PI;
    } else if (x == 0 && y > 0) {
        return PI / 2;
    } else if (x == 0 && y < 0) {
        return -PI / 2;
    } else {
        return 0; // Undefined
    }
}

/**
 * exp(n)
 * Calculate the exponential function of a value n.
 * @param n The value.
 * @return The exponential function of n.
 * @example exp(0) returns 1.
 */
double exp(double x) {
    double term = 1.0;
    double sum = 1.0;

    int n = 1;
    const double epsilon = 0.0000001;
    while (abs(term) > epsilon) {
        term *= x / n;
        sum += term;
        n++;
    }
    return sum;
}

/**
 * ln(n)
 * Calculate the natural logarithm of a value n.
 * @param n The value.
 * @return The natural logarithm of n.
 * @example ln(1) returns 0.
 */
double ln(double x) {
    if (x <= 0) {
        return -1; // Undefined
    }

    double y = x - 1.0;
    double prev_y = 0.0;
    const double epsilon = 0.0000001;
    while (abs(y - prev_y) > epsilon) {
        prev_y = y;
        y = y - (exp(y) - x) / exp(y);
    }
    return y;
}

/**
 * log10(n)
 * Calculate the base-10 logarithm of a value n.
 * @param n The value.
 * @return The base-10 logarithm of n.
 * @example log10(10) returns 1.
 */
double log10(double x) {
    return ln(x) / ln(10.0);
}

/**
 * log(base, n)
 * Calculate the logarithm of a value n with a specified base.
 * @param base The base of the logarithm.
 * @param n The value.
 * @return The logarithm of n with the specified base.
 * @example log(2, 8) returns 3.
 */
double log(double base, double x) {
    return ln(x) / ln(base);
}

/**
 * deg_to_rad(deg)
 * Convert an angle from degrees to radians.
 * @param deg The angle in degrees.
 * @return The angle in radians.
 * @example deg_to_rad(180) returns π.
 */
double deg_to_rad(double deg) {
    return deg * (PI / 180.0);
}

/**
 * rad_to_deg(rad)
 * Convert an angle from radians to degrees.
 * @param rad The angle in radians.
 * @return The angle in degrees.
 * @example rad_to_deg(π) returns 180.
 */
double rad_to_deg(double rad) {
    return rad * (180.0 / PI);
}

/**
 * hypot(x, y)
 * Calculate the hypotenuse of a right-angled triangle with sides x and y.
 * @param x The length of the first side.
 * @param y The length of the second side.
 * @return The length of the hypotenuse.
 * @example hypot(3, 4) returns 5.
 */
double hypot(double x, double y) {
    return sqrt(x * x + y * y);
}

/**
 * rand()
 * Generate pseudo-random numbers using a linear congruential generator.
 * @return A pseudo-random number between 0 and 1.
 */
double rand(void) {
    uint64_t static seed = 0;
    seed = read_cntpct() * 6364136223846793005ULL + 1;
    return (uint32_t)(seed >> 32) / (double)UINT32_MAX;
}

/**
 * expf(n)
 * Calculate the exponential function of a value n.
 * @param n The value.
 * @return The exponential function of n.
 * @example expf(0) returns 1.
 */
float expf(float x) {
    float term = 1.0f;
    float sum = 1.0f;

    int n = 1;
    const float epsilon = 0.0000001f;
    while (abs(term) > epsilon) {
        term *= x / n;
        sum += term;
        n++;
    }
    return sum;
}
