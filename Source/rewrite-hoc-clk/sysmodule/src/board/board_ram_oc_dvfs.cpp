/*
 * Copyright (c) Souldbminer, Lightos_ and Horizon OC Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/* --------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <p-sam@d3vs.net>, <natinusala@gmail.com>, <m4x@m4xw.net>
 * wrote this file. As long as you retain this notice you can do whatever you
 * want with this stuff. If you meet any of us some day, and you think this
 * stuff is worth it, you can buy us a beer in return.  - The sys-clk authors
 * --------------------------------------------------------------------------
 */

#include <switch.h>
#include <memmem.h>
#include "../file_utils.h"
#include <cstring>
#include "board_ram_oc_dvfs.hpp"

namespace board {

    GpuVoltData voltData = {};

    Handle GetPcvHandle() {
        constexpr u64 PcvID = 0x10000000000001a;
        u64 processIDList[80]{};
        s32 processCount    = 0;
        Handle handle       = INVALID_HANDLE;

        DebugEventInfo debugEvent{};

        /* Get all running processes. */
        Result resultGetProcessList = svcGetProcessList(&processCount, processIDList, std::size(processIDList));
        if (R_FAILED(resultGetProcessList)) {
            return INVALID_HANDLE;
        }

        /* Try to find pcv. */
        for (int i = 0; i < processCount; ++i) {
            if (handle != INVALID_HANDLE) {
                svcCloseHandle(handle);
                handle = INVALID_HANDLE;
            }

            /* Try to debug process, if it fails, try next process. */
            Result resultSvcDebugProcess = svcDebugActiveProcess(&handle, processIDList[i]);
            if (R_FAILED(resultSvcDebugProcess)) {
                continue;
            }

            /* Try to get a debug event. */
            Result resultDebugEvent = svcGetDebugEvent(&debugEvent, handle);
            if (R_SUCCEEDED(resultDebugEvent)) {
                if (debugEvent.info.create_process.program_id == PcvID) {
                    return handle;
                }
            }
        }

        /* Failed to get handle. */
        return INVALID_HANDLE;
    }

    void CacheGpuVoltTable() {


        UnkRegulator reg = {
            .voltageMinUV = 600000,
            .voltageStep  = 12500,
            .voltageMax   = 1400000,
        };

        Handle handle = GetPcvHandle();
        if (handle == INVALID_HANDLE) {
            FileUtils::LogLine("[dvfs] Invalid handle!");
            return;
        }

        MemoryInfo memoryInfo = {};
        u64 address = 0;
        u32 pageInfo = 0;
        constexpr u32 PageSize = 0x1000;
        u8 buffer[PageSize];

        /* Loop until failure. */
        while (true) {
            /* Find pcv heap. */
            while (true) {
                Result resultProcessMemory = svcQueryDebugProcessMemory(&memoryInfo, &pageInfo, handle, address);
                address = memoryInfo.addr + memoryInfo.size;

                if (R_FAILED(resultProcessMemory) || !address) {
                    svcCloseHandle(handle);
                    FileUtils::LogLine("[dvfs] Failed to get process data. %u", R_DESCRIPTION(resultProcessMemory));
                    handle = INVALID_HANDLE;
                    return;
                }

                if (memoryInfo.size && (memoryInfo.perm & 3) == 3 && static_cast<char>(memoryInfo.type) == 0x4) {
                    /* Found valid memory. */
                    break;
                }
            }

            for (u64 base = 0; base < memoryInfo.size; base += PageSize) {
                u32 memorySize = std::min(memoryInfo.size, static_cast<u64>(PageSize));
                if (R_FAILED(svcReadDebugProcessMemory(buffer, handle, base + memoryInfo.addr, memorySize))) {
                    break;
                }

                u8 *resultFindReg = static_cast<u8 *>(memmem_impl(buffer, sizeof(buffer), &reg, sizeof(reg)));
                u32 index = resultFindReg - buffer;

                if (!resultFindReg) {
                    continue;
                }

                /* Assuming mariko. */
                const u32 vmax = 800;
                constexpr u32 VoltageTableOffset = 312;
                if (!std::memcmp(&buffer[index + VoltageTableOffset], &vmax, sizeof(vmax))) {
                    std::memcpy(voltData.voltTable, &buffer[index + VoltageTableOffset], sizeof(voltData.voltTable));
                    voltData.voltTableAddress = base + memoryInfo.addr + VoltageTableOffset + index;
                }

                svcCloseHandle(handle);
                handle = INVALID_HANDLE;
                return;
            }
        }

        svcCloseHandle(handle);
        handle = INVALID_HANDLE;
        return;
    }

    void PcvHijackGpuVolts(u32 vmin) {
        u32 table[192];
        static_assert(sizeof(table) == sizeof(voltData.voltTable));
        std::memcpy(table, voltData.voltTable, sizeof(voltData.voltTable));

        if (voltData.ramVmin == vmin) {
            return;
        }

        for (u32 i = 0; i < std::size(table); ++i) {
            if (table[i] && table[i] <= vmin) {
                table[i] = vmin;
            }
        }

        Handle handle = GetPcvHandle();
        if (handle == INVALID_HANDLE) {
            FileUtils::LogLine("Invalid handle!");
            return;
        }

        Result rc = svcWriteDebugProcessMemory(handle, table, voltData.dvfsAddress, sizeof(table));

        if (R_SUCCEEDED(rc)) {
            voltData.ramVmin = vmin;
        }

        svcCloseHandle(handle);
        FileUtils::LogLine("[dvfs] voltage set to %u mV", vmin);
    }

    u32 GetMinimumVmin(u32 freqMhz, u32 bracket) {
        static const u32 ramTable[][22] = {
            { 2133, 2200, 2266, 2300, 2366, 2400, 2433, 2466, 2533, 2566, 2600, 2633, 2700, 2733, 2766, 2833, 2866, 2900, 2933, 3033, 3066, 3100, },
            { 2300, 2366, 2433, 2466, 2533, 2566, 2633, 2700, 2733, 2800, 2833, 2900, 2933, 2966, 3033, 3066, 3100, 3133, 3166, 3200, 3233, 3266, },
            { 2433, 2466, 2533, 2600, 2666, 2733, 2766, 2800, 2833, 2866, 2933, 2966, 3033, 3066, 3100, 3133, 3166, 3200, 3233, 3300, 3333, 3366, },
            { 2500, 2533, 2600, 2633, 2666, 2733, 2800, 2866, 2900, 2966, 3033, 3100, 3166, 3200, 3233, 3266, 3300, 3333, 3366, 3400, 3400, 3400, },
        };

        static const u32 gpuVoltArray[] = { 590, 600, 610, 620, 630, 640, 650, 660, 670, 680, 690, 700, 710, 720, 730, 740, 750, 760, 770, 780, 790, 800, };

        if (freqMhz <= 1600) {
            return 0;
        }

        for (u32 i = 0; std::size(gpuDvfsArray) < 22; ++i) {
            if (freqMhz <= ramTable[bracket][i]) {
                return gpuVoltArray[i];
            }
        }

        return gpuVoltArray[std::size(gpuVoltArray) - 1];
    }

}
