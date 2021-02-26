#ifndef util_h
#define util_h

#include <stdlib.h>
#include <string.h>

#define ABORT(msg)               \
    do {                         \
        Serial.print("ABORT: "); \
        Serial.println(msg);     \
        while (1)                \
            ;                    \
    } while (0)

// #define max(a, b)               \
//     ({                          \
//         __typeof__(a) _a = (a); \
//         __typeof__(b) _b = (b); \
//         _a > _b ? _a : _b;      \
//     })
// #define min(a, b)               \
//     ({                          \
//         __typeof__(a) _a = (a); \
//         __typeof__(b) _b = (b); \
//         _a < _b ? _a : _b;      \
//     })

#define serial1(a)         \
    do {                   \
        Serial.println(a); \
    } while (0)
#define serial(a, b)       \
    do {                   \
        Serial.print(a);   \
        Serial.print(" "); \
        Serial.println(b); \
    } while (0)
#define serial2(a, b)      \
    do {                   \
        Serial.print(a);   \
        Serial.print(" "); \
        Serial.println(b); \
    } while (0)
#define serial3(a, b, c)   \
    do {                   \
        Serial.print(a);   \
        Serial.print(" "); \
        Serial.print(b);   \
        Serial.print(" "); \
        Serial.println(c); \
    } while (0)
#define serial4(a, b, c, d) \
    do {                    \
        Serial.print(a);    \
        Serial.print(" ");  \
        Serial.print(b);    \
        Serial.print(" ");  \
        Serial.print(c);    \
        Serial.print(" ");  \
        Serial.println(d);  \
    } while (0)
#define serial5(a, b, c, d, e) \
    do {                       \
        Serial.print(a);       \
        Serial.print(" ");     \
        Serial.print(b);       \
        Serial.print(" ");     \
        Serial.print(c);       \
        Serial.print(" ");     \
        Serial.print(d);       \
        Serial.print(" ");     \
        Serial.println(e);     \
    } while (0)
#define serial6(a, b, c, d, e, f) \
    do {                          \
        Serial.print(a);          \
        Serial.print(" ");        \
        Serial.print(b);          \
        Serial.print(" ");        \
        Serial.print(c);          \
        Serial.print(" ");        \
        Serial.print(d);          \
        Serial.print(" ");        \
        Serial.print(e);          \
        Serial.print(" ");        \
        Serial.println(f);        \
    } while (0)

#endif
