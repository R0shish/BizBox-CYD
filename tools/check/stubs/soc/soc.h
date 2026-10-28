#pragma once
#include <cstdint>
extern uint32_t g_dacReg; extern uint32_t g_dacWrites;
#define SET_PERI_REG_BITS(reg, bits, val, shift) (g_dacWrites++, g_dacReg = (g_dacReg & ~((bits) << (shift))) | (((uint32_t)(val) & (bits)) << (shift)))
