/*
 *
 * Copyright (c) 2025 Lightos_
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
 */

#include "mtc_timing_value.hpp"

namespace ams::ldr::oc::pcv::mariko {
    double tCK_avg = 1000'000.0 / C.marikoEmcMaxClock;
    u32 tRCD  = tRCD_values[C.t1_tRCD];
    u32 tRPpb = tRP_values[C.t2_tRP];
    u32 tRAS  = tRAS_values[C.t3_tRAS];
    double tRRD = tRRD_values[C.t4_tRRD];
    u32 tRFCpb = tRFC_values[C.t5_tRFC];
    u32 tWTR   = 10 - tWTR_values[C.t7_tWTR];
    u32 tRC = tRAS + tRPpb;
    u32 tRFCab = tRFCpb * 2;
    double tXSR = (double) (tRFCab + 7.5);
    u32 tFAW = static_cast<u32>(tRRD * 4.0);
    double tRPab = tRPpb + 3;
    u32 tR2P = 12 + (C.mem_burst_read_latency / 2);
    u32 tR2W = 0;
    u32 tRTM = RL + 9 + (tDQSCK_max / tCK_avg) + FLOOR(tRPST) + CEIL(10 / tCK_avg);
    u32 tRATM = tRTM + CEIL(10 / tCK_avg) - 12;
    u32 rdv = 0;
    u32 quse = FLOOR((-0.0048159 * (C.marikoEmcMaxClock / 1000.0)) + RL_DBI) + (FLOOR((C.marikoEmcMaxClock / 1000.0) * 0.0050997) * 1.5134);
    u32 einput = quse - ((C.marikoEmcMaxClock / 1000.0) * 0.01);
    u32 einput_duration = 0;
    u32 ibdly = 0;
    u32 obdly = 0;
    u32 quse_width = 0;
    u32 rext = 0;
    u32 qrst = 0;
    u32 qsafe = 0;
    u32 qpop = 0;
    u32 tW2P = (CEIL(WL * 1.7303) * 2) - 5;
    u32 tWTPDEN = 0;
    u32 tW2R = CEIL(MAX(WL + (0.010322547033278747 * (C.marikoEmcMaxClock / 1000.0)), (WL * -0.2067922202979121) + FLOOR(((RL_DBI * -0.1331159971685554) + WL) * 3.654131957826108)) - (tWTR / tCK_avg));
    u32 tWTM = WL + (BL / 2) + 1 + CEIL(7.5 / tCK_avg);
    u32 tWATM = tWTM + CEIL(tWR / tCK_avg);
    u32 wdv = WL;
    u32 wsv = WL - 2;
    u32 wev = 0xA + C.mem_burst_write_latency;
    u32 pdex2rw = 0;
    u32 cke2pden = 0;
    u32 tCKE = CEIL(1.0795 * CEIL(0.0074472 * (C.marikoEmcMaxClock / 1000.0)));
    double tMMRI = tRCD + (tCK_avg * 3);
    double pdex2mrr = tMMRI + 10;
}