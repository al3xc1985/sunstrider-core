
#include "../../playerbot.h"
#include "LfgActions.h"
#include "../../AiFactory.h"
#include "../../PlayerbotAIConfig.h"
#include "../ItemVisitors.h"
#include "../../RandomPlayerbotMgr.h"
//#include "../../../DungeonFinding/LFGMgr.h"
//#include "../../../DungeonFinding/LFG.h"

using namespace ai;
#ifdef LICH_KING
using namespace lfg;
#endif

bool LfgJoinAction::Execute(Event event)
{
#ifdef LICH_KING
    if (!sPlayerbotAIConfig.randomBotJoinLfg)
        return false;

    if (bot->IsDead())
        return false;

    if (!sRandomPlayerbotMgr.IsRandomBot(bot))
        return false;

    if (sLFGMgr->GetState(bot->GetGUID()) != LFG_STATE_NONE)
        return false;

    if (bot->IsBeingTeleported())
        return false;

    Map* map = bot->GetMap();
    if (map && map->Instanceable())
        return false;

    return JoinProposal();
#else
    return false;
#endif
}

uint8 LfgJoinAction::GetRoles()
{
#ifdef LICH_KING
    int spec = AiFactory::GetPlayerSpecTab(bot);
    switch (bot->GetClass())
    {
    case CLASS_DRUID:
        if (spec == 2)
            return PLAYER_ROLE_HEALER;
        else if (spec == 1 && bot->GetLevel() >= 40)
            return PLAYER_ROLE_TANK;
        else
            return PLAYER_ROLE_DAMAGE;
        break;
    case CLASS_PALADIN:
        if (spec == 1)
            return PLAYER_ROLE_TANK;
        else if (spec == 0)
            return PLAYER_ROLE_HEALER;
        else
            return PLAYER_ROLE_DAMAGE;
        break;
    case CLASS_PRIEST:
        if (spec != 2)
            return PLAYER_ROLE_HEALER;
        else
            return PLAYER_ROLE_DAMAGE;
        break;
    case CLASS_SHAMAN:
        if (spec == 2)
            return PLAYER_ROLE_HEALER;
        else
            return PLAYER_ROLE_DAMAGE;
        break;
    case CLASS_WARRIOR:
        if (spec == 2)
            return PLAYER_ROLE_TANK;
        else
            return PLAYER_ROLE_DAMAGE;
        break;
    default:
        return PLAYER_ROLE_DAMAGE;
        break;
    }
    return PLAYER_ROLE_DAMAGE;
#endif
    return 0;
}

bool LfgJoinAction::SetRoles()
{
#ifdef LICH_KING
    sLFGMgr->SetRoles(bot->GetGUID(), GetRoles());
#endif
    return true;
}

bool LfgJoinAction::JoinProposal()
{
#ifdef LICH_KING
    ItemCountByQuality visitor;
    IterateItems(&visitor, ITERATE_ITEMS_IN_EQUIP);
    bool heroic = urand(0, 100) < 50 && (visitor.count[ITEM_QUALITY_EPIC] >= 3 || visitor.count[ITEM_QUALITY_RARE] >= 10) && bot->GetLevel() >= 70;
    bool random = urand(0, 100) < 25;
    bool raid = !heroic && (urand(0, 100) < 50 && visitor.count[ITEM_QUALITY_EPIC] >= 5 && (bot->GetLevel() == 60 || bot->GetLevel() == 70 || bot->GetLevel() == 80));

    LfgDungeonSet list;
    vector<uint32> idx;
    for (uint32 i = 0; i < sLFGDungeonStore.GetNumRows(); ++i)
    {
        LFGDungeonEntry const* dungeon = sLFGDungeonStore.LookupEntry(i);
        if (!dungeon || (dungeon->type != LFG_TYPE_RANDOM && dungeon->type != LFG_TYPE_DUNGEON && dungeon->type != LFG_TYPE_HEROIC &&
                dungeon->type != LFG_TYPE_RAID))
            continue;

        int botLevel = (int)bot->GetLevel();
        if (dungeon->minlevel && botLevel < (int)dungeon->minlevel)
            continue;

        if (dungeon->minlevel && botLevel > (int)dungeon->minlevel + 10)
            continue;

        if (dungeon->maxlevel && botLevel > (int)dungeon->maxlevel)
            continue;

        if (heroic && !dungeon->difficulty)
            continue;

        if (raid && dungeon->type != LFG_TYPE_RAID)
            continue;

        if (random && dungeon->type != LFG_TYPE_RANDOM)
            continue;

        if (!random && !raid && !heroic && dungeon->type != LFG_TYPE_DUNGEON)
            continue;

        if (!random)
            list.insert(dungeon->ID);
        else
            idx.push_back(dungeon->ID);
    }

    if (list.empty())
        return false;

    uint8 roles = GetRoles();
    if (random)
    {
        list.insert(idx[urand(0, idx.size() - 1)]);
        sLFGMgr->JoinLfg(bot, roles, list, "bot");

        sLog->outMessage("playerbot", LOG_LEVEL_DEBUG, "Bot %s joined to LFG_TYPE_RANDOM as %d", bot->GetName().c_str(), (uint32)roles);
        return true;
    }
    else if (heroic)
    {
        sLog->outMessage("playerbot", LOG_LEVEL_DEBUG, "Bot %s joined to LFG_TYPE_HEROIC_DUNGEON as %d", bot->GetName().c_str(), (uint32)roles);
    }
    else if (raid)
    {
        sLog->outMessage("playerbot", LOG_LEVEL_DEBUG, "Bot %s joined to LFG_TYPE_RAID as %d", bot->GetName().c_str(), (uint32)roles);
    }
    else
    {
        sLog->outMessage("playerbot", LOG_LEVEL_DEBUG, "Bot %s joined to LFG_TYPE_DUNGEON as %d", bot->GetName().c_str(), (uint32)roles);
    }

    sLFGMgr->JoinLfg(bot, roles, list, "bot");
#endif
    return true;
}

bool LfgRoleCheckAction::Execute(Event event)
{
#ifdef LICH_KING
    Group* group = bot->GetGroup();
    if (group)
    {
        sLFGMgr->UpdateRoleCheck(group->GetGUID(), bot->GetGUID(), GetRoles());
        return true;
    }
#endif
    return false;
}

bool LfgAcceptAction::Execute(Event event)
{
#ifdef LICH_KING

    uint32 id = AI_VALUE(uint32, "lfg proposal");
    if (id)
    {
        if (bot->IsInCombat() || bot->IsDead() || bot->IsFalling())
        {
            sLFGMgr->LeaveLfg(bot->GetGUID());
            return false;
        }

        ai->ChangeStrategy("-grind", BOT_STATE_NON_COMBAT);
        if (urand(0, 1 + 10 / sPlayerbotAIConfig.randomChangeMultiplier))
            return false;

        if (sRandomPlayerbotMgr.IsRandomBot(bot) && !bot->GetGroup())
            ai->ChangeStrategy("-grind", BOT_STATE_NON_COMBAT);

        sLog->outMessage("playerbot", LOG_LEVEL_DEBUG, "Bot %s updated proposal %d", bot->GetName().c_str(), id);
        ai->GetAiObjectContext()->GetValue<uint32>("lfg proposal")->Set(0);
        bot->ClearUnitState(UNIT_STATE_ALL_STATE_SUPPORTED);
        sLFGMgr->UpdateProposal(id, bot->GetGUID(), true);

        return true;
    }

    WorldPacket p(event.getPacket());

    uint32 dungeon;
    uint8 state;
    p >> dungeon >> state >> id;

    ai->GetAiObjectContext()->GetValue<uint32>("lfg proposal")->Set(id);
#endif
    return true;
}

bool LfgLeaveAction::Execute(Event event)
{
#ifdef LICH_KING

    if (sLFGMgr->GetState(bot->GetGUID()) != LFG_STATE_QUEUED)
        return false;

    sLFGMgr->LeaveLfg(bot->GetGUID());
#endif
    return true;
}

bool LfgTeleportAction::Execute(Event event)
{
#ifdef LICH_KING

    bool out = false;

    WorldPacket p(event.getPacket());
    if (!p.empty())
    {
        p.rpos(0);
        p >> out;
    }

    bot->ClearUnitState(UNIT_STATE_ALL_STATE_SUPPORTED);
    sLFGMgr->TeleportPlayer(bot, out);
#endif
    return true;
}
