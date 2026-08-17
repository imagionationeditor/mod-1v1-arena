#pragma once
#include "Define.h"

namespace TournamentCurrency
{
    constexpr uint32 GOLD_TO_COPPER = 10000;
    
    inline uint32 ToCopper(uint32 gold) 
    { 
        return gold * GOLD_TO_COPPER; 
    }
    
    inline uint32 ToGold(uint32 copper) 
    { 
        return copper / GOLD_TO_COPPER; 
    }
}
