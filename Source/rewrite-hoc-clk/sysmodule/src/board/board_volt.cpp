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

    u32 GetVoltage(HocClkVoltage voltage) {
        RgltrSession session;
        Result rc = 0;
        u32 out = 0;

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
