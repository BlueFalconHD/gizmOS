#include "trig.h"

// Sine function using Taylor series expansion
double sin(double x) {
    double result = 0.0;
    double term = x;
    double sign = 1.0;
    double fact = 1.0;
    for (int i = 1; i < 10; i++) {
        result += sign * term;
        sign = -sign;
        fact *= (2 * i) * (2 * i + 1);
        term = term * x * x / fact;
    }
    return result;
}

// Cosine function using Taylor series expansion
double cos(double x) {
    double result = 0.0;
    double term = 1.0;
    double sign = 1.0;
    double fact = 1.0;
    for (int i = 1; i < 10; i++) {
        result += sign * term;
        sign = -sign;
        fact *= (2 * i) * (2 * i - 1);
        term = term * x * x / fact;
    }
    return result;
}


// Tangent function using Taylor series expansion
double tan(double x) {
    return sin(x) / cos(x);
}
