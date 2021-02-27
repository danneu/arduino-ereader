#pragma once
#ifndef util_h
#define util_h

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

void transpose8(unsigned char A[8], int32_t m, int32_t n, unsigned char B[8]);

#define ABORT(msg)                  \
    do {                            \
        Serial.print(F("ABORT: ")); \
        Serial.println(msg);        \
        while (1)                   \
            ;                       \
    } while (0)

#if SERIAL_ON
#define serial1(a)         \
    do {                   \
        Serial.println(a); \
    } while (0)
#else
#define serial1(a) (void)0
#endif

#if SERIAL_ON
#define serial(a, b)       \
    do {                   \
        Serial.print(a);   \
        Serial.print(" "); \
        Serial.println(b); \
    } while (0)
#else
#define serial(a, b) (void)0
#endif

#if SERIAL_ON
#define serial2(a, b)      \
    do {                   \
        Serial.print(a);   \
        Serial.print(" "); \
        Serial.println(b); \
    } while (0)
#else
#define serial2(a, b) (void)0
#endif

#if SERIAL_ON
#define serial3(a, b, c)   \
    do {                   \
        Serial.print(a);   \
        Serial.print(" "); \
        Serial.print(b);   \
        Serial.print(" "); \
        Serial.println(c); \
    } while (0)
#else
#define serial3(a, b, c) (void)0
#endif

#if SERIAL_ON
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
#else
#define serial4(a, b, c, d) (void)0
#endif

#if SERIAL_ON
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
#else
#define serial5(a, b, c, d, e) (void)0
#endif

#if SERIAL_ON
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
#else
#define serial6(a, b, c, d, e, f) (void)0
#endif

#ifndef max
#define max(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a > _b ? _a : _b;      \
    })
#endif

#ifndef min
#define min(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a < _b ? _a : _b;      \
    })
#endif

#endif
