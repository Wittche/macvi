#pragma once
#include <stdint.h>

// Dummy commpage data to satisfy compilation.
// A real macOS x86_64 commpage would contain CPU capability flags, timestamp data, etc.
static const uint8_t commpage_data[4096] = {0};
