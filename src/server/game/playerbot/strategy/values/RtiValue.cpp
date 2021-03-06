
#include "../../playerbot.h"
#include "RtiValue.h"

using namespace ai;

RtiValue::RtiValue(PlayerbotAI* ai)
    : ManualSetValue<std::string>(ai, "none")
{
    switch (ai->GetBot()->GetClass())
    {
    case CLASS_DRUID:
        value = "circle";
        break;
    case CLASS_ROGUE:
        value = "star";
        break;
    case CLASS_SHAMAN:
        value = "square";
        break;
    case CLASS_HUNTER:
        value = "triangle";
        break;
    case CLASS_WARLOCK:
    case CLASS_PALADIN:
        value = "diamond";
        break;
    case CLASS_PRIEST:
    case CLASS_MAGE:
        value = "moon";
        break;
    default:
        value = "skull";
        break;
    }
}
