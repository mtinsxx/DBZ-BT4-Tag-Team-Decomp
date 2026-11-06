#include <stdarg.h>
float native_scale(float x) { return x * 2.0f; }
double native_variadic(int ignored, ...) {
    va_list args;
    double a, b;
    (void)ignored;
    va_start(args, ignored);
    a = va_arg(args, double);
    b = va_arg(args, double);
    va_end(args);
    return a + b;
}
int native_format(int ignored, ...) {
    va_list args;
    double a, b;
    (void)ignored;
    va_start(args, ignored);
    a = va_arg(args, double); b = va_arg(args, double);
    va_end(args);
    return (int)(a + b);
}
