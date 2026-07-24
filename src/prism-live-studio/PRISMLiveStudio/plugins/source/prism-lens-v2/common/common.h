#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

const unsigned int MAX_OUTPUT = 3;

#ifdef __APPLE__
const unsigned int CAPTURE_PERROW_ALIMENT_BITS = 64;
#elif _WIN32
const unsigned int CAPTURE_PERROW_ALIMENT_BITS = 0;
#endif

#ifdef _MSC_VER
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
