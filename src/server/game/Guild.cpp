
#include "DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "MapManager.h"
#include "Player.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "Guild.h"
#include "Chat.h"
#include "SocialMgr.h"
#include "Util.h"
#include "Config.h"
#include "AccountMgr.h"
#include "CharacterCache.h"

Guild::Guild()
{
    Id = 0;
    name = "";
    GINFO = MOTD = "";
    EmblemStyle = 0;
    EmblemColor = 0;
    BorderStyle = 0;
    BorderColor = 0;
    BackgroundColor = 0;
    m_accountsNumber = 0;
    GuildEventlogMaxGuid = 0;

    CreatedYear = 0;
    CreatedMonth = 0;
    CreatedDay = 0;
}

Guild::~Guild()
{

}

bool Guild::create(ObjectGuid lGuid, std::string gname)
{
    std::string rname;
    std::string lName;

    if(!sCharacterCache->GetCharacterNameByGuid(lGuid, lName))
        return false;
    if(sObjectMgr->GetGuildByName(gname))
        return false;

    leaderGuid = lGuid;
    name = gname;
    GINFO = "";
    MOTD = "No message set.";
    guildbank_money = 0;
    purchased_tabs = 0;

    Id = sObjectMgr->GenerateGuildId();

    // gname already assigned to Guild::name, use it to encode string for DB
    CharacterDatabase.EscapeString(gname);

    std::string dbGINFO = GINFO;
    std::string dbMOTD = MOTD;
    CharacterDatabase.EscapeString(dbGINFO);
    CharacterDatabase.EscapeString(dbMOTD);

    SQLTransaction trans = CharacterDatabase.BeginTransaction();
    // CharacterDatabase.PExecute("DELETE FROM guild WHERE guildid='%u'", Id); - MAX(guildid)+1 not exist
    trans->PAppend("DELETE FROM guild_rank WHERE guildid='%u'", Id);
    trans->PAppend("DELETE FROM guild_member WHERE guildid='%u'", Id);
    trans->PAppend("INSERT INTO guild (guildid,name,leaderguid,info,motd,createdate,EmblemStyle,EmblemColor,BorderStyle,BorderColor,BackgroundColor,BankMoney) "
        "VALUES('%u','%s','%u', '%s', '%s', NOW(),'%u','%u','%u','%u','%u','" UI64FMTD "')",
        Id, gname.c_str(), leaderGuid.GetCounter(), dbGINFO.c_str(), dbMOTD.c_str(), EmblemStyle, EmblemColor, BorderStyle, BorderColor, BackgroundColor, guildbank_money);

    std::string leader;
    std::string officer;
    std::string veteran;
    std::string member;
    std::string initiate;
    switch (sWorld->GetDefaultDbcLocale())
    {
    case LOCALE_frFR:
        leader = "Maître";
        officer = "Officier";
        veteran = "Vétéran";
        member = "Membre";
        initiate = "Initié";
        break;
    default:
        leader = "Leader";
        officer = "Officer";
        veteran = "Veteran";
        member = "Member";
        initiate = "Initiate";
        break;
    }

    CreateRank(leader,GR_RIGHT_ALL, trans);
    CreateRank(officer,GR_RIGHT_ALL, trans);
    CreateRank(veteran,GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK, trans);
    CreateRank(member,GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK, trans);
    CreateRank(initiate,GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK, trans);

    if (!AddMember(lGuid, (uint32)GR_GUILDMASTER, trans)) {
        TC_LOG_ERROR("guild", "Player %u not added to guild %u!", lGuid.GetCounter(), Id);
        return false;
    }
    
    CharacterDatabase.CommitTransaction(trans);
    
    return true;
}

bool Guild::AddMember(ObjectGuid plGuid, uint32 plRank)
{
    SQLTransaction trans = CharacterDatabase.BeginTransaction();
    bool returnValue = AddMember(plGuid, plRank, trans);
    if(returnValue) //do not commit empty trans
        CharacterDatabase.CommitTransaction(trans);

    return returnValue;
}

bool Guild::AddMember(ObjectGuid plGuid, uint32 plRank, SQLTransaction trans)
{
    Player* pl = ObjectAccessor::FindPlayer(plGuid);
    if(pl)
    {
        if(pl->GetGuildId() != 0)
            return false;
    }
    else
    {
        if(Player::GetGuildIdFromCharacterInfo(plGuid) != 0)           // player already in guild
            return false;
    }

    // remove all player signs from another petitions
    // this will be prevent attempt joining player to many guilds and corrupt guild data integrity
    Player::RemovePetitionsAndSigns(plGuid, 9, trans);

    // fill player data
    MemberSlot newmember;

    if(!FillPlayerData(plGuid, &newmember))                 // problems with player data collection
        return false;

    newmember.RankId = plRank;
    newmember.OFFnote = (std::string)"";
    newmember.Pnote = (std::string)"";
    newmember.logout_time = time(nullptr);
    newmember.BankResetTimeMoney = 0;                       // this will force update at first query
    for (uint32 & i : newmember.BankResetTimeTab)
        i = 0;
    members[plGuid.GetCounter()] = newmember;

    std::string dbPnote = newmember.Pnote;
    std::string dbOFFnote = newmember.OFFnote;
    CharacterDatabase.EscapeString(dbPnote);
    CharacterDatabase.EscapeString(dbOFFnote);

    trans->PAppend("INSERT INTO guild_member (guildid,guid,`rank`,pnote,offnote) VALUES ('%u', '%u', '%u','%s','%s')",
        Id, plGuid.GetCounter(), newmember.RankId, dbPnote.c_str(), dbOFFnote.c_str());

    // If player not in game data in data field will be loaded from guild tables, no need to update it!!
    if(pl)
    {
        pl->SetInGuild(Id);
        pl->SetRank(newmember.RankId);
        pl->SetGuildIdInvited(0);
    }
    
    UpdateAccountsNumber();
    
    return true;
}

void Guild::SetMOTD(std::string motd)
{
    MOTD = motd;

    // motd now can be used for encoding to DB
    CharacterDatabase.EscapeString(motd);
    CharacterDatabase.PExecute("UPDATE guild SET motd='%s' WHERE guildid='%u'", motd.c_str(), Id);
}

void Guild::SetGINFO(std::string ginfo)
{
    GINFO = ginfo;

    // ginfo now can be used for encoding to DB
    CharacterDatabase.EscapeString(ginfo);
    CharacterDatabase.PExecute("UPDATE guild SET info='%s' WHERE guildid='%u'", ginfo.c_str(), Id);
}

bool Guild::LoadGuildFromDB(const std::string guildname)
{
    std::string escapedname = guildname;
    CharacterDatabase.EscapeString(escapedname);

    QueryResult result = CharacterDatabase.PQuery("SELECT guildid FROM guild WHERE name='%s'", escapedname.c_str());
    if (!result)
        return false;

    Field *fields = result->Fetch();

    uint32 guildId = fields[0].GetUInt32();

    return LoadGuildFromDB(guildId);
}

bool Guild::LoadGuildFromDB(uint32 GuildId)
{
    if(!LoadRanksFromDB(GuildId))
        return false;

    if(!LoadMembersFromDB(GuildId))
        return false;

    QueryResult result = CharacterDatabase.PQuery("SELECT MAX(TabId) FROM guild_bank_tab WHERE guildid='%u'", GuildId);
    if(result)
    {
        Field *fields = result->Fetch();
        purchased_tabs = fields[0].GetUInt8()+1;            // Because TabId begins at 0
    }
    else
        purchased_tabs = 0;

    LoadBankRightsFromDB(GuildId);                          // Must be after LoadRanksFromDB because it populates rank struct

    //                                        0        1     2           3            4            5           6
    result = CharacterDatabase.PQuery("SELECT guildid, name, leaderguid, EmblemStyle, EmblemColor, BorderStyle, BorderColor,"
    //   7                8     9     10          11
        "BackgroundColor, info, motd, createdate, BankMoney FROM guild WHERE guildid = '%u'", GuildId);

    if(!result)
        return false;

    Field *fields = result->Fetch();

    Id = fields[0].GetUInt32();
    name = fields[1].GetString();
    leaderGuid  = ObjectGuid(HighGuid::Player, fields[2].GetUInt32());

    EmblemStyle = fields[3].GetUInt32();
    EmblemColor = fields[4].GetUInt32();
    BorderStyle = fields[5].GetUInt32();
    BorderColor = fields[6].GetUInt32();
    BackgroundColor = fields[7].GetUInt32();
    GINFO = fields[8].GetString();
    MOTD = fields[9].GetString();
    std::string time = fields[10].GetString();                   //datetime is uint64 type ... YYYYmmdd:hh:mm:ss
    guildbank_money = fields[11].GetUInt64();

    /*uint64 dTime = time /1000000;
    CreatedDay   = dTime%100;
    CreatedMonth = (dTime/100)%100;
    CreatedYear  = (dTime/10000)%10000;*/
    
    CreatedYear = atoi(time.substr(0, 4).c_str());
    CreatedMonth = atoi(time.substr(5, 2).c_str());
    CreatedDay = atoi(time.substr(8, 2).c_str());

    // If the leader does not exist attempt to promote another member
    if(!sCharacterCache->GetCharacterAccountIdByGuid(leaderGuid ))
    {
        DeleteMember(leaderGuid);

        // check no members case (disbanded)
        if(members.empty())
            return false;
    }

    m_bankloaded = false;
    m_eventlogloaded = false;
    m_onlinemembers = 0;
    RenumBankLogs();
    RenumGuildEventlog();
    return true;
}

bool Guild::LoadRanksFromDB(uint32 GuildId)
{
    Field *fields;
    QueryResult result = CharacterDatabase.PQuery("SELECT rname,rights,BankMoneyPerDay,rid FROM guild_rank WHERE guildid = '%u' ORDER BY rid ASC", GuildId);

    if(!result)
        return false;

    bool broken_ranks = false;

    do
    {
        fields = result->Fetch();

        std::string rankName = fields[0].GetString();
        uint32 rankRights    = fields[1].GetUInt32();
        uint32 rankMoney     = fields[2].GetUInt32();
        uint32 rankRID       = fields[3].GetUInt32();

        if(rankRID != m_ranks.size()+1)                     // guild_rank.rid always store rank+1
            broken_ranks =  true;

        if(m_ranks.size()==GR_GUILDMASTER)                  // prevent loss leader rights
            rankRights |= GR_RIGHT_ALL;

        AddRank(rankName,rankRights,rankMoney);
    }while( result->NextRow() );

    if(m_ranks.size()==0)                                   // empty rank table?
    {
        AddRank("Guild Master",GR_RIGHT_ALL,0);
        broken_ranks = true;
    }

    // guild_rank have wrong numbered ranks, repair
    if(broken_ranks)
    {
        TC_LOG_ERROR("FIXME","Guild %u have broken `guild_rank` data, repairing...",GuildId);
        SQLTransaction trans = CharacterDatabase.BeginTransaction();
        trans->PAppend("DELETE FROM guild_rank WHERE guildid='%u'", GuildId);
        for(size_t i =0; i < m_ranks.size(); ++i)
        {
            // guild_rank.rid always store rank+1
            std::string thename = m_ranks[i].name;
            uint32 rights = m_ranks[i].rights;
            CharacterDatabase.EscapeString(thename);
            trans->PAppend( "INSERT INTO guild_rank (guildid,rid,rname,rights) VALUES ('%u', '%u', '%s', '%u')", GuildId, i+1, thename.c_str(), rights);
        }
        CharacterDatabase.CommitTransaction(trans);
    }

    return true;
}

bool Guild::LoadMembersFromDB(uint32 GuildId)
{
    //                                                    0                  1       2      3        4                  5
    QueryResult result = CharacterDatabase.PQuery("SELECT guild_member.guid, `rank`, pnote, offnote, BankResetTimeMoney,BankRemMoney,"
    //   6                  7                 8                  9                 10                 11
        "BankResetTimeTab0, BankRemSlotsTab0, BankResetTimeTab1, BankRemSlotsTab1, BankResetTimeTab2, BankRemSlotsTab2,"
    //   12                 13                14                 15                16                 17
        "BankResetTimeTab3, BankRemSlotsTab3, BankResetTimeTab4, BankRemSlotsTab4, BankResetTimeTab5, BankRemSlotsTab5,"
    //   18
        "logout_time FROM guild_member LEFT JOIN characters ON characters.guid = guild_member.guid WHERE guildid = '%u'", GuildId);

    if(!result)
        return false;

    do
    {
        Field *fields = result->Fetch();
        MemberSlot newmember;
        newmember.RankId = fields[1].GetUInt8();
        ObjectGuid guid = ObjectGuid(HighGuid::Player, fields[0].GetUInt32());

        // Player does not exist
        if(!FillPlayerData(guid, &newmember))
            continue;

        newmember.Pnote                 = fields[2].GetString();
        newmember.OFFnote               = fields[3].GetString();
        newmember.BankResetTimeMoney    = fields[4].GetUInt32();
        newmember.BankRemMoney          = fields[5].GetUInt32();
        for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
        {
            newmember.BankResetTimeTab[i] = fields[6+(2*i)].GetUInt32();
            newmember.BankRemSlotsTab[i]  = fields[7+(2*i)].GetUInt32();
        }
        newmember.logout_time           = fields[18].GetUInt64();
        sCharacterCache->UpdateCharacterGuildId(guid.GetCounter(), GetId());
        members[guid.GetCounter()]      = newmember;

    }while( result->NextRow() );

    if(members.empty())
        return false;
        
    UpdateAccountsNumber();

    return true;
}

bool Guild::FillPlayerData(ObjectGuid guid, MemberSlot* memslot)
{
    std::string plName;
    uint32 plLevel;
    uint32 plClass;
    uint32 plZone;

    Player* pl = ObjectAccessor::FindPlayer(guid);
    if(pl)
    {
        plName  =   pl->GetName();
        plLevel =   pl->GetLevel();
        plClass =   pl->GetClass();
        plZone  =   pl->GetZoneId();
    }
    else
    {
        QueryResult result = CharacterDatabase.PQuery("SELECT name, level, zone, class FROM characters WHERE guid = '%u'", guid.GetCounter());
        if(!result)
            return false;                                   // player doesn't exist

        Field *fields = result->Fetch();

        plName =  fields[0].GetString();
        plLevel = fields[1].GetUInt8();
        plZone =  fields[2].GetUInt32();
        plClass = fields[3].GetUInt8();

        if(plLevel<1||plLevel>STRONG_MAX_LEVEL)             // can be at broken `data` field
        {
            TC_LOG_ERROR("sql.sql","Player (GUID: %u) has a broken data in field `characters`.`data`.",guid.GetCounter());
            return false;
        }

        if(!plZone)
        {
            TC_LOG_ERROR("sql.sql","Player (GUID: %u) has broken zone-data",guid.GetCounter());
            //here it will also try the same, to get the zone from characters-table, but additional it tries to find
            plZone = Player::GetZoneIdFromDB(guid);
            //the zone through xy coords.. this is a bit redundant, but
            //shouldn't be called often
        }

        if(plClass<CLASS_WARRIOR||plClass>=MAX_CLASSES)     // can be at broken `class` field
        {
            TC_LOG_ERROR("sql.sql","Player (GUID: %u) has a broken data in field `characters`.`class`.",guid.GetCounter());
            return false;
        }
    }

    memslot->name = plName;
    memslot->level = plLevel;
    memslot->Class = plClass;
    memslot->zoneId = plZone;

    return(true);
}

void Guild::LoadPlayerStatsByGuid(ObjectGuid guid)
{
    auto itr = members.find(guid.GetCounter());
    if (itr == members.end() )
        return;

    Player *pl = ObjectAccessor::FindPlayer(guid);
    if(!pl)
        return;
    itr->second.name  = pl->GetName();
    itr->second.level = pl->GetLevel();
    itr->second.Class = pl->GetClass();
}

void Guild::SetLeader(ObjectGuid guid)
{
    leaderGuid = guid;
    ChangeRank(guid, GR_GUILDMASTER);

    CharacterDatabase.PExecute("UPDATE guild SET leaderguid='%u' WHERE guildid='%u'", guid.GetCounter(), Id);
}

void Guild::DeleteMember(ObjectGuid guid, bool isDisbanding)
{
    //enforce CONFIG_GM_FORCE_GUILD
    uint32 GMForceGuildId = sWorld->getIntConfig(CONFIG_GM_FORCE_GUILD);
    if(GetId() == GMForceGuildId)
    {
        uint32 accountId = sCharacterCache->GetCharacterAccountIdByGuid(guid);
        uint32 memberSecurity = sAccountMgr->GetSecurity(accountId);
        if (memberSecurity > SEC_PLAYER)
            return;
    }

    if(leaderGuid == guid && !isDisbanding)
    {
        MemberSlot* oldLeader = nullptr;
        MemberSlot* best = nullptr;
        ObjectGuid newLeaderGUID;
        for(auto & member : members)
        {
            if(member.first == guid.GetCounter())
            {
                oldLeader = &(member.second);
                continue;
            }

            if(!best || best->RankId > member.second.RankId)
            {
                best = &(member.second);
                newLeaderGUID = ObjectGuid(HighGuid::Player, member.first);
            }
        }
        if(!best)
        {
            Disband();
            return;
        }

        SetLeader(newLeaderGUID);

        // If player not online data in data field will be loaded from guild tabs no need to update it !!
        if(Player *newLeader = ObjectAccessor::FindPlayer(newLeaderGUID))
            newLeader->SetRank(GR_GUILDMASTER);

        // when leader non-exist (at guild load with deleted leader only) not send broadcasts
        if(oldLeader)
        {
            WorldPacket data(SMSG_GUILD_EVENT, (1+1+(oldLeader->name).size()+1+(best->name).size()+1));
            data << (uint8)GE_LEADER_CHANGED;
            data << (uint8)2;
            data << oldLeader->name;
            data << best->name;
            BroadcastPacket(&data);

            data.Initialize(SMSG_GUILD_EVENT, (1+1+(oldLeader->name).size()+1));
            data << (uint8)GE_LEFT;
            data << (uint8)1;
            data << oldLeader->name;
            BroadcastPacket(&data);
        }
    }

    members.erase(guid.GetCounter());

    Player *player = ObjectAccessor::FindPlayer(guid);
    // If player not online data in data field will be loaded from guild tabs no need to update it !!
    if(player)
    {
        player->SetInGuild(0);
        player->SetRank(0);
    } else
        sCharacterCache->UpdateCharacterGuildId(guid, 0);

    CharacterDatabase.PExecute("DELETE FROM guild_member WHERE guid = '%u'", guid.GetCounter());
}

void Guild::ChangeRank(ObjectGuid guid, uint32 newRank)
{
    auto itr = members.find(guid.GetCounter());
    if( itr != members.end() )
        itr->second.RankId = newRank;

    Player *player = ObjectAccessor::FindPlayer(guid);
    // If player not online data in data field will be loaded from guild tabs no need to update it !!
    if(player)
        player->SetRank(newRank);

    CharacterDatabase.PExecute( "UPDATE guild_member SET `rank`='%u' WHERE guid='%u'", newRank, guid.GetCounter() );
}

void Guild::SetPNOTE(ObjectGuid guid,std::string pnote)
{
    auto itr = members.find(guid.GetCounter());
    if( itr == members.end() )
        return;

    itr->second.Pnote = pnote;

    // pnote now can be used for encoding to DB
    CharacterDatabase.EscapeString(pnote);
    CharacterDatabase.PExecute("UPDATE guild_member SET pnote = '%s' WHERE guid = '%u'", pnote.c_str(), itr->first);
}

void Guild::SetOFFNOTE(ObjectGuid guid,std::string offnote)
{
    auto itr = members.find(guid.GetCounter());
    if( itr == members.end() )
        return;
    itr->second.OFFnote = offnote;
    // offnote now can be used for encoding to DB
    CharacterDatabase.EscapeString(offnote);
    CharacterDatabase.PExecute("UPDATE guild_member SET offnote = '%s' WHERE guid = '%u'", offnote.c_str(), itr->first);
}

void Guild::BroadcastToGuild(WorldSession *session, const std::string& msg, Language language)
{
    if (session && session->GetPlayer() && HasRankRight(session->GetPlayer()->GetRank(),GR_RIGHT_GCHATSPEAK))
    {
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_GUILD, language, session->GetPlayer(), nullptr, msg);

        for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
        {
            Player *pl = ObjectAccessor::FindPlayer(ObjectGuid(HighGuid::Player, itr->first));

            if (pl && pl->GetSession() && HasRankRight(pl->GetRank(),GR_RIGHT_GCHATLISTEN) && !pl->GetSocial()->HasIgnore(session->GetPlayer()->GetGUID().GetCounter()) )
                pl->SendDirectMessage(&data);
        }
    }
}

std::string Guild::GetOnlineMembersName()
{
    std::string ret = "";
    bool online = false;
    
    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr) {
        if (Player *pl = ObjectAccessor::FindPlayer(ObjectGuid(HighGuid::Player, itr->first))) {
            online = true;
            ret.append(pl->GetName());
            ret.append(" ");
        }
    }
    
    if (online)
        return ret;
        
    return "Aucun";
}

void Guild::BroadcastToOfficers(WorldSession *session, const std::string& msg, Language language)
{
    if (session && session->GetPlayer() && HasRankRight(session->GetPlayer()->GetRank(),GR_RIGHT_OFFCHATSPEAK))
    {
        for(auto itr = members.begin(); itr != members.end(); ++itr)
        {
            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_OFFICER, language, session->GetPlayer(), nullptr, msg);

            Player *pl = ObjectAccessor::FindPlayer(ObjectGuid(HighGuid::Player, itr->first));

            if (pl && pl->GetSession() && HasRankRight(pl->GetRank(),GR_RIGHT_OFFCHATLISTEN) && !pl->GetSocial()->HasIgnore(session->GetPlayer()->GetGUID().GetCounter()))
                pl->SendDirectMessage(&data);
        }
    }
}

void Guild::BroadcastPacket(WorldPacket *packet)
{
    for(auto itr = members.begin(); itr != members.end(); ++itr)
    {
        Player *player = ObjectAccessor::FindPlayer(ObjectGuid(HighGuid::Player, itr->first));
        if(player)
            player->SendDirectMessage(packet);
    }
}

void Guild::BroadcastPacketToRank(WorldPacket *packet, uint32 rankId)
{
    for(auto itr = members.begin(); itr != members.end(); ++itr)
    {
        if (itr->second.RankId == rankId)
        {
            Player *player = ObjectAccessor::FindPlayer(ObjectGuid(HighGuid::Player, itr->first));
            if(player)
                player->SendDirectMessage(packet);
        }
    }
}

void Guild::CreateRank(std::string name_,uint32 rights, SQLTransaction trans)
{
    if(m_ranks.size() >= GUILD_MAX_RANKS_COUNT)
        return;

    AddRank(name_,rights,0);

    for (int i = 0; i < purchased_tabs; ++i)
    {
        CreateBankRightForTab(m_ranks.size()-1, uint8(i), trans);
    }

    // guild_rank.rid always store rank+1 value

    // name now can be used for encoding to DB
    CharacterDatabase.EscapeString(name_);
    trans->PAppend( "INSERT INTO guild_rank (guildid,rid,rname,rights) VALUES ('%u', '%u', '%s', '%u')", Id, m_ranks.size(), name_.c_str(), rights );
}

void Guild::AddRank(const std::string& name_,uint32 rights, uint32 money)
{
    m_ranks.push_back(RankInfo(name_,rights,money));
}

void Guild::DelRank()
{
    if(m_ranks.empty())
        return;

    // guild_rank.rid always store rank+1 value
    uint32 rank = m_ranks.size()-1;
    CharacterDatabase.PExecute("DELETE FROM guild_rank WHERE rid>='%u' AND guildid='%u'", (rank+1), Id);

    m_ranks.pop_back();
}

std::string Guild::GetRankName(uint32 rankId)
{
    if(rankId >= m_ranks.size())
        return "<unknown>";

    return m_ranks[rankId].name;
}

uint32 Guild::GetRankRights(uint32 rankId)
{
    if(rankId >= m_ranks.size())
        return 0;

    return m_ranks[rankId].rights;
}

void Guild::SetRankName(uint32 rankId, std::string name_)
{
    if(rankId >= m_ranks.size())
        return;

    m_ranks[rankId].name = name_;

    // name now can be used for encoding to DB
    CharacterDatabase.EscapeString(name_);
    CharacterDatabase.PExecute("UPDATE guild_rank SET rname='%s' WHERE rid='%u' AND guildid='%u'", name_.c_str(), (rankId+1), Id);
}

void Guild::SetRankRights(uint32 rankId, uint32 rights)
{
    if(rankId >= m_ranks.size())
        return;

    m_ranks[rankId].rights = rights;

    CharacterDatabase.PExecute("UPDATE guild_rank SET rights='%u' WHERE rid='%u' AND guildid='%u'", rights, (rankId+1), Id);
}

int32 Guild::GetRank(ObjectGuid::LowType LowGuid)
{
    auto itr = members.find(LowGuid);
    if (itr==members.end())
        return -1;

    return itr->second.RankId;
}

void Guild::Disband()
{
    WorldPacket data(SMSG_GUILD_EVENT, 1);
    data << (uint8)GE_DISBANDED;
    BroadcastPacket(&data);

    while (!members.empty())
    {
        auto itr = members.begin();
        DeleteMember(ObjectGuid(HighGuid::Player, itr->first), true);
    }

    if (Id == 0)
        return;

    SQLTransaction trans = CharacterDatabase.BeginTransaction();
    trans->PAppend("DELETE FROM guild WHERE guildid = '%u'",Id);
    trans->PAppend("DELETE FROM guild_rank WHERE guildid = '%u'",Id);
    trans->PAppend("DELETE FROM guild_bank_tab WHERE guildid = '%u'",Id);
    trans->PAppend("DELETE FROM guild_bank_item WHERE guildid = '%u'",Id);
    trans->PAppend("DELETE FROM guild_bank_right WHERE guildid = '%u'",Id);
    trans->PAppend("DELETE FROM guild_bank_eventlog WHERE guildid = '%u'",Id);
    trans->PAppend("DELETE FROM guild_eventlog WHERE guildid = '%u'",Id);
    CharacterDatabase.CommitTransaction(trans);
    sObjectMgr->RemoveGuild(Id);
}

void Guild::Roster(WorldSession *session /*= NULL*/)
{
                                                            // we can only guess size
    WorldPacket data(SMSG_GUILD_ROSTER, (4+MOTD.length()+1+GINFO.length()+1+4+m_ranks.size()*(4+4+GUILD_BANK_MAX_TABS*(4+4))+members.size()*50));
    data << (uint32)members.size();
    data << MOTD;
    data << GINFO;

    data << (uint32)m_ranks.size();
    for (RankList::const_iterator ritr = m_ranks.begin(); ritr != m_ranks.end(); ++ritr)
    {
        data << (uint32)ritr->rights;
        data << (uint32)ritr->BankMoneyPerDay;              // count of: withdraw gold(gold/day) Note: in game set gold, in packet set bronze.
        for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
        {
            data << (uint32)ritr->TabRight[i];              // for TAB_i rights: view tabs = 0x01, deposit items =0x02
            data << (uint32)ritr->TabSlotPerDay[i];         // for TAB_i count of: withdraw items(stack/day)
        }
    }
    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        if (Player *pl = ObjectAccessor::FindPlayer(ObjectGuid(HighGuid::Player, itr->first)))
        {
            data << (uint64)pl->GetGUID();
            data << (uint8)1;
            data << (std::string)pl->GetName();
            data << (uint32)itr->second.RankId;
            data << (uint8)pl->GetLevel();
            data << (uint8)pl->GetClass();
            data << (uint8)0;                               // new 2.4.0
            data << (uint32)pl->GetZoneId();
            data << itr->second.Pnote;
            data << itr->second.OFFnote;
        }
        else
        {
            data << uint64(ObjectGuid(HighGuid::Player, itr->first));
            data << (uint8)0;
            data << itr->second.name;
            data << (uint32)itr->second.RankId;
            data << (uint8)itr->second.level;
            data << (uint8)itr->second.Class;
            data << (uint8)0;                               // new 2.4.0
            data << (uint32)itr->second.zoneId;
            data << (float(time(nullptr)-itr->second.logout_time) / DAY);
            data << itr->second.Pnote;
            data << itr->second.OFFnote;
        }
    }
    if (session)
        session->SendPacket(&data);
    else
        BroadcastPacket(&data);
}

void Guild::Query(WorldSession *session)
{
    WorldPacket data(SMSG_GUILD_QUERY_RESPONSE, (8*32+200));// we can only guess size

    data << uint32(Id);
    data << name;

    for (size_t i = 0 ; i < GUILD_MAX_RANKS_COUNT; ++i)                        // show always 10 ranks
    {
        if(i < m_ranks.size())
            data << m_ranks[i].name;
        else
            data << (uint8)0;                               // null string
    }

    data << uint32(EmblemStyle);
    data << uint32(EmblemColor);
    data << uint32(BorderStyle);
    data << uint32(BorderColor);
    data << uint32(BackgroundColor);
    if(session->GetClientBuild() == BUILD_335)
        data << uint32(m_ranks.size());                                // Number of ranks used

    session->SendPacket( &data );
}

void Guild::SetEmblem(uint32 emblemStyle, uint32 emblemColor, uint32 borderStyle, uint32 borderColor, uint32 backgroundColor)
{
    EmblemStyle = emblemStyle;
    EmblemColor = emblemColor;
    BorderStyle = borderStyle;
    BorderColor = borderColor;
    BackgroundColor = backgroundColor;

    CharacterDatabase.PExecute("UPDATE guild SET EmblemStyle=%u, EmblemColor=%u, BorderStyle=%u, BorderColor=%u, BackgroundColor=%u WHERE guildid = %u", EmblemStyle, EmblemColor, BorderStyle, BorderColor, BackgroundColor, Id);
}

void Guild::UpdateLogoutTime(ObjectGuid guid)
{
    auto itr = members.find(guid.GetCounter());
    if (itr == members.end() )
        return;

    itr->second.logout_time = time(nullptr);

    if (m_onlinemembers > 0)
        --m_onlinemembers;
    else
    {
        UnloadGuildBank();
        UnloadGuildEventlog();
    }
}

void Guild::UpdateAccountsNumber()
{
    // We use a set to be sure each element will be unique
    std::set<uint32> accountsIdSet;
    for (MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
        accountsIdSet.insert(itr->second.accountId);
        
    m_accountsNumber = accountsIdSet.size();
}

// *************************************************
// Guild Eventlog part
// *************************************************
// Display guild eventlog
void Guild::DisplayGuildEventlog(WorldSession *session)
{
    // Load guild eventlog, if not already done
    if (!m_eventlogloaded)
        LoadGuildEventLogFromDB();

    // Sending result
    WorldPacket data(MSG_GUILD_EVENT_LOG_QUERY, 0);
    // count, max count == 100
    data << uint8(m_GuildEventlog.size());
    for (GuildEventlog::const_iterator itr = m_GuildEventlog.begin(); itr != m_GuildEventlog.end(); ++itr)
    {
        // Event type
        data << uint8((*itr)->EventType);
        // Player 1
        data << uint64((*itr)->PlayerGuid1);
        // Player 2 not for left/join guild events
        if( (*itr)->EventType != GUILD_EVENT_LOG_JOIN_GUILD && (*itr)->EventType != GUILD_EVENT_LOG_LEAVE_GUILD )
            data << uint64((*itr)->PlayerGuid2);
        // New Rank - only for promote/demote guild events
        if( (*itr)->EventType == GUILD_EVENT_LOG_PROMOTE_PLAYER || (*itr)->EventType == GUILD_EVENT_LOG_DEMOTE_PLAYER )
            data << uint8((*itr)->NewRank);
        // Event timestamp
        data << uint32(time(nullptr)-(*itr)->TimeStamp);
    }
    session->SendPacket(&data);
}

// Load guild eventlog from DB
void Guild::LoadGuildEventLogFromDB()
{
    // Return if already loaded
    if (m_eventlogloaded)
        return;

    QueryResult result = CharacterDatabase.PQuery("SELECT LogGuid, EventType, PlayerGuid1, PlayerGuid2, NewRank, TimeStamp FROM guild_eventlog WHERE guildid=%u ORDER BY LogGuid DESC LIMIT %u", Id, GUILD_EVENTLOG_MAX_ENTRIES);
    if(!result)
        return;
    do
    {
        Field *fields = result->Fetch();
        auto NewEvent = new GuildEventlogEntry;
        // Fill entry
        NewEvent->LogGuid = fields[0].GetUInt32();
        NewEvent->EventType = fields[1].GetUInt8();
        NewEvent->PlayerGuid1 = fields[2].GetUInt32();
        NewEvent->PlayerGuid2 = fields[3].GetUInt32();
        NewEvent->NewRank = fields[4].GetUInt8();
        NewEvent->TimeStamp = fields[5].GetUInt64();
        // Add entry to map
        m_GuildEventlog.push_front(NewEvent);

    } while( result->NextRow() );

    // Check lists size in case to many event entries in db
    // This cases can happen only if a crash occured somewhere and table has too many log entries
    if (!m_GuildEventlog.empty())
    {
        CharacterDatabase.PExecute("DELETE FROM guild_eventlog WHERE guildid=%u AND LogGuid < %u", Id, m_GuildEventlog.front()->LogGuid);
    }
    m_eventlogloaded = true;
}

// Unload guild eventlog
void Guild::UnloadGuildEventlog()
{
    if (!m_eventlogloaded)
        return;
    GuildEventlogEntry *EventLogEntry;
    if( !m_GuildEventlog.empty() )
    {
        do
        {
            EventLogEntry = *(m_GuildEventlog.begin());
            m_GuildEventlog.pop_front();
            delete EventLogEntry;
        }while( !m_GuildEventlog.empty() );
    }
    m_eventlogloaded = false;
}

// This will renum guids used at load to prevent always going up until infinit
void Guild::RenumGuildEventlog()
{
    QueryResult result = CharacterDatabase.PQuery("SELECT Min(LogGuid), Max(LogGuid) FROM guild_eventlog WHERE guildid = %u", Id);
    if(!result)
        return;

    Field *fields = result->Fetch();
    if (fields[0].GetUInt32() == 1) {
        return;
    }

    CharacterDatabase.PExecute("UPDATE guild_eventlog SET LogGuid=LogGuid-%u+1 WHERE guildid=%u ORDER BY LogGuid %s",fields[0].GetUInt32(), Id, fields[0].GetUInt32()?"ASC":"DESC");
    GuildEventlogMaxGuid = fields[1].GetUInt32()+1;
}

// Add entry to guild eventlog
void Guild::LogGuildEvent(uint8 EventType, ObjectGuid::LowType PlayerGuid1, ObjectGuid::LowType PlayerGuid2, uint8 NewRank)
{
    auto NewEvent = new GuildEventlogEntry;
    // Fill entry
    NewEvent->LogGuid = GuildEventlogMaxGuid++;
    NewEvent->EventType = EventType;
    NewEvent->PlayerGuid1 = PlayerGuid1;
    NewEvent->PlayerGuid2 = PlayerGuid2;
    NewEvent->NewRank = NewRank;
    NewEvent->TimeStamp = uint32(time(nullptr));
    SQLTransaction trans = CharacterDatabase.BeginTransaction();
    // Check max entry limit and delete from db if needed
    if (m_GuildEventlog.size() > GUILD_EVENTLOG_MAX_ENTRIES)
    {
        GuildEventlogEntry *OldEvent = *(m_GuildEventlog.begin());
        m_GuildEventlog.pop_front();
        trans->PAppend("DELETE FROM guild_eventlog WHERE guildid='%u' AND LogGuid='%u'", Id, OldEvent->LogGuid);
        delete OldEvent;
    }
    // Add entry to map
    m_GuildEventlog.push_back(NewEvent);
    // Add new eventlog entry into DB
    trans->PAppend("INSERT INTO guild_eventlog (guildid, LogGuid, EventType, PlayerGuid1, PlayerGuid2, NewRank, TimeStamp) VALUES ('%u','%u','%u','%u','%u','%u','" UI64FMTD "')",
        Id, NewEvent->LogGuid, uint32(NewEvent->EventType), NewEvent->PlayerGuid1, NewEvent->PlayerGuid2, uint32(NewEvent->NewRank), NewEvent->TimeStamp);
    CharacterDatabase.CommitTransaction(trans);
}

// *************************************************
// Guild Bank part
// *************************************************
// Bank content related
void Guild::DisplayGuildBankContent(WorldSession *session, uint8 TabId)
{
    WorldPacket data(SMSG_GUILD_BANK_LIST,1200);

    GuildBankTab const* tab = GetBankTab(TabId);
    if (!tab)
        return;

    if(!IsMemberHaveRights(session->GetPlayer()->GetGUID().GetCounter(),TabId,GUILD_BANK_RIGHT_VIEW_TAB))
        return;

    data << uint64(GetGuildBankMoney());
    data << uint8(TabId);
                                                            // remaining slots for today
    data << uint32(GetMemberSlotWithdrawRem(session->GetPlayer()->GetGUID().GetCounter(), TabId));
    data << uint8(0);                                       // Tell client this is a tab content packet

    data << uint8(GUILD_BANK_MAX_SLOTS);

    for (int i=0; i<GUILD_BANK_MAX_SLOTS; ++i)
        AppendDisplayGuildBankSlot(data, tab, i);

    session->SendPacket(&data);
}

void Guild::DisplayGuildBankMoneyUpdate()
{
    WorldPacket data(SMSG_GUILD_BANK_LIST, 8+1+4+1+1);

    data << uint64(GetGuildBankMoney());
    data << uint8(0);                                       // TabId, default 0
    data << uint32(0);                                      // slot withdrow, default 0
    data << uint8(0);                                       // Tell client this is a tab content packet
    data << uint8(0);                                       // not send items
    BroadcastPacket(&data);
}

void Guild::DisplayGuildBankContentUpdate(uint8 TabId, int32 slot1, int32 slot2)
{
    GuildBankTab const* tab = GetBankTab(TabId);
    if (!tab)
        return;

    WorldPacket data(SMSG_GUILD_BANK_LIST,1200);

    data << uint64(GetGuildBankMoney());
    data << uint8(TabId);
    // remaining slots for today

    size_t rempos = data.wpos();
    data << uint32(0);                                      // will be filled later
    data << uint8(0);                                       // Tell client this is a tab content packet

    if(slot2==-1)                                           // single item in slot1
    {
        data << uint8(1);

        AppendDisplayGuildBankSlot(data, tab, slot1);
    }
    else                                                    // 2 items (in slot1 and slot2)
    {
        data << uint8(2);

        if(slot1 > slot2)
            std::swap(slot1,slot2);

        AppendDisplayGuildBankSlot(data, tab, slot1);
        AppendDisplayGuildBankSlot(data, tab, slot2);
    }

    for(MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        Player *player = ObjectAccessor::FindPlayer(ObjectGuid(HighGuid::Player, itr->first));
        if(!player)
            continue;

        if(!IsMemberHaveRights(itr->first,TabId,GUILD_BANK_RIGHT_VIEW_TAB))
            continue;

        data.put<uint32>(rempos,uint32(GetMemberSlotWithdrawRem(player->GetGUID().GetCounter(), TabId)));

        player->SendDirectMessage(&data);
    }
}

void Guild::DisplayGuildBankContentUpdate(uint8 TabId, GuildItemPosCountVec const& slots)
{
    GuildBankTab const* tab = GetBankTab(TabId);
    if (!tab)
        return;

    WorldPacket data(SMSG_GUILD_BANK_LIST,1200);

    data << uint64(GetGuildBankMoney());
    data << uint8(TabId);
    // remaining slots for today

    size_t rempos = data.wpos();
    data << uint32(0);                                      // will be filled later
    data << uint8(0);                                       // Tell client this is a tab content packet

    data << uint8(slots.size());                            // updates count

    for(auto slot : slots)
        AppendDisplayGuildBankSlot(data, tab, slot.slot);

    for(MemberList::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        Player *player = ObjectAccessor::FindPlayer(ObjectGuid(HighGuid::Player, itr->first));
        if(!player)
            continue;

        if(!IsMemberHaveRights(itr->first,TabId,GUILD_BANK_RIGHT_VIEW_TAB))
            continue;

        data.put<uint32>(rempos,uint32(GetMemberSlotWithdrawRem(player->GetGUID().GetCounter(), TabId)));

        player->SendDirectMessage(&data);
    }
}

Item* Guild::GetItem(uint8 TabId, uint8 SlotId)
{
    if (TabId >= m_TabListMap.size() || SlotId >= GUILD_BANK_MAX_SLOTS)
        return nullptr;
    return m_TabListMap[TabId]->Slots[SlotId];
}

// *************************************************
// Tab related

void Guild::DisplayGuildBankTabsInfo(WorldSession *session)
{
    // Time to load bank if not already done
    if (!m_bankloaded)
        LoadGuildBankFromDB();

    WorldPacket data(SMSG_GUILD_BANK_LIST, 500);

    data << uint64(GetGuildBankMoney());
    data << uint8(0);                                       // TabInfo packet must be for TabId 0
    data << uint32(0xFFFFFFFF);                             // bit 9 must be set for this packet to work
    data << uint8(1);                                       // Tell Client this is a TabInfo packet

    data << uint8(purchased_tabs);                          // here is the number of tabs

    for(int i = 0; i < purchased_tabs; ++i)
    {
        data << m_TabListMap[i]->Name.c_str();
        data << m_TabListMap[i]->Icon.c_str();
    }
    data << uint8(0);                                       // Do not send tab content
    session->SendPacket(&data);
}

void Guild::CreateNewBankTab()
{
    if (purchased_tabs >= GUILD_BANK_MAX_TABS)
        return;

    ++purchased_tabs;

    auto  AnotherTab = new GuildBankTab;
    memset(AnotherTab->Slots, 0, GUILD_BANK_MAX_SLOTS * sizeof(Item*));
    m_TabListMap.resize(purchased_tabs);
    m_TabListMap[purchased_tabs-1] = AnotherTab;

    SQLTransaction trans = CharacterDatabase.BeginTransaction();
    trans->PAppend("DELETE FROM guild_bank_tab WHERE guildid='%u' AND TabId='%u'", Id, uint32(purchased_tabs-1));
    trans->PAppend("INSERT INTO guild_bank_tab (guildid,TabId) VALUES ('%u','%u')", Id, uint32(purchased_tabs-1));
    CharacterDatabase.CommitTransaction(trans);
}

void Guild::SetGuildBankTabInfo(uint8 TabId, std::string Name, std::string Icon)
{
    if (TabId >= GUILD_BANK_MAX_TABS)
        return;
    if (TabId >= m_TabListMap.size())
        return;

    if (!m_TabListMap[TabId])
        return;

    if(m_TabListMap[TabId]->Name == Name && m_TabListMap[TabId]->Icon == Icon)
        return;

    m_TabListMap[TabId]->Name = Name;
    m_TabListMap[TabId]->Icon = Icon;

    CharacterDatabase.EscapeString(Name);
    CharacterDatabase.EscapeString(Icon);
    CharacterDatabase.PExecute("UPDATE guild_bank_tab SET TabName='%s',TabIcon='%s' WHERE guildid='%u' AND TabId='%u'", Name.c_str(), Icon.c_str(), Id, uint32(TabId));
}

void Guild::CreateBankRightForTab(uint32 rankId, uint8 TabId, SQLTransaction trans)
{
    if (rankId >= m_ranks.size() || TabId >= GUILD_BANK_MAX_TABS)
        return;

    m_ranks[rankId].TabRight[TabId]=0;
    m_ranks[rankId].TabSlotPerDay[TabId]=0;
    trans->PAppend("DELETE FROM guild_bank_right WHERE guildid = '%u' AND TabId = '%u' AND rid = '%u'", Id, uint32(TabId), rankId);
    trans->PAppend("INSERT INTO guild_bank_right (guildid,TabId,rid) VALUES ('%u','%u','%u')", Id, uint32(TabId), rankId);
}

uint32 Guild::GetBankRights(uint32 rankId, uint8 TabId) const
{
    if(rankId >= m_ranks.size() || TabId >= GUILD_BANK_MAX_TABS)
        return 0;

    return m_ranks[rankId].TabRight[TabId];
}

// *************************************************
// Guild bank loading/unloading related

// This load should be called when the bank is first accessed by a guild member
void Guild::LoadGuildBankFromDB()
{
    if (m_bankloaded)
        return;

    m_bankloaded = true;
    LoadGuildBankEventLogFromDB();

    //                                                     0      1        2        3
    QueryResult result = CharacterDatabase.PQuery("SELECT TabId, TabName, TabIcon, TabText FROM guild_bank_tab WHERE guildid='%u' ORDER BY TabId", Id);
    if(!result)
    {
        purchased_tabs = 0;
        return;
    }

    m_TabListMap.resize(purchased_tabs);
    do
    {
        Field *fields = result->Fetch();
        uint8 TabId = fields[0].GetUInt8();

        auto NewTab = new GuildBankTab;
        memset(NewTab->Slots, 0, GUILD_BANK_MAX_SLOTS * sizeof(Item*));

        NewTab->Name = fields[1].GetString();
        NewTab->Icon = fields[2].GetString();
        NewTab->Text = fields[3].GetString();

        m_TabListMap[TabId] = NewTab;
    }while( result->NextRow() );

    // data needs to be at first place for Item::LoadFromDB
    //                                          0     1          2          3    
    result = CharacterDatabase.PQuery("SELECT TabId, SlotId, item_guid, item_entry FROM guild_bank_item WHERE guildid='%u' ORDER BY TabId", Id);
    if(!result)
        return;

    do
    {
        Field *fields = result->Fetch();
        uint8 TabId = fields[0].GetUInt8();
        uint8 SlotId = fields[1].GetUInt8();
        ObjectGuid::LowType ItemGuid = fields[2].GetUInt32();
        uint32 ItemEntry = fields[3].GetUInt32();

        if (TabId >= purchased_tabs || TabId >= GUILD_BANK_MAX_TABS)
        {
            TC_LOG_ERROR( "guild", "Guild::LoadGuildBankFromDB: Invalid tab for item (GUID: %u id: #%u) in guild bank, skipped.", ItemGuid,ItemEntry);
            continue;
        }

        if (SlotId >= GUILD_BANK_MAX_SLOTS)
        {
            TC_LOG_ERROR( "guild", "Guild::LoadGuildBankFromDB: Invalid slot for item (GUID: %u id: #%u) in guild bank, skipped.", ItemGuid,ItemEntry);
            continue;
        }

        ItemTemplate const *proto = sObjectMgr->GetItemTemplate(ItemEntry);
        if(!proto)
        {
            TC_LOG_ERROR( "guild", "Guild::LoadGuildBankFromDB: Unknown item (GUID: %u id: #%u) in guild bank, skipped.", ItemGuid,ItemEntry);
            continue;
        }

        Item *pItem = NewItemOrBag(proto);

        PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ITEM_INSTANCE);
        stmt->setUInt32(0, ItemGuid);
        PreparedQueryResult result2 = CharacterDatabase.Query(stmt);
        if(!result2 || !pItem->LoadFromDB(ItemGuid, ObjectGuid::Empty, result2->Fetch(), ItemEntry))
        {
            CharacterDatabase.PExecute("DELETE FROM guild_bank_item WHERE guildid='%u' AND TabId='%u' AND SlotId='%u'", Id, uint32(TabId), uint32(SlotId)); // Dangerous DELETE
            TC_LOG_ERROR("guild","Item GUID %u not found in item_instance, deleting from Guild Bank!", ItemGuid);
            delete pItem;
            continue;
        }

        pItem->AddToWorld();
        m_TabListMap[TabId]->Slots[SlotId] = pItem;
    }while( result->NextRow() );
}

// This unload should be called when the last member of the guild gets offline
void Guild::UnloadGuildBank()
{
    if (!m_bankloaded)
        return;
    for (uint8 i = 0 ; i < purchased_tabs ; ++i )
    {
        for (auto & Slot : m_TabListMap[i]->Slots)
        {
            if (Slot)
            {
                Slot->RemoveFromWorld();
                delete Slot;
            }
        }
        delete m_TabListMap[i];
    }
    m_TabListMap.clear();

    UnloadGuildBankEventLog();
    m_bankloaded = false;
}

// *************************************************
// Money deposit/withdraw related

void Guild::SendMoneyInfo(WorldSession *session, ObjectGuid::LowType LowGuid)
{
    WorldPacket data(MSG_GUILD_BANK_MONEY_WITHDRAWN, 4);
    data << uint32(GetMemberMoneyWithdrawRem(LowGuid));
    session->SendPacket(&data);
}

bool Guild::HandleMemberWithdrawMoney(uint32 amount, ObjectGuid::LowType LowGuid, SQLTransaction trans)
{
    Player* player = ObjectAccessor::FindConnectedPlayer(ObjectGuid(HighGuid::Player, LowGuid));
    if(!player)
        return false;

    //ensure player wont withdraw more money than he can hold
    uint32 maxPossibleWithdraw = MAX_MONEY_AMOUNT - player->GetMoney();
    amount = std::min(amount, maxPossibleWithdraw);
    if(amount == 0)
        return false;

    uint32 MoneyWithDrawRight = GetMemberMoneyWithdrawRem(LowGuid);

    if (MoneyWithDrawRight < amount || GetGuildBankMoney() < amount)
        return false;

    SetBankMoney(GetGuildBankMoney()-amount, trans);

    if (MoneyWithDrawRight < WITHDRAW_MONEY_UNLIMITED)
    {
        auto itr = members.find(LowGuid);
        if (itr == members.end() )
            return false;
        itr->second.BankRemMoney -= amount;
        trans->PAppend("UPDATE guild_member SET BankRemMoney='%u' WHERE guildid='%u' AND guid='%u'",
            itr->second.BankRemMoney, Id, LowGuid);
    }

    return true;
}

void Guild::SetBankMoney(int64 money, SQLTransaction trans)
{
    if (money < 0)                                          // I don't know how this happens, it does!!
        money = 0;
    guildbank_money = money;

    trans->PAppend("UPDATE guild SET BankMoney='" UI64FMTD "' WHERE guildid='%u'", money, Id);
}

// *************************************************
// Item per day and money per day related

bool Guild::MemberItemWithdraw(uint8 TabId, ObjectGuid::LowType LowGuid)
{
    uint32 SlotsWithDrawRight = GetMemberSlotWithdrawRem(LowGuid, TabId);

    if (SlotsWithDrawRight == 0)
        return false;

    if (SlotsWithDrawRight < WITHDRAW_SLOT_UNLIMITED)
    {
        auto itr = members.find(LowGuid);
        if (itr == members.end() )
            return false;
        --itr->second.BankRemSlotsTab[TabId];
        CharacterDatabase.PExecute("UPDATE guild_member SET BankRemSlotsTab%u='%u' WHERE guildid='%u' AND guid='%u'",
            uint32(TabId), itr->second.BankRemSlotsTab[TabId], Id, LowGuid);
    }
    return true;
}

bool Guild::IsMemberHaveRights(ObjectGuid::LowType LowGuid, uint8 TabId, uint32 rights) const
{
    auto itr = members.find(LowGuid);
    if (itr == members.end() )
        return false;

    if (itr->second.RankId == GR_GUILDMASTER)
        return true;

    return (GetBankRights(itr->second.RankId,TabId) & rights)==rights;
}

uint32 Guild::GetMemberSlotWithdrawRem(ObjectGuid::LowType LowGuid, uint8 TabId)
{
    auto itr = members.find(LowGuid);
    if (itr == members.end() )
        return 0;

    if (itr->second.RankId == GR_GUILDMASTER)
        return WITHDRAW_SLOT_UNLIMITED;

    if((GetBankRights(itr->second.RankId,TabId) & GUILD_BANK_RIGHT_VIEW_TAB)!=GUILD_BANK_RIGHT_VIEW_TAB)
        return 0;

    uint32 curTime = uint32(time(nullptr)/MINUTE);
    if (curTime - itr->second.BankResetTimeTab[TabId] >= 24*HOUR/MINUTE)
    {
        itr->second.BankResetTimeTab[TabId] = curTime;
        itr->second.BankRemSlotsTab[TabId] = GetBankSlotPerDay(itr->second.RankId, TabId);
        CharacterDatabase.PExecute("UPDATE guild_member SET BankResetTimeTab%u='%u',BankRemSlotsTab%u='%u' WHERE guildid='%u' AND guid='%u'",
            uint32(TabId), itr->second.BankResetTimeTab[TabId], uint32(TabId), itr->second.BankRemSlotsTab[TabId], Id, LowGuid);
    }
    return itr->second.BankRemSlotsTab[TabId];
}

uint32 Guild::GetMemberMoneyWithdrawRem(ObjectGuid::LowType LowGuid)
{
    auto itr = members.find(LowGuid);
    if (itr == members.end() )
        return 0;

    if (itr->second.RankId == GR_GUILDMASTER)
        return WITHDRAW_MONEY_UNLIMITED;

    uint32 curTime = uint32(time(nullptr)/MINUTE);             // minutes
                                                            // 24 hours
    if (curTime > itr->second.BankResetTimeMoney + 24*HOUR/MINUTE)
    {
        itr->second.BankResetTimeMoney = curTime;
        itr->second.BankRemMoney = GetBankMoneyPerDay(itr->second.RankId);
        CharacterDatabase.PExecute("UPDATE guild_member SET BankResetTimeMoney='%u',BankRemMoney='%u' WHERE guildid='%u' AND guid='%u'",
            itr->second.BankResetTimeMoney, itr->second.BankRemMoney, Id, LowGuid);
    }
    return itr->second.BankRemMoney;
}

void Guild::SetBankMoneyPerDay(uint32 rankId, uint32 money)
{
    if (rankId >= m_ranks.size())
        return;

    if (rankId == GR_GUILDMASTER)
        money = WITHDRAW_MONEY_UNLIMITED;

    m_ranks[rankId].BankMoneyPerDay = money;

    for (auto & member : members)
        if (member.second.RankId == rankId)
            member.second.BankResetTimeMoney = 0;

    CharacterDatabase.PExecute("UPDATE guild_rank SET BankMoneyPerDay='%u' WHERE rid='%u' AND guildid='%u'", money, (rankId+1), Id);
    CharacterDatabase.PExecute("UPDATE guild_member SET BankResetTimeMoney='0' WHERE guildid='%u' AND `rank`='%u'", Id, rankId);
}

void Guild::SetBankRightsAndSlots(uint32 rankId, uint8 TabId, uint32 right, uint32 nbSlots, bool db)
{
    if(rankId >= m_ranks.size() ||
        TabId >= GUILD_BANK_MAX_TABS ||
        TabId >= purchased_tabs)
        return;

    if (rankId == GR_GUILDMASTER)
    {
        nbSlots = WITHDRAW_SLOT_UNLIMITED;
        right = GUILD_BANK_RIGHT_FULL;
    }

    m_ranks[rankId].TabSlotPerDay[TabId]=nbSlots;
    m_ranks[rankId].TabRight[TabId]=right;

    if (db)
    {
        for (auto & member : members)
            if (member.second.RankId == rankId)
                for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
                    member.second.BankResetTimeTab[i] = 0;

        //CharacterDatabase.PExecute("DELETE FROM guild_bank_right WHERE guildid='%u' AND TabId='%u' AND rid='%u'", Id, uint32(TabId), rankId);
        CharacterDatabase.PExecute("REPLACE INTO guild_bank_right (guildid,TabId,rid,gbright,SlotPerDay) VALUES "
            "('%u','%u','%u','%u','%u')", Id, uint32(TabId), rankId, m_ranks[rankId].TabRight[TabId], m_ranks[rankId].TabSlotPerDay[TabId]);
        CharacterDatabase.PExecute("UPDATE guild_member SET BankResetTimeTab%u='0' WHERE guildid='%u' AND `rank`='%u'", uint32(TabId), Id, rankId);
    }
}

uint32 Guild::GetBankMoneyPerDay(uint32 rankId)
{
    if(rankId >= m_ranks.size())
        return 0;

    if (rankId == GR_GUILDMASTER)
        return WITHDRAW_MONEY_UNLIMITED;
    return m_ranks[rankId].BankMoneyPerDay;
}

uint32 Guild::GetBankSlotPerDay(uint32 rankId, uint8 TabId)
{
    if(rankId >= m_ranks.size() || TabId >= GUILD_BANK_MAX_TABS)
        return 0;

    if (rankId == GR_GUILDMASTER)
        return WITHDRAW_SLOT_UNLIMITED;
    return m_ranks[rankId].TabSlotPerDay[TabId];
}

// *************************************************
// Rights per day related

void Guild::LoadBankRightsFromDB(uint32 GuildId)
{
    //                                                     0      1    2        3
    QueryResult result = CharacterDatabase.PQuery("SELECT TabId, rid, gbright, SlotPerDay FROM guild_bank_right WHERE guildid = '%u' ORDER BY TabId", GuildId);

    if(!result)
        return;

    do
    {
        Field *fields = result->Fetch();
        uint8 TabId = fields[0].GetUInt8();
        uint32 rankId = fields[1].GetUInt32();
        uint16 right = fields[2].GetUInt8();
        uint16 SlotPerDay = fields[3].GetUInt32();

        SetBankRightsAndSlots(rankId, TabId, right, SlotPerDay, false);

    }while( result->NextRow() );

    return;
}

// *************************************************
// Bank log related

void Guild::LoadGuildBankEventLogFromDB()
{
    // We can't add a limit as in Guild::LoadGuildEventLogFromDB since we fetch both money and bank log and know nothing about the composition
    //                                                     0        1         2      3           4            5               6          7
    QueryResult result = CharacterDatabase.PQuery("SELECT LogGuid, LogEntry, TabId, PlayerGuid, ItemOrMoney, ItemStackCount, DestTabId, TimeStamp FROM guild_bank_eventlog WHERE guildid='%u' ORDER BY TimeStamp DESC", Id);
    if(!result)
        return;

    do
    {
        Field *fields = result->Fetch();
        auto NewEvent = new GuildBankEvent;

        NewEvent->LogGuid = fields[0].GetUInt32();
        NewEvent->LogEntry = fields[1].GetUInt8();
        uint8 TabId = fields[2].GetUInt8();
        NewEvent->PlayerGuid = fields[3].GetUInt32();
        NewEvent->ItemOrMoney = fields[4].GetUInt32();
        NewEvent->ItemStackCount = fields[5].GetUInt8();
        NewEvent->DestTabId = fields[6].GetUInt8();
        NewEvent->TimeStamp = fields[7].GetUInt64();

        if (TabId >= GUILD_BANK_MAX_TABS)
        {
            TC_LOG_ERROR( "guild", "Guild::LoadGuildBankEventLogFromDB: Invalid tabid '%u' for guild bank log entry (guild: '%s', LogGuid: %u), skipped.", TabId, GetName().c_str(), NewEvent->LogGuid);
            delete NewEvent;
            continue;
        }
        if ((NewEvent->isMoneyEvent() && m_GuildBankEventLog_Money.size() >= GUILD_BANK_MAX_LOGS)
                || m_GuildBankEventLog_Item[TabId].size() >= GUILD_BANK_MAX_LOGS)
        {
            delete NewEvent;
            continue;
        }
        if (NewEvent->isMoneyEvent())
            m_GuildBankEventLog_Money.push_front(NewEvent);
        else
            m_GuildBankEventLog_Item[TabId].push_front(NewEvent);

    }while( result->NextRow() );

    // Check lists size in case to many event entries in db for a tab or for money
    // This cases can happen only if a crash occured somewhere and table has too many log entries
    if (!m_GuildBankEventLog_Money.empty())
    {
        CharacterDatabase.PExecute("DELETE FROM guild_bank_eventlog WHERE guildid=%u AND LogGuid < %u",
            Id, m_GuildBankEventLog_Money.front()->LogGuid);
    }
    for (auto & i : m_GuildBankEventLog_Item)
    {
        if (!i.empty())
        {
            CharacterDatabase.PExecute("DELETE FROM guild_bank_eventlog WHERE guildid=%u AND LogGuid < %u",
                Id, i.front()->LogGuid);
        }
    }
}

void Guild::UnloadGuildBankEventLog()
{
    GuildBankEvent *EventLogEntry;
    if( !m_GuildBankEventLog_Money.empty() )
    {
        do
        {
            EventLogEntry = *(m_GuildBankEventLog_Money.begin());
            m_GuildBankEventLog_Money.pop_front();
            delete EventLogEntry;
        }while( !m_GuildBankEventLog_Money.empty() );
    }

    for (auto & i : m_GuildBankEventLog_Item)
    {
        if( !i.empty() )
        {
            do
            {
                EventLogEntry = *(i.begin());
                i.pop_front();
                delete EventLogEntry;
            }while( !i.empty() );
        }
    }
}

void Guild::DisplayGuildBankLogs(WorldSession *session, uint8 TabId)
{
    if (TabId > GUILD_BANK_MAX_TABS)
        return;

    if (TabId == GUILD_BANK_MAX_TABS)
    {
        // Here we display money logs
        WorldPacket data(MSG_GUILD_BANK_LOG_QUERY, m_GuildBankEventLog_Money.size()*(4*4+1)+1+1);
        data << uint8(TabId);                               // Here GUILD_BANK_MAX_TABS
        data << uint8(m_GuildBankEventLog_Money.size());    // number of log entries
        for (GuildBankEventLog::const_iterator itr = m_GuildBankEventLog_Money.begin(); itr != m_GuildBankEventLog_Money.end(); ++itr)
        {
            data << uint8((*itr)->LogEntry);
            data << uint64(ObjectGuid(HighGuid::Player, (*itr)->PlayerGuid));
            data << uint32((*itr)->ItemOrMoney);
            data << uint32(time(nullptr)-(*itr)->TimeStamp);
        }
        session->SendPacket(&data);
    }
    else
    {
        // here we display current tab logs
        WorldPacket data(MSG_GUILD_BANK_LOG_QUERY, m_GuildBankEventLog_Item[TabId].size()*(4*4+1+1)+1+1);
        data << uint8(TabId);                               // Here a real Tab Id
                                                            // number of log entries
        data << uint8(m_GuildBankEventLog_Item[TabId].size());
        for (GuildBankEventLog::const_iterator itr = m_GuildBankEventLog_Item[TabId].begin(); itr != m_GuildBankEventLog_Item[TabId].end(); ++itr)
        {
            data << uint8((*itr)->LogEntry);
            data << uint64(ObjectGuid(HighGuid::Player, (*itr)->PlayerGuid));
            data << uint32((*itr)->ItemOrMoney);
            data << uint8((*itr)->ItemStackCount);
            if ((*itr)->LogEntry == GUILD_BANK_LOG_MOVE_ITEM || (*itr)->LogEntry == GUILD_BANK_LOG_MOVE_ITEM2)
                data << uint8((*itr)->DestTabId);           // moved tab
            data << uint32(time(nullptr)-(*itr)->TimeStamp);
        }
        session->SendPacket(&data);
    }
}

void Guild::LogBankEvent(uint8 LogEntry, uint8 TabId, ObjectGuid::LowType PlayerGuidLow, uint32 ItemOrMoney, uint8 ItemStackCount, uint8 DestTabId)
{
    auto NewEvent = new GuildBankEvent;

    NewEvent->LogGuid = LogMaxGuid++;
    NewEvent->LogEntry = LogEntry;
    NewEvent->PlayerGuid = PlayerGuidLow;
    NewEvent->ItemOrMoney = ItemOrMoney;
    NewEvent->ItemStackCount = ItemStackCount;
    NewEvent->DestTabId = DestTabId;
    NewEvent->TimeStamp = uint32(time(nullptr));
    
    SQLTransaction trans = CharacterDatabase.BeginTransaction();

    if (NewEvent->isMoneyEvent())
    {
        if (m_GuildBankEventLog_Money.size() > GUILD_BANK_MAX_LOGS)
        {
            GuildBankEvent *OldEvent = *(m_GuildBankEventLog_Money.begin());
            m_GuildBankEventLog_Money.pop_front();
            trans->PAppend("DELETE FROM guild_bank_eventlog WHERE guildid='%u' AND LogGuid='%u'", Id, OldEvent->LogGuid);
            delete OldEvent;
        }
        m_GuildBankEventLog_Money.push_back(NewEvent);
    }
    else
    {
        if (m_GuildBankEventLog_Item[TabId].size() > GUILD_BANK_MAX_LOGS)
        {
            GuildBankEvent *OldEvent = *(m_GuildBankEventLog_Item[TabId].begin());
            m_GuildBankEventLog_Item[TabId].pop_front();
            trans->PAppend("DELETE FROM guild_bank_eventlog WHERE guildid='%u' AND LogGuid='%u'", Id, OldEvent->LogGuid);
            delete OldEvent;
        }
        m_GuildBankEventLog_Item[TabId].push_back(NewEvent);
    }
    trans->PAppend("INSERT INTO guild_bank_eventlog (guildid,LogGuid,LogEntry,TabId,PlayerGuid,ItemOrMoney,ItemStackCount,DestTabId,TimeStamp) VALUES ('%u','%u','%u','%u','%u','%u','%u','%u','" UI64FMTD "')",
        Id, NewEvent->LogGuid, uint32(NewEvent->LogEntry), uint32(TabId), NewEvent->PlayerGuid, NewEvent->ItemOrMoney, uint32(NewEvent->ItemStackCount), uint32(NewEvent->DestTabId), NewEvent->TimeStamp);
    
    CharacterDatabase.CommitTransaction(trans);
}

// This will renum guids used at load to prevent always going up until infinit
void Guild::RenumBankLogs()
{
    QueryResult result = CharacterDatabase.PQuery("SELECT Min(LogGuid), Max(LogGuid) FROM guild_bank_eventlog WHERE guildid = %u", Id);
    if(!result)
        return;

    Field *fields = result->Fetch();

    if (fields[0].GetUInt32() == 1)
        return;

    CharacterDatabase.PExecute("UPDATE guild_bank_eventlog SET LogGuid=LogGuid-%u+1 WHERE guildid=%u ORDER BY LogGuid %s",fields[0].GetUInt32(), Id, fields[0].GetUInt32()?"ASC":"DESC");
    LogMaxGuid = fields[1].GetUInt32()+1;
}

bool Guild::AddGBankItemToDB(uint32 GuildId, uint32 BankTab , uint32 BankTabSlot , ObjectGuid::LowType GUIDLow, uint32 Entry, SQLTransaction trans )
{
    //CharacterDatabase.PExecute("DELETE FROM guild_bank_item WHERE guildid = '%u' AND TabId = '%u'AND SlotId = '%u'", GuildId, BankTab, BankTabSlot);
    trans->PAppend("REPLACE INTO guild_bank_item (guildid,TabId,SlotId,item_guid,item_entry) "
        "VALUES ('%u', '%u', '%u', '%u', '%u')", GuildId, BankTab, BankTabSlot, GUIDLow, Entry);
    return true;
}

void Guild::AppendDisplayGuildBankSlot( WorldPacket& data, GuildBankTab const *tab, int slot )
{
    Item *pItem = tab->Slots[slot];
    uint32 entry = pItem ? pItem->GetEntry() : 0;

    data << uint8(slot);
    data << uint32(entry);
    if (entry)
    {
        // random item property id +8
        data << (uint32)pItem->GetItemRandomPropertyId();
        if (pItem->GetItemRandomPropertyId())
            // SuffixFactor +4
            data << (uint32)pItem->GetItemSuffixFactor();
        // +12 // ITEM_FIELD_STACK_COUNT
        data << uint8(pItem->GetCount());
        data << uint32(pItem->GetEnchantmentId(PERM_ENCHANTMENT_SLOT));   // +16 Permanent enchantment
        data << uint8(abs(pItem->GetSpellCharges()));       // Spell charges

        uint8 enchCount = 0;
        size_t enchCountPos = data.wpos();

        data << uint8(enchCount);                           // Number of enchantments
#ifdef LICH_KING
        for (uint32 i = PERM_ENCHANTMENT_SLOT; i < MAX_ENCHANTMENT_SLOT;                    ++i)
#else
        for (uint32 i = SOCK_ENCHANTMENT_SLOT; i < SOCK_ENCHANTMENT_SLOT + MAX_GEM_SOCKETS; ++i)
#endif
        {
            if (uint32 enchId = pItem->GetEnchantmentId(EnchantmentSlot(i)))
            {
#ifdef LICH_KING
                data << uint8(i);
#else
                data << uint8(i - SOCK_ENCHANTMENT_SLOT);
#endif
                data << uint32(enchId);
                ++enchCount;
            }
        }
        data.put<uint8>(enchCountPos, enchCount);
    }
}

Item* Guild::StoreItem(uint8 tabId, GuildItemPosCountVec const& dest, Item* pItem, SQLTransaction trans)
{
    if( !pItem )
        return nullptr;

    Item* lastItem = pItem;

    for(auto itr = dest.begin(); itr != dest.end(); )
    {
        uint8 slot = itr->slot;
        uint32 count = itr->count;

        ++itr;

        if(itr == dest.end())
        {
            lastItem = _StoreItem(tabId,slot,pItem,count,false, trans);
            break;
        }

        lastItem = _StoreItem(tabId,slot,pItem,count,true, trans);
    }

    return lastItem;
}

// Return stored item (if stored to stack, it can diff. from pItem). And pItem ca be deleted in this case.
Item* Guild::_StoreItem( uint8 tab, uint8 slot, Item *pItem, uint32 count, bool clone, SQLTransaction trans )
{
    if( !pItem )
        return nullptr;

    Item* pItem2 = m_TabListMap[tab]->Slots[slot];

    if( !pItem2 )
    {
        if(clone)
            pItem = pItem->CloneItem(count);
        else
            pItem->SetCount(count);

        if(!pItem)
            return nullptr;

        m_TabListMap[tab]->Slots[slot] = pItem;

        pItem->SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid::Empty);
        pItem->SetGuidValue(ITEM_FIELD_OWNER, ObjectGuid::Empty);
        AddGBankItemToDB(GetId(), tab, slot, pItem->GetGUID().GetCounter(), pItem->GetEntry(), trans);
        pItem->FSetState(ITEM_NEW);
        pItem->SaveToDB(trans);                                  // not in inventory and can be save standalone

        return pItem;
    }
    else
    {
        pItem2->SetCount( pItem2->GetCount() + count );
        pItem2->FSetState(ITEM_CHANGED);
        pItem2->SaveToDB(trans);                                  // not in inventory and can be save standalone

        if(!clone)
        {
            pItem->RemoveFromWorld();
            pItem->DeleteFromDB(trans);
            delete pItem;
        }

        return pItem2;
    }
}

void Guild::RemoveItem(uint8 tab, uint8 slot, SQLTransaction trans)
{
    m_TabListMap[tab]->Slots[slot] = nullptr;
    trans->PAppend("DELETE FROM guild_bank_item WHERE guildid='%u' AND TabId='%u' AND SlotId='%u'",
        GetId(), uint32(tab), uint32(slot));
}

uint8 Guild::_CanStoreItem_InSpecificSlot( uint8 tab, uint8 slot, GuildItemPosCountVec &dest, uint32& count, bool swap, Item* pSrcItem ) const
{
    Item* pItem2 = m_TabListMap[tab]->Slots[slot];

    // ignore move item (this slot will be empty at move)
    if(pItem2==pSrcItem)
        pItem2 = nullptr;

    uint32 need_space;

    // empty specific slot - check item fit to slot
    if( !pItem2 || swap )
    {
        // non empty stack with space
        need_space = pSrcItem->GetMaxStackCount();
    }
    // non empty slot, check item type
    else
    {
        // check item type
        if(pItem2->GetEntry() != pSrcItem->GetEntry())
            return EQUIP_ERR_ITEM_CANT_STACK;

        // check free space
        if(pItem2->GetCount() >= pSrcItem->GetMaxStackCount())
            return EQUIP_ERR_ITEM_CANT_STACK;

        need_space = pSrcItem->GetMaxStackCount() - pItem2->GetCount();
    }

    if(need_space > count)
        need_space = count;

    GuildItemPosCount newPosition = GuildItemPosCount(slot,need_space);
    if(!newPosition.isContainedIn(dest))
    {
        dest.push_back(newPosition);
        count -= need_space;
    }

    return EQUIP_ERR_OK;
}

uint8 Guild::_CanStoreItem_InTab( uint8 tab, GuildItemPosCountVec &dest, uint32& count, bool merge, Item* pSrcItem, uint8 skip_slot ) const
{
    for(uint32 j = 0; j < GUILD_BANK_MAX_SLOTS; j++)
    {
        // skip specific slot already processed in first called _CanStoreItem_InSpecificSlot
        if(j==skip_slot)
            continue;

        Item* pItem2 = m_TabListMap[tab]->Slots[j];

        // ignore move item (this slot will be empty at move)
        if(pItem2==pSrcItem)
            pItem2 = nullptr;

        // if merge skip empty, if !merge skip non-empty
        if((pItem2!=nullptr)!=merge)
            continue;

        if( pItem2 )
        {
            if(pItem2->GetEntry() == pSrcItem->GetEntry() && pItem2->GetCount() < pSrcItem->GetMaxStackCount() )
            {
                uint32 need_space = pSrcItem->GetMaxStackCount() - pItem2->GetCount();
                if(need_space > count)
                    need_space = count;

                GuildItemPosCount newPosition = GuildItemPosCount(j,need_space);
                if(!newPosition.isContainedIn(dest))
                {
                    dest.push_back(newPosition);
                    count -= need_space;

                    if(count==0)
                        return EQUIP_ERR_OK;
                }
            }
        }
        else
        {
            uint32 need_space = pSrcItem->GetMaxStackCount();
            if(need_space > count)
                need_space = count;

            GuildItemPosCount newPosition = GuildItemPosCount(j,need_space);
            if(!newPosition.isContainedIn(dest))
            {
                dest.push_back(newPosition);
                count -= need_space;

                if(count==0)
                    return EQUIP_ERR_OK;
            }
        }
    }
    return EQUIP_ERR_OK;
}

uint8 Guild::CanStoreItem( uint8 tab, uint8 slot, GuildItemPosCountVec &dest, uint32 count, Item *pItem, bool swap ) const
{
    if(count > pItem->GetCount())
        return EQUIP_ERR_COULDNT_SPLIT_ITEMS;

    if(pItem->IsSoulBound())
        return EQUIP_ERR_CANT_DROP_SOULBOUND;

    // in specific slot
    if( slot != NULL_SLOT )
    {
        uint8 res = _CanStoreItem_InSpecificSlot(tab,slot,dest,count,swap,pItem);
        if(res!=EQUIP_ERR_OK)
            return res;

        if(count==0)
            return EQUIP_ERR_OK;
    }

    // not specific slot or have spece for partly store only in specific slot

    // search stack in tab for merge to
    if( pItem->GetMaxStackCount() > 1 )
    {
        uint8 res = _CanStoreItem_InTab(tab,dest,count,true,pItem,slot);
        if(res!=EQUIP_ERR_OK)
            return res;

        if(count==0)
            return EQUIP_ERR_OK;
    }

    // search free slot in bag for place to
    uint8 res = _CanStoreItem_InTab(tab,dest,count,false,pItem,slot);
    if(res!=EQUIP_ERR_OK)
        return res;

    if(count==0)
        return EQUIP_ERR_OK;

    return EQUIP_ERR_BANK_FULL;
}

void Guild::SetGuildBankTabText(uint8 TabId, std::string text)
{
    if (TabId >= GUILD_BANK_MAX_TABS)
        return;
    if (TabId >= m_TabListMap.size())
        return;
    if (!m_TabListMap[TabId])
        return;

    if(m_TabListMap[TabId]->Text==text)
        return;

    utf8truncate(text,500);                                 // DB and client size limitation

    m_TabListMap[TabId]->Text = text;

    CharacterDatabase.EscapeString(text);
    CharacterDatabase.PExecute("UPDATE guild_bank_tab SET TabText='%s' WHERE guildid='%u' AND TabId='%u'", text.c_str(), Id, uint32(TabId));
    
    // announce
    SendGuildBankTabText(nullptr, TabId);
}

void Guild::SendGuildBankTabText(WorldSession *session, uint8 TabId)
{
    if (TabId > GUILD_BANK_MAX_TABS)
        return;

    GuildBankTab const *tab = GetBankTab(TabId);
    if (!tab)
        return;

    WorldPacket data(MSG_QUERY_GUILD_BANK_TEXT, 1+tab->Text.size()+1);
    data << uint8(TabId);
    data << tab->Text;
    if (session)
        session->SendPacket(&data);
    else
        BroadcastPacket(&data);
}

bool GuildItemPosCount::isContainedIn(GuildItemPosCountVec const &vec) const
{
    for(auto itr : vec)
        if(itr.slot == slot)
            return true;

    return false;
}

void Guild::SetName(std::string newName)
{
    if (newName.empty())
        return;

    name = newName;
}
