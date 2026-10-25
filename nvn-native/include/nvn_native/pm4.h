/* Packet-7 encoding adapted from Mesa freedreno_pm4.h.
 * Copyright © 2012-2018 Rob Clark <robclark@freedesktop.org>
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <stdint.h>
static inline uint32_t nvn_pm4_odd_parity(uint32_t value)
{
    value ^= value >> 16;
    value ^= value >> 8;
    value ^= value >> 4;
    return ((~0x6996u) >> (value & 15)) & 1;
}
static inline uint32_t nvn_pm4_packet7(uint8_t opcode, uint16_t count)
{
    return 0x70000000u | count | (nvn_pm4_odd_parity(count) << 15) |
           ((opcode & 0x7fu) << 16) | (nvn_pm4_odd_parity(opcode) << 23);
}
static inline uint32_t nvn_pm4_packet4(uint32_t reg, uint32_t count)
{
    return 0x40000000u | count | (nvn_pm4_odd_parity(count) << 7) |
           (reg << 8) | (nvn_pm4_odd_parity(reg) << 27);
}
/* Walk packet boundaries, not arbitrary words that happen to resemble an
 * opcode. Images are generated type4/type7 streams with canonical parity. */
static inline uint32_t nvn_pm4_packet_words(uint32_t header)
{
    if ((header >> 28) == 4) {
        uint32_t count=header & 0x7fu, reg=(header >> 8) & 0x7ffffu;
        return count && header == nvn_pm4_packet4(reg,count) ? count+1 : 0;
    }
    if ((header >> 28) == 7) {
        uint32_t count=header & 0x3fffu, opcode=(header >> 16) & 0x7fu;
        return header == nvn_pm4_packet7(opcode,count) ? count+1 : 0;
    }
    return 0;
}
/* adreno_pm4.xml, A5XX and later: 64-bit address, followed by data. */
enum { NVN_CP_MEM_WRITE = 0x3d, NVN_CP_WAIT_FOR_IDLE = 0x26,
       NVN_CP_WAIT_FOR_ME = 0x13, NVN_CP_INDIRECT_BUFFER = 0x3f,
       NVN_CP_MEM_TO_MEM = 0x73 };
