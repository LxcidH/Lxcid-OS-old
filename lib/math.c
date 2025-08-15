#include "math.h"

double floor(double x) {
    // If the number is already an integer, return it
    if (x == (long long)x) {
        return x;
    }

    // If the number is positive, truncate it (e.g., 3.7 -> 3.0)
    if (x >= 0.0) {
        return (double)(long long)x;
    }

    // If the number is negative, truncate and subtract 1 (e.g., -3.7 -> -4.0)
    return (double)((long long)x - 1);
}

double ceil(double x) {
    // If the number is already an integer, return it
    if (x == (long long)x) {
        return x;
    }

    // If the number is positive, truncate and add 1 (e.g., 3.7 -> 4.0)
    if (x >= 0.0) {
        return (double)((long long)x + 1);
    }

    // If the number is negative, truncate it (e.g., -3.7 -> -3.0)
    return (double)(long long)x;
}
