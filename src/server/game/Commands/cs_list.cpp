#include "Chat.h"
#include "Language.h"
#include "SpellAuraEffects.h"

bool ChatHandler::HandleListCreatureCommand(const char* args)
{
    ARGS_CHECK

    // number or [name] Shift-click form |color|Hcreature_entry:creature_id|h[name]|h|r
    char* cId = extractKeyFromLink((char*)args,"Hcreature_entry");
    if(!cId)
        return false;

    uint32 cr_id = atol(cId);
    if(!cr_id)
    {
        PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, cr_id);
        SetSentErrorMessage(true);
        return false;
    }

    CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(cr_id);
    if(!cInfo)
    {
        PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, cr_id);
        SetSentErrorMessage(true);
        return false;
    }

    char* c_count = strtok(nullptr, " ");
    int count = c_count ? atol(c_count) : 10;

    if(count < 0)
        return false;

    QueryResult result;

    uint32 cr_count = 0;
    result=WorldDatabase.PQuery("SELECT COUNT(guid) FROM creature WHERE id='%u'",cr_id);
    if(result)
    {
        cr_count = (*result)[0].GetUInt64();
    }

    if(m_session)
    {
        Player* pl = m_session->GetPlayer();
        result = WorldDatabase.PQuery("SELECT guid, position_x, position_y, position_z, map, (POW(position_x - '%f', 2) + POW(position_y - '%f', 2) + POW(position_z - '%f', 2)) AS order_ FROM creature WHERE id = '%u' ORDER BY order_ ASC LIMIT %u",
            pl->GetPositionX(), pl->GetPositionY(), pl->GetPositionZ(), cr_id,uint32(count));
    }
    else
        result = WorldDatabase.PQuery("SELECT guid, position_x, position_y, position_z, map FROM creature WHERE id = '%u' LIMIT %u",
            cr_id,uint32(count));

    if (result)
    {
        do
        {
            Field *fields = result->Fetch();
            ObjectGuid::LowType guid = fields[0].GetUInt32();
            float x = fields[1].GetFloat();
            float y = fields[2].GetFloat();
            float z = fields[3].GetFloat();
            int mapid = fields[4].GetUInt16();

            if  (m_session)
                PSendSysMessage(LANG_CREATURE_LIST_CHAT, guid, guid, cInfo->Name.c_str(), x, y, z, mapid);
            else
                PSendSysMessage(LANG_CREATURE_LIST_CONSOLE, guid, cInfo->Name.c_str(), x, y, z, mapid);
        } while (result->NextRow());
    }

    PSendSysMessage(LANG_COMMAND_LISTCREATUREMESSAGE,cr_id,cr_count);
    return true;
}

bool ChatHandler::HandleListItemCommand(const char* args)
{
    ARGS_CHECK

    char* cId = extractKeyFromLink((char*)args,"Hitem");
    if(!cId)
        return false;

    uint32 item_id = atol(cId);
    if(!item_id)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
        SetSentErrorMessage(true);
        return false;
    }

    ItemTemplate const* itemProto = sObjectMgr->GetItemTemplate(item_id);
    if(!itemProto)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
        SetSentErrorMessage(true);
        return false;
    }

    char* c_count = strtok(nullptr, " ");
    int count = c_count ? atol(c_count) : 10;

    if(count < 0)
        return false;

    QueryResult result;

    // inventory case
    uint32 inv_count = 0;
    result = CharacterDatabase.PQuery("SELECT COUNT(item_template) FROM character_inventory WHERE item_template='%u'", item_id);
    if(result)
    {
        inv_count = (*result)[0].GetUInt64();
    }

    result=CharacterDatabase.PQuery(
    //          0        1             2             3        4                  5
        "SELECT ci.item, cibag.slot AS bag, ci.slot, ci.guid, characters.account,characters.name "
        "FROM character_inventory AS ci LEFT JOIN character_inventory AS cibag ON (cibag.item=ci.bag),characters "
        "WHERE ci.item_template='%u' AND ci.guid = characters.guid LIMIT %u ",
        item_id, uint32(count));

    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            ObjectGuid::LowType item_guid = fields[0].GetUInt32();
            uint32 item_bag = fields[1].GetUInt32();
            uint32 item_slot = fields[2].GetUInt32();
            ObjectGuid::LowType owner_guid = fields[3].GetUInt32();
            uint32 owner_acc = fields[4].GetUInt32();
            std::string owner_name = fields[5].GetString();

            char const* item_pos = nullptr;
            if(Player::IsEquipmentPos(item_bag,item_slot))
                item_pos = "[equipped]";
            else if(Player::IsInventoryPos(item_bag,item_slot))
                item_pos = "[in inventory]";
            else if(Player::IsBankPos(item_bag,item_slot))
                item_pos = "[in bank]";
            else
                item_pos = "";

            PSendSysMessage(LANG_ITEMLIST_SLOT,
                item_guid,owner_name.c_str(),owner_guid,owner_acc,item_pos);
        } while (result->NextRow());

        int64 res_count = result->GetRowCount();

        if(count > res_count)
            count-=res_count;
        else if(count)
            count = 0;
    }

    // mail case
    uint32 mail_count = 0;

    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAIL_COUNT_ITEM);
    stmt->setUInt32(0, item_id);
    PreparedQueryResult result2 = CharacterDatabase.Query(stmt);

    if (result2)
        mail_count = (*result2)[0].GetUInt64();
    if (count > 0)
    {
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAIL_ITEMS_BY_ENTRY);
        stmt->setUInt32(0, item_id);
        stmt->setUInt32(1, count);
        result2 = CharacterDatabase.Query(stmt);
    }
    else
        result2 = PreparedQueryResult(nullptr);

    if (result2)
    {
        do
        {
            Field* fields = result2->Fetch();
            ObjectGuid::LowType itemGuid = fields[0].GetUInt32();
            ObjectGuid::LowType itemSender = fields[1].GetUInt32();
            uint32 itemReceiver = fields[2].GetUInt32();
            uint32 itemSenderAccountId = fields[3].GetUInt32();
            std::string itemSenderName = fields[4].GetString();
            uint32 itemReceiverAccount = fields[5].GetUInt32();
            std::string itemReceiverName = fields[6].GetString();

            char const* itemPos = "[in mail]";

            PSendSysMessage(LANG_ITEMLIST_MAIL, itemGuid, itemSenderName.c_str(), itemSender, itemSenderAccountId, itemReceiverName.c_str(), itemReceiver, itemReceiverAccount, itemPos);
        } while (result2->NextRow());

        uint32 resultCount = uint32(result2->GetRowCount());

        if (count > resultCount)
            count -= resultCount;
        else if (count)
            count = 0;
    }

    // auction case
    uint32 auc_count = 0;
    result=CharacterDatabase.PQuery("SELECT COUNT(item_template) FROM auctionhouse WHERE item_template='%u'",item_id);
    if(result)
    {
        auc_count = (*result)[0].GetUInt32();
    }

    if(count > 0)
    {
        result=CharacterDatabase.PQuery(
        //           0                      1                       2                   3
            "SELECT  auctionhouse.itemguid, auctionhouse.itemowner, characters.account, characters.name "
            "FROM auctionhouse,characters WHERE auctionhouse.item_template='%u' AND characters.guid = auctionhouse.itemowner LIMIT %u",
            item_id,uint32(count));
    }
    else
        result = nullptr;

    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            ObjectGuid::LowType item_guid       = fields[0].GetUInt32();
            uint32 owner           = fields[1].GetUInt32();
            uint32 owner_acc       = fields[2].GetUInt32();
            std::string owner_name = fields[3].GetString();

            char const* item_pos = "[in auction]";

            PSendSysMessage(LANG_ITEMLIST_AUCTION, item_guid, owner_name.c_str(), owner, owner_acc,item_pos);
        } while (result->NextRow());
    }

    // guild bank case
    uint32 guild_count = 0;
    result=CharacterDatabase.PQuery("SELECT COUNT(item_entry) FROM guild_bank_item WHERE item_entry='%u'",item_id);
    if(result)
    {
        guild_count = (*result)[0].GetUInt32();
    }

    result=CharacterDatabase.PQuery(
        //      0             1           2
        "SELECT gi.item_guid, gi.guildid, guild.name "
        "FROM guild_bank_item AS gi, guild WHERE gi.item_entry='%u' AND gi.guildid = guild.guildid LIMIT %u ",
        item_id,uint32(count));

    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            ObjectGuid::LowType item_guid = fields[0].GetUInt32();
            uint32 guild_guid = fields[1].GetUInt32();
            std::string guild_name = fields[2].GetString();

            char const* item_pos = "[in guild bank]";

            PSendSysMessage(LANG_ITEMLIST_GUILD,item_guid,guild_name.c_str(),guild_guid,item_pos);
        } while (result->NextRow());

        //int64 res_count = result->GetRowCount();
    }

    if(inv_count+mail_count+auc_count+guild_count == 0)
    {
        SendSysMessage(LANG_COMMAND_NOITEMFOUND);
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_COMMAND_LISTITEMMESSAGE,item_id,inv_count+mail_count+auc_count+guild_count,inv_count,mail_count,auc_count,guild_count);

    return true;
}

bool ChatHandler::HandleListObjectCommand(const char* args)
{
    ARGS_CHECK

    // number or [name] Shift-click form |color|Hgameobject_entry:go_id|h[name]|h|r
    char* cId = extractKeyFromLink((char*)args,"Hgameobject_entry");
    if(!cId)
        return false;

    uint32 go_id = atol(cId);
    if(!go_id)
    {
        PSendSysMessage(LANG_COMMAND_LISTOBJINVALIDID, go_id);
        SetSentErrorMessage(true);
        return false;
    }

    GameObjectTemplate const * gInfo = sObjectMgr->GetGameObjectTemplate(go_id);
    if(!gInfo)
    {
        PSendSysMessage(LANG_COMMAND_LISTOBJINVALIDID, go_id);
        SetSentErrorMessage(true);
        return false;
    }

    char* c_count = strtok(nullptr, " ");
    int count = c_count ? atol(c_count) : 10;

    if(count < 0)
        return false;

    QueryResult result;

    uint32 obj_count = 0;
    result=WorldDatabase.PQuery("SELECT COUNT(guid) FROM gameobject WHERE id='%u'",go_id);
    if(result)
    {
        obj_count = (*result)[0].GetUInt64();
    }

    if(m_session)
    {
        Player* pl = m_session->GetPlayer();
        result = WorldDatabase.PQuery("SELECT guid, position_x, position_y, position_z, map, id, (POW(position_x - '%f', 2) + POW(position_y - '%f', 2) + POW(position_z - '%f', 2)) AS order_ FROM gameobject WHERE id = '%u' ORDER BY order_ ASC LIMIT %u",
            pl->GetPositionX(), pl->GetPositionY(), pl->GetPositionZ(),go_id,uint32(count));
    }
    else
        result = WorldDatabase.PQuery("SELECT guid, position_x, position_y, position_z, map FROM gameobject WHERE id = '%u' LIMIT %u",
            go_id,uint32(count));

    if (result)
    {
        do
        {
            Field *fields = result->Fetch();
            ObjectGuid::LowType guid = fields[0].GetUInt32();
            float x = fields[1].GetFloat();
            float y = fields[2].GetFloat();
            float z = fields[3].GetFloat();
            int mapid = fields[4].GetUInt16();
            uint32 id = fields[5].GetUInt32();

            if (m_session)
                PSendSysMessage(LANG_GO_LIST_CHAT, guid, id, guid, gInfo->name.c_str(), x, y, z, mapid);
            else
                PSendSysMessage(LANG_GO_LIST_CONSOLE, guid, gInfo->name.c_str(), x, y, z, mapid);
        } while (result->NextRow());
    }

    PSendSysMessage(LANG_COMMAND_LISTOBJMESSAGE,go_id,obj_count);
    return true;
}

bool ChatHandler::HandleListAurasCommand (const char * /*args*/)
{
    Unit* unit = GetSelectedUnit();
    if (!unit)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    char const* talentStr = GetTrinityString(LANG_TALENT);
    char const* passiveStr = GetTrinityString(LANG_PASSIVE);

    Unit::AuraApplicationMap const& auras = unit->GetAppliedAuras();
    PSendSysMessage(LANG_COMMAND_TARGET_LISTAURAS, auras.size());
    for (Unit::AuraApplicationMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
    {
        bool talent = GetTalentSpellCost(itr->second->GetBase()->GetId()) > 0;

        AuraApplication const* aurApp = itr->second;
        Aura const* aura = aurApp->GetBase();
        char const* name = aura->GetSpellInfo()->SpellName[GetSessionDbcLocale()];

        std::ostringstream ss_name;
 //       ss_name << "|cffffffff|Hspell:" << aura->GetId() << "|h[" << name << "]|h|r"; //message does not appear, what's wrong? link format is the same on LK and BC
        ss_name << name;

        PSendSysMessage(LANG_COMMAND_TARGET_AURADETAIL, aura->GetId(), (GetSession() ? ss_name.str().c_str() : name),
            aurApp->GetEffectMask(), aura->GetCharges(), aura->GetStackAmount(), aurApp->GetSlot(),
            aura->GetDuration(), aura->GetMaxDuration(), (aura->IsPassive() ? passiveStr : ""),
            (talent ? talentStr : ""), aura->GetCasterGUID().IsPlayer() ? "player" : "creature",
            aura->GetCasterGUID().GetCounter());

        //not working... why?
        /*std::ostringstream test;
        test << "|cffffffff|Hspell:" << aura->GetId() << "|h[" << name << "]|h|r";
        SendSysMessage(test.str().c_str());*/
    }

    for (uint16 i = 0; i < TOTAL_AURAS; ++i)
    {
        Unit::AuraEffectList const& auraList = unit->GetAuraEffectsByType(AuraType(i));
        if (auraList.empty())
            continue;

        PSendSysMessage(LANG_COMMAND_TARGET_LISTAURATYPE, auraList.size(), i);

        for (Unit::AuraEffectList::const_iterator itr = auraList.begin(); itr != auraList.end(); ++itr)
            PSendSysMessage(LANG_COMMAND_TARGET_AURASIMPLE, (*itr)->GetId(), (*itr)->GetEffIndex(), (*itr)->GetAmount());
    }

    return true;
}
