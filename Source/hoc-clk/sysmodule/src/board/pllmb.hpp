#pragma once
#include <cstdint>
#include <switch.h>
#include <hocclk.h>
#include "board.hpp"  
#include <registers.h>
namespace pllmb {

    typedef enum PLLSource {
         EMC_SRC_PLLM      = 0,
         EMC_SRC_PLLC      = 1,
         EMC_SRC_PLLP      = 2,
         EMC_SRC_CLK_M     = 3,
         EMC_SRC_PLLM_UD   = 4,
         EMC_SRC_PLLMB_UD  = 5,
         EMC_SRC_PLLMB     = 6,
         EMC_SRC_PLLP_UD   = 7
    } PLLSource;

    u64 getRamClockRatePLLMB();
}