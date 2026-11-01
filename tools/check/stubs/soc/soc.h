#pragma once
#include <cstdint>
#include <vector>
// DAC register writes land in g_dacReg; every value written is also logged
extern uint32_t g_dacReg; extern uint32_t g_dacWrites; extern std::vector<uint8_t> g_dacLog;
#define SET_PERI_REG_BITS(reg, bits, val, shift) \
  (g_dacWrites++, g_dacLog.push_back((uint8_t)(val)), \
   g_dacReg = (g_dacReg & ~((bits) << (shift))) | (((uint32_t)(val) & (bits)) << (shift)))
