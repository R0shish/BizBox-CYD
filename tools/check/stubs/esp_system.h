#pragma once
#include <cstdint>
#include <cstdlib>
inline uint32_t esp_random() { return (uint32_t)rand() * 2654435761u; }
