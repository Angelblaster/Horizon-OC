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
#include <sysclk.h>
#include "board.hpp"

namespace board {

    /*
    * Switch Power domains (max77620):
    * Name  | Usage         | uV step | uV min | uV default | uV max  | Init
    *-------+---------------+---------+--------+------------+---------+------------------
    *  sd0  | SoC           | 12500   | 600000 |  625000    | 1400000 | 1.125V (pkg1.1)
    *  sd1  | SDRAM         | 12500   | 600000 | 1125000    | 1125000 | 1.1V   (pkg1.1)
    *  sd2  | ldo{0-1, 7-8} | 12500   | 600000 | 1325000    | 1350000 | 1.325V (pcv)
    *  sd3  | 1.8V general  | 12500   | 600000 | 1800000    | 1800000 |
    *  ldo0 | Display Panel | 25000   | 800000 | 1200000    | 1200000 | 1.2V   (pkg1.1)
    *  ldo1 | XUSB, PCIE    | 25000   | 800000 | 1050000    | 1050000 | 1.05V  (pcv)
    *  ldo2 | SDMMC1        | 50000   | 800000 | 1800000    | 3300000 |
    *  ldo3 | GC ASIC       | 50000   | 800000 | 3100000    | 3100000 | 3.1V   (pcv)
    *  ldo4 | RTC           | 12500   | 800000 |  850000    |  850000 | 0.85V  (AO, pcv)
    *  ldo5 | GC Card       | 50000   | 800000 | 1800000    | 1800000 | 1.8V   (pcv)
    *  ldo6 | Touch, ALS    | 50000   | 800000 | 2900000    | 2900000 | 2.9V   (pcv)
    *  ldo7 | XUSB          | 50000   | 800000 | 1050000    | 1050000 | 1.05V  (pcv)
    *  ldo8 | XUSB, DP, MCU | 50000   | 800000 | 1050000    | 2800000 | 1.05V/2.8V (pcv)

    typedef enum {
        PcvPowerDomainId_Max77620_Sd0  = 0x3A000080,
        PcvPowerDomainId_Max77620_Sd1  = 0x3A000081, // vdd2
        PcvPowerDomainId_Max77620_Sd2  = 0x3A000082,
        PcvPowerDomainId_Max77620_Sd3  = 0x3A000083,
        PcvPowerDomainId_Max77620_Ldo0 = 0x3A0000A0,
        PcvPowerDomainId_Max77620_Ldo1 = 0x3A0000A1,
        PcvPowerDomainId_Max77620_Ldo2 = 0x3A0000A2,
        PcvPowerDomainId_Max77620_Ldo3 = 0x3A0000A3,
        PcvPowerDomainId_Max77620_Ldo4 = 0x3A0000A4,
        PcvPowerDomainId_Max77620_Ldo5 = 0x3A0000A5,
        PcvPowerDomainId_Max77620_Ldo6 = 0x3A0000A6,
        PcvPowerDomainId_Max77620_Ldo7 = 0x3A0000A7,
        PcvPowerDomainId_Max77620_Ldo8 = 0x3A0000A8,
        PcvPowerDomainId_Max77621_Cpu  = 0x3A000003,
        PcvPowerDomainId_Max77621_Gpu  = 0x3A000004,
        PcvPowerDomainId_Max77812_Cpu  = 0x3A000003,
        PcvPowerDomainId_Max77812_Gpu  = 0x3A000004,
        PcvPowerDomainId_Max77812_Dram = 0x3A000005, // vddq
    } PowerDomainId;
    */
    u32 GetVoltage(HocClkVoltage voltage) {
        RgltrSession session;
        Result rc = 0;
        u32 out = 0;
        BatteryChargeInfo info;

        switch (voltage) {
            case HocClkVoltage_SOC:
                rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77620_Sd0);
                ASSERT_RESULT_OK(rc, "rgltrOpenSession")
                rgltrGetVoltage(&session, &out);
                rgltrCloseSession(&session);
                break;
            case HocClkVoltage_EMCVDD2:
                rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77620_Sd1);
                ASSERT_RESULT_OK(rc, "rgltrOpenSession")
                rgltrGetVoltage(&session, &out);
                rgltrCloseSession(&session);
                break;
            case HocClkVoltage_CPU:
                if (GetSocType() == SysClkSocType_Mariko) {
                    rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77621_Cpu);
                } else {
                    rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77812_Cpu);
                }
                ASSERT_RESULT_OK(rc, "rgltrOpenSession")
                rgltrGetVoltage(&session, &out);
                rgltrCloseSession(&session);
                break;
            case HocClkVoltage_GPU:
                if (GetSocType() == SysClkSocType_Mariko) {
                    rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77621_Gpu);
                } else {
                    rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77812_Gpu);
                    ASSERT_RESULT_OK(rc, "rgltrOpenSession")
                    rgltrGetVoltage(&session, &out);
                    rgltrCloseSession(&session);
                }
                break;
            case HocClkVoltage_EMCVDDQ_MarikoOnly:
                if (GetSocType() == SysClkSocType_Mariko) {
                    rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77812_Dram);
                    ASSERT_RESULT_OK(rc, "rgltrOpenSession")
                    rgltrGetVoltage(&session, &out);
                    rgltrCloseSession(&session);
                } else {
                    out = GetVoltage(HocClkVoltage_EMCVDD2);
                }
                break;
            case HocClkVoltage_Display:
                rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77620_Ldo0);
                ASSERT_RESULT_OK(rc, "rgltrOpenSession")
                rgltrGetVoltage(&session, &out);
                rgltrCloseSession(&session);
                break;
            case HocClkVoltage_Battery:
                batteryInfoGetChargeInfo(&info);
                out = info.VoltageAvg;
                break;
            default:
                ASSERT_ENUM_VALID(HocClkVoltage, voltage);
        }

        return out > 0 ? out : 0;
    }

}
