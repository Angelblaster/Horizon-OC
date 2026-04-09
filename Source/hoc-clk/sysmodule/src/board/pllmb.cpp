#include "pllmb.hpp"

namespace pllmb {
    static const u8 qlin_hw_to_pdiv[17] = {
        1, 2, 3, 4, 5, 6, 8, 9, 10, 12, 15, 16, 18, 20, 24, 30, 32
    };

    enum pdiv_type {
        PDIV_QLIN, 
        PDIV_POW2 
    };

    struct pll_desc_t {
        u32       base_offset;
        u8        divm_shift;
        u8        divm_width;
        u8        divn_shift;
        u8        divn_width;
        u8        divp_shift;
        u8        divp_width;
        pdiv_type ptype;
    };

    static const pll_desc_t pll_table[] = {
        { PLLM_BASE,  0, 8,  8, 8, 20, 5, PDIV_QLIN },
        { PLLMB_BASE, 0, 8,  8, 8, 20, 5, PDIV_QLIN },
        { PLLP_BASE,  0, 8,  8, 8, 20, 5, PDIV_POW2 },
        { PLLA_BASE,  0, 8,  8, 8, 20, 5, PDIV_POW2 },
        { PLLU_BASE,  0, 8,  8, 8, 20, 5, PDIV_POW2 },
        { _PLLD_BASE,  0, 8,  8, 8, 20, 5, PDIV_POW2 },
        { PLLX_BASE,  0, 8,  8, 8, 20, 5, PDIV_QLIN },
        { PLLA1_BASE, 0, 8,  8, 8, 20, 5, PDIV_QLIN },
        { PLLDP_BASE, 0, 8,  8, 8, 20, 5, PDIV_QLIN },
        { PLLD2_BASE, 0, 8,  8, 8, 20, 5, PDIV_QLIN },
        { PLLC4_BASE, 0, 8,  8, 8, 20, 5, PDIV_QLIN },
        { PLLRE_BASE, 0, 8,  8, 8, 16, 4, PDIV_QLIN },
        { PLLC_BASE,  0, 8, 10, 8, 20, 5, PDIV_QLIN },
        { PLLC2_BASE, 0, 8, 10, 8, 20, 5, PDIV_QLIN },
        { PLLC3_BASE, 0, 8, 10, 8, 20, 5, PDIV_QLIN },
    };

    static inline u32 clk_read32(u32 offset)
    {
        return *(volatile u32 *)(uintptr_t)(board::clkVirtAddr + offset);
    }

    static inline u32 extract(u32 val, u8 shift, u8 width)
    {
        return (val >> shift) & ((1u << width) - 1u);
    }

    static u64 pll_rate_from_desc(const pll_desc_t &pll, u64 osc_hz,
                                  bool undivided)
    {
        u32 base = clk_read32(pll.base_offset);
        u32 divm = extract(base, pll.divm_shift, pll.divm_width);
        u32 divn = extract(base, pll.divn_shift, pll.divn_width);

        if (divm == 0 || divn == 0)
            return 0;

        u64 vco = osc_hz * divn / divm;

        if (undivided)
            return vco;

        u32 hw_p = extract(base, pll.divp_shift, pll.divp_width);
        u32 pdiv;

        if (pll.ptype == PDIV_QLIN)
            pdiv = (hw_p < 17) ? qlin_hw_to_pdiv[hw_p] : 1;
        else
            pdiv = 1u << hw_p;

        return vco / pdiv;
    }

    static u64 pll_rate_by_offset(u32 base_offset, u64 osc_hz,
                                  bool undivided)
    {
        for (const auto &pll : pll_table) {
            if (pll.base_offset == base_offset)
                return pll_rate_from_desc(pll, osc_hz, undivided);
        }
        return 0;
    }

    u64 getRamClockRatePLLMB()
    {
        u32 clk_src = clk_read32(CLK_SOURCE_EMC);
        u32 src     = (clk_src >> 29) & 0x7;
        u32 div     = (clk_src >>  0) & 0xff;

        u32  pll_off;
        bool undivided = false;

        switch (src) {
            case EMC_SRC_PLLM:
                pll_off = PLLM_BASE;
                break;
            case EMC_SRC_PLLM_UD:
                pll_off = PLLM_BASE;
                undivided = true;
                break;
            case EMC_SRC_PLLMB:
                pll_off = PLLMB_BASE;
                break;
            case EMC_SRC_PLLMB_UD:
                pll_off = PLLMB_BASE;
                undivided = true;
                break;
            case EMC_SRC_PLLP:
                pll_off = PLLP_BASE;
                break;
            case EMC_SRC_PLLP_UD:
                pll_off = PLLP_BASE;
                undivided = true;
                break;
            case EMC_SRC_PLLC:
                pll_off = PLLC_BASE;
                break;
            case EMC_SRC_CLK_M:
                return OSC_HZ;
            default:
                return 0;
        }

        u64 pll_hz = pll_rate_by_offset(pll_off, OSC_HZ, undivided);
        return pll_hz / (div + 2) * 2;
    }

}