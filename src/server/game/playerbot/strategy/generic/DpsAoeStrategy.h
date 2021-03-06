#include "../generic/NonCombatStrategy.h"
#pragma once

namespace ai
{
    class DpsAoeStrategy : public NonCombatStrategy
    {
    public:
        DpsAoeStrategy(PlayerbotAI* ai) : NonCombatStrategy(ai) {}
        virtual std::string getName() { return "dps aoe"; }
        virtual int GetType() { return STRATEGY_TYPE_DPS; }

    public:
        virtual void InitTriggers(std::list<std::shared_ptr<TriggerNode>> &triggers);
    };


}
