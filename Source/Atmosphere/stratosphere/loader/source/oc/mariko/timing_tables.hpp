/*
 * Copyright (c) Lightos_
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

#pragma once
#include "../mtc_timing_value.hpp"

namespace ams::ldr::oc::pcv::mariko {
    void SetTableMaxClock(u32 maxClock);
    struct ReplacePatch {
        u32 freq;
        u32 correct;
    };

    extern const ReplacePatch  g_rext_table[];
    extern const u32           g_rext_table_size;
    const        ReplacePatch *FindRext();

    struct AdjustPatch {
        u32 freq;
        s32 adjust;
    };

    extern const AdjustPatch g_ibdly_patches[];
    extern const u32         g_ibdly_table_size;
    const AdjustPatch       *FindIbdlyPatch();

    extern const AdjustPatch g_obdly_patches[];
    extern const u32         g_obdly_table_size;
    const AdjustPatch       *FindObdlyPatch();

    extern const AdjustPatch g_tr2w_patches[];
    extern const u32         g_tr2w_table_size;
    const AdjustPatch       *FindTR2WPatch();

    extern const AdjustPatch g_qrst_patches[];
    extern const u32         q_qrst_table_size;
    const AdjustPatch       *FindQrstPatch();

    extern const AdjustPatch g_qsafe_patches[];
    extern const u32         g_qsafe_table_size;
    const AdjustPatch       *FindQsafePatch();

    extern const AdjustPatch g_pdex2rw_patches[];
    extern const u32         g_pdex2rw_table_size;
    const AdjustPatch       *FindPdex2rwPatch();

    extern const AdjustPatch g_cke2pden_patches[];
    extern const u32         g_cke2pden_table_size;
    const AdjustPatch       *FindCke2pdenPatch();

    struct MiscTimings {
        u32 min_freq;
        u32 rdv_inc;
        u32 einput;
        u32 quse_width;
    };

    extern const MiscTimings g_misc_table[];
    extern const u32         g_misc_table_size;

}
