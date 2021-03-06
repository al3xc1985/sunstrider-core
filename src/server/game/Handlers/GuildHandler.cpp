
#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "Guild.h"
#include "MapManager.h"
#include "GossipDef.h"
#include "SocialMgr.h"
#include "LogsDatabaseAccessor.h"
#include "CharacterCache.h"

void WorldSession::HandleGuildQueryOpcode(WorldPacket& recvPacket)
{
    uint32 guildId;
    Guild *guild;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_QUERY");

    recvPacket >> guildId;

    guild = sObjectMgr->GetGuildById(guildId);
    if(!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    guild->Query(this);
}

void WorldSession::HandleGuildCreateOpcode(WorldPacket& recvPacket)
{
    //not used in game, charts are used

 /*   
    std::string gname;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_CREATE");

    recvPacket >> gname;

    if(GetPlayer()->GetGuildId())
        return;

    Guild *guild = new Guild;
    if(!guild->create(GetPlayer()->GetGUID(),gname))
    {
        delete guild;
        return;
    }

    sObjectMgr->AddGuild(guild); */
}

void WorldSession::HandleGuildInviteOpcode(WorldPacket& recvPacket)
{
    std::string Invitedname, plname;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_INVITE");

    Player * player = nullptr;

    recvPacket >> Invitedname;

    if(normalizePlayerName(Invitedname))
        player = ObjectAccessor::FindPlayerByName(Invitedname.c_str());

    if(!player)
    {
        SendGuildCommandResult(GUILD_INVITE_S, Invitedname, GUILD_PLAYER_NOT_FOUND);
        return;
    }

    Guild *guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
    if(!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    // OK result but not send invite
    if(player->GetSocial()->HasIgnore(GetPlayer()->GetGUID().GetCounter()))
        return;

    // not let enemies sign guild charter
    if (!sWorld->getConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GUILD) && player->GetTeam() != GetPlayer()->GetTeam())
    {
        SendGuildCommandResult(GUILD_INVITE_S, Invitedname, GUILD_NOT_ALLIED);
        return;
    }

    if(player->GetGuildId())
    {
        plname = player->GetName();
        SendGuildCommandResult(GUILD_INVITE_S, plname, ALREADY_IN_GUILD);
        return;
    }

    if(player->GetGuildIdInvited())
    {
        plname = player->GetName();
        SendGuildCommandResult(GUILD_INVITE_S, plname, ALREADY_INVITED_TO_GUILD);
        return;
    }

    if(!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_INVITE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_PERMISSIONS);
        return;
    }

    player->SetGuildIdInvited(GetPlayer()->GetGuildId());
    // Put record into guildlog
    guild->LogGuildEvent(GUILD_EVENT_LOG_INVITE_PLAYER, GetPlayer()->GetGUID().GetCounter(), player->GetGUID().GetCounter(), 0);

    WorldPacket data(SMSG_GUILD_INVITE, (8+10));            // guess size
    data << GetPlayer()->GetName();
    data << guild->GetName();
    player->SendDirectMessage(&data);

    //TC_LOG_DEBUG("network.opcode","WORLD: Sent (SMSG_GUILD_INVITE)");
}

void WorldSession::HandleGuildRemoveOpcode(WorldPacket& recvPacket)
{
    std::string plName;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_REMOVE");

    recvPacket >> plName;

    if(!normalizePlayerName(plName))
        return;

    Guild* guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
    if(!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if(!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_REMOVE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_PERMISSIONS);
        return;
    }

    ObjectGuid plGuid;
    MemberSlot* slot = guild->GetMemberSlot(plName, plGuid);
    if(!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if(slot->RankId == GR_GUILDMASTER)
    {
        SendGuildCommandResult(GUILD_QUIT_S, "", GUILD_LEADER_LEAVE);
        return;
    }

    
    if(GetPlayer()->GetRank() >= slot->RankId)
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_PERMISSIONS);
        return;
    }

    guild->DeleteMember(plGuid);
    // Put record into guildlog
    guild->LogGuildEvent(GUILD_EVENT_LOG_UNINVITE_PLAYER, GetPlayer()->GetGUID().GetCounter(), plGuid.GetCounter(), 0);

    WorldPacket data(SMSG_GUILD_EVENT, (2+20));             // guess size
    data << (uint8)GE_REMOVED;
    data << (uint8)2;                                       // strings count
    data << plName;
    data << GetPlayer()->GetName();
    guild->BroadcastPacket(&data);
}

void WorldSession::HandleGuildAcceptOpcode(WorldPacket& /*recvPacket*/)
{
    Guild *guild;
    Player *player = GetPlayer();

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_ACCEPT");

    guild = sObjectMgr->GetGuildById(player->GetGuildIdInvited());
    if(!guild || player->GetGuildId())
        return;

    // not let enemies sign guild charter
    if (!sWorld->getConfig(CONFIG_ALLOW_TWO_SIDE_INTERACTION_GUILD) && player->GetTeam() != sCharacterCache->GetCharacterTeamByGuid(guild->GetLeaderGUID()))
        return;

    SQLTransaction trans = CharacterDatabase.BeginTransaction();
    if(!guild->AddMember(GetPlayer()->GetGUID(),guild->GetLowestRank(), trans))
        return;
    CharacterDatabase.CommitTransaction(trans);
    // Put record into guildlog
    guild->LogGuildEvent(GUILD_EVENT_LOG_JOIN_GUILD, GetPlayer()->GetGUID().GetCounter(), 0, 0);

    WorldPacket data(SMSG_GUILD_EVENT, (2+10));             // guess size
    data << (uint8)GE_JOINED;
    data << (uint8)1;
    data << player->GetName();
    guild->BroadcastPacket(&data);

    //TC_LOG_DEBUG("network.opcode","WORLD: Sent (SMSG_GUILD_EVENT)");
}

void WorldSession::HandleGuildDeclineOpcode(WorldPacket& /*recvPacket*/)
{
    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_DECLINE");

    GetPlayer()->SetGuildIdInvited(0);
    GetPlayer()->SetInGuild(0);
}

void WorldSession::HandleGuildInfoOpcode(WorldPacket& /*recvPacket*/)
{
    Guild *guild;
    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_INFO");

    guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
    if(!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    WorldPacket data(SMSG_GUILD_INFO, (5*4 + guild->GetName().size() + 1));
    data << guild->GetName();
    data << guild->GetCreatedDay();
    data << guild->GetCreatedMonth();
    data << guild->GetCreatedYear();
    data << guild->GetMemberCount();
    data << guild->GetAccountsNumber();

    SendPacket(&data);
}

void WorldSession::HandleGuildRosterOpcode(WorldPacket& /*recvPacket*/)
{
    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_ROSTER");

    Guild* guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
    if(!guild)
        return;

    guild->Roster(this);
}

void WorldSession::HandleGuildPromoteOpcode(WorldPacket& recvPacket)
{
    std::string plName;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_PROMOTE");

    recvPacket >> plName;

    if(!normalizePlayerName(plName))
        return;

    Guild* guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
    if(!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if(!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_PROMOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_PERMISSIONS);
        return;
    }

    ObjectGuid plGuid;
    MemberSlot* slot = guild->GetMemberSlot(plName, plGuid);

    if(!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if(plGuid == GetPlayer()->GetGUID())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_NAME_INVALID);
        return;
    }

    if(slot->RankId < 2 || (slot->RankId-1) < GetPlayer()->GetRank())
        return;

    uint32 newRankId = slot->RankId < guild->GetNrRanks() ? slot->RankId-1 : guild->GetNrRanks()-1;

    guild->ChangeRank(plGuid, newRankId);
    // Put record into guildlog
    guild->LogGuildEvent(GUILD_EVENT_LOG_PROMOTE_PLAYER, GetPlayer()->GetGUID().GetCounter(), plGuid.GetCounter(), newRankId);

    WorldPacket data(SMSG_GUILD_EVENT, (2+30));             // guess size
    data << (uint8)GE_PROMOTION;
    data << (uint8)3;
    data << GetPlayer()->GetName();
    data << plName;
    data << guild->GetRankName(newRankId);
    guild->BroadcastPacket(&data);
}

void WorldSession::HandleGuildDemoteOpcode(WorldPacket& recvPacket)
{
    std::string plName;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_DEMOTE");

    recvPacket >> plName;

    if(!normalizePlayerName(plName))
        return;

    Guild* guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());

    if(!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if(!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_DEMOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_PERMISSIONS);
        return;
    }

    ObjectGuid plGuid;
    MemberSlot* slot = guild->GetMemberSlot(plName, plGuid);

    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if(plGuid == GetPlayer()->GetGUID())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_NAME_INVALID);
        return;
    }

    if((slot->RankId+1) >= guild->GetNrRanks() || slot->RankId <= GetPlayer()->GetRank())
        return;

    guild->ChangeRank(plGuid, (slot->RankId+1));
    // Put record into guildlog
    guild->LogGuildEvent(GUILD_EVENT_LOG_DEMOTE_PLAYER, GetPlayer()->GetGUID().GetCounter(), plGuid.GetCounter(), (slot->RankId));

    WorldPacket data(SMSG_GUILD_EVENT, (2+30));             // guess size
    data << (uint8)GE_DEMOTION;
    data << (uint8)3;
    data << GetPlayer()->GetName();
    data << plName;
    data << guild->GetRankName(slot->RankId);
    guild->BroadcastPacket(&data);
}

void WorldSession::HandleGuildLeaveOpcode(WorldPacket& /*recvPacket*/)
{
    std::string plName;
    Guild *guild;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_LEAVE");

    guild = sObjectMgr->GetGuildById(_player->GetGuildId());
    if(!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if(_player->GetGUID() == guild->GetLeaderGUID() && guild->GetMemberCount() > 1)
    {
        SendGuildCommandResult(GUILD_QUIT_S, "", GUILD_LEADER_LEAVE);
        return;
    }

    if(_player->GetGUID() == guild->GetLeaderGUID())
    {
        guild->Disband();
        return;
    }

    plName = _player->GetName();

    guild->DeleteMember(_player->GetGUID());
    // Put record into guildlog
    guild->LogGuildEvent(GUILD_EVENT_LOG_LEAVE_GUILD, _player->GetGUID().GetCounter(), 0, 0);

    WorldPacket data(SMSG_GUILD_EVENT, (2+10));             // guess size
    data << (uint8)GE_LEFT;
    data << (uint8)1;
    data << plName;
    guild->BroadcastPacket(&data);

    //TC_LOG_DEBUG("network.opcode","WORLD: Sent (SMSG_GUILD_EVENT)");

    SendGuildCommandResult(GUILD_QUIT_S, guild->GetName(), GUILD_PLAYER_NO_MORE_IN_GUILD);
}

void WorldSession::HandleGuildDisbandOpcode(WorldPacket& /*recvPacket*/)
{
    std::string name;
    Guild *guild;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_DISBAND");

    guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
    if(!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if(GetPlayer()->GetGUID() != guild->GetLeaderGUID())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_PERMISSIONS);
        return;
    }

    guild->Disband();

    //TC_LOG_DEBUG("network.opcode","WORLD: Guild Sucefully Disbanded");
}

void WorldSession::HandleGuildLeaderOpcode(WorldPacket& recvPacket)
{
    std::string name;
    Player *oldLeader = GetPlayer();
    Guild *guild;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_LEADER");

    recvPacket >> name;

    if(!normalizePlayerName(name))
        return;

    guild = sObjectMgr->GetGuildById(oldLeader->GetGuildId());

    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if( oldLeader->GetGUID() != guild->GetLeaderGUID())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_PERMISSIONS);
        return;
    }

    ObjectGuid newLeaderGUID;
    MemberSlot* slot = guild->GetMemberSlot(name, newLeaderGUID);

    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, name, GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    guild->SetLeader(newLeaderGUID);
    guild->ChangeRank(oldLeader->GetGUID(), GR_OFFICER);

    WorldPacket data(SMSG_GUILD_EVENT, (2+20));             // guess size
    data << (uint8)GE_LEADER_CHANGED;
    data << (uint8)2;
    data << oldLeader->GetName();
    data << name.c_str();
    guild->BroadcastPacket(&data);

    //TC_LOG_DEBUG("network.opcode","WORLD: Sent (SMSG_GUILD_EVENT)");
}

void WorldSession::HandleGuildMOTDOpcode(WorldPacket& recvPacket)
{
    Guild *guild;
    std::string MOTD;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_MOTD");

    guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
    if(!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if(!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_SETMOTD))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_PERMISSIONS);
        return;
    }

    if(!recvPacket.empty())
        recvPacket >> MOTD;
    else
        MOTD = "";

    guild->SetMOTD(MOTD);

    WorldPacket data(SMSG_GUILD_EVENT, (2+MOTD.size()+1));
    data << (uint8)GE_MOTD;
    data << (uint8)1;
    data << MOTD;
    guild->BroadcastPacket(&data);

    //TC_LOG_DEBUG("network.opcode","WORLD: Sent (SMSG_GUILD_EVENT)");
}

void WorldSession::HandleGuildSetPublicNoteOpcode(WorldPacket& recvPacket)
{
    std::string name,PNOTE;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_SET_PUBLIC_NOTE");

    recvPacket >> name;

    if(!normalizePlayerName(name))
        return;

    Guild* guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_EPNOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_PERMISSIONS);
        return;
    }

    ObjectGuid plGuid;
    MemberSlot* slot = guild->GetMemberSlot(name, plGuid);

    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, name, GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    recvPacket >> PNOTE;
    guild->SetPNOTE(plGuid, PNOTE);

    guild->Roster(this);
}

void WorldSession::HandleGuildSetOfficerNoteOpcode(WorldPacket& recvPacket)
{
    std::string plName, OFFNOTE;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_SET_OFFICER_NOTE");

    recvPacket >> plName;

    if(!normalizePlayerName(plName))
        return;

    Guild* guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_EOFFNOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_PERMISSIONS);
        return;
    }

    ObjectGuid plGuid;
    MemberSlot* slot = guild->GetMemberSlot(plName, plGuid);

    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    recvPacket >> OFFNOTE;
    guild->SetOFFNOTE(plGuid, OFFNOTE);

    guild->Roster(this);
}

void WorldSession::HandleGuildRankOpcode(WorldPacket& recvPacket)
{
    //recvPacket.hexlike();

    Guild *guild;
    std::string rankname;
    uint32 rankId;
    uint32 rights, MoneyPerDay;
    uint32 BankRights;
    uint32 BankSlotPerDay;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_RANK");

    guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
    if(!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    else if(GetPlayer()->GetGUID() != guild->GetLeaderGUID())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_PERMISSIONS);
        return;
    }

    recvPacket >> rankId;
    recvPacket >> rights;
    recvPacket >> rankname;
    recvPacket >> MoneyPerDay;

    for (int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
    {
        recvPacket >> BankRights;
        recvPacket >> BankSlotPerDay;
        guild->SetBankRightsAndSlots(rankId, uint8(i), uint16(BankRights & 0xFF), uint16(BankSlotPerDay), true);
    }

    guild->SetBankMoneyPerDay(rankId, MoneyPerDay);
    guild->SetRankName(rankId, rankname);

    if(rankId==GR_GUILDMASTER)                              // prevent loss leader rights
        rights |= GR_RIGHT_ALL;

    guild->SetRankRights(rankId, rights);

    guild->Query(this);
    guild->Roster();
}

void WorldSession::HandleGuildAddRankOpcode(WorldPacket& recvPacket)
{
    Guild *guild;
    std::string rankname;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_ADD_RANK");

    guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
    if(!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if(GetPlayer()->GetGUID() != guild->GetLeaderGUID())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_PERMISSIONS);
        return;
    }

    if(guild->GetNrRanks() >= GUILD_MAX_RANKS_COUNT)              // client not let create more 10 than ranks
        return;

    recvPacket >> rankname;

    SQLTransaction trans = CharacterDatabase.BeginTransaction();
    guild->CreateRank(rankname, GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK, trans);
    CharacterDatabase.CommitTransaction(trans);

    guild->Query(this);
    guild->Roster();
}

void WorldSession::HandleGuildDelRankOpcode(WorldPacket& /*recvPacket*/)
{
    Guild *guild;
    std::string rankname;

    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_DEL_RANK");

    guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
    if(!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    else if(GetPlayer()->GetGUID() != guild->GetLeaderGUID())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", GUILD_PERMISSIONS);
        return;
    }

    guild->DelRank();

    guild->Query(this);
    guild->Roster();
}

void WorldSession::SendGuildCommandResult(uint32 typecmd, const std::string& str,uint32 cmdresult)
{
    WorldPacket data(SMSG_GUILD_COMMAND_RESULT, (8+str.size()+1));
    data << typecmd;
    data << str;
    data << cmdresult;
    SendPacket(&data);

    //TC_LOG_DEBUG("network.opcode","WORLD: Sent (SMSG_GUILD_COMMAND_RESULT)");
}

void WorldSession::HandleGuildChangeInfoTextOpcode(WorldPacket& recvPacket)
{
    //TC_LOG_DEBUG("network.opcode","WORLD: Received CMSG_GUILD_INFO_TEXT");

    std::string GINFO;

    recvPacket >> GINFO;

    Guild *guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
    if(!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if(!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_MODIFY_GUILD_INFO))
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", GUILD_PERMISSIONS);
        return;
    }

    guild->SetGINFO(GINFO);
}

void WorldSession::HandleSaveGuildEmblemOpcode(WorldPacket& recvPacket)
{
    //TC_LOG_DEBUG("network.opcode","WORLD: Received MSG_SAVE_GUILD_EMBLEM");

    ObjectGuid vendorGuid;

    uint32 EmblemStyle;
    uint32 EmblemColor;
    uint32 BorderStyle;
    uint32 BorderColor;
    uint32 BackgroundColor;

    recvPacket >> vendorGuid;

    Creature *pCreature = GetPlayer()->GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_TABARDDESIGNER);
    if (!pCreature)
    {
        //"That's not an emblem vendor!"
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_INVALIDVENDOR);
        TC_LOG_ERROR("FIXME","WORLD: HandleSaveGuildEmblemOpcode - Unit (GUID: %u) not found or you can't interact with him.", vendorGuid.GetCounter());
        return;
    }

    // remove fake death
    if(GetPlayer()->HasUnitState(UNIT_STATE_DIED))
        GetPlayer()->RemoveAurasByType(SPELL_AURA_FEIGN_DEATH);

    recvPacket >> EmblemStyle;
    recvPacket >> EmblemColor;
    recvPacket >> BorderStyle;
    recvPacket >> BorderColor;
    recvPacket >> BackgroundColor;

    Guild *guild = sObjectMgr->GetGuildById(GetPlayer()->GetGuildId());
    if(!guild)
    {
        //"You are not part of a guild!";
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOGUILD);
        return;
    }

    if (guild->GetLeaderGUID() != GetPlayer()->GetGUID())
    {
        //"Only guild leaders can create emblems."
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOTGUILDMASTER);
        return;
    }

    if(GetPlayer()->GetMoney() < 10*GOLD)
    {
        //"You can't afford to do that."
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOTENOUGHMONEY);
        return;
    }

    GetPlayer()->ModifyMoney(-10*int32(GOLD));
    guild->SetEmblem(EmblemStyle, EmblemColor, BorderStyle, BorderColor, BackgroundColor);

    //"Guild Emblem saved."
    SendSaveGuildEmblem(ERR_GUILDEMBLEM_SUCCESS);

    guild->Query(this);
}

void WorldSession::HandleGuildEventLogOpcode(WorldPacket& /* recvPacket */)
{
    uint32 GuildId = GetPlayer()->GetGuildId();
    if (GuildId == 0)
        return;

    Guild *pGuild = sObjectMgr->GetGuildById(GuildId);
    if(!pGuild)
        return;

    pGuild->DisplayGuildEventlog(this);
}

/******  GUILD BANK  *******/

void WorldSession::HandleGuildBankMoneyWithdrawn( WorldPacket & /* recvData */ )
{
    uint32 GuildId = GetPlayer()->GetGuildId();
    if (GuildId == 0)
        return;

    Guild *pGuild = sObjectMgr->GetGuildById(GuildId);
    if(!pGuild)
        return;

    pGuild->SendMoneyInfo(this, GetPlayer()->GetGUID().GetCounter());
}

void WorldSession::HandleGuildPermissions( WorldPacket& /* recvData */ )
{
    uint32 GuildId = GetPlayer()->GetGuildId();
    if (GuildId == 0)
        return;

    Guild *pGuild = sObjectMgr->GetGuildById(GuildId);
    if(!pGuild)
        return;

    uint32 rankId = GetPlayer()->GetRank();

    WorldPacket data(MSG_GUILD_PERMISSIONS, 4*15+1);
    data << uint32(rankId);                                 // guild rank id
    data << uint32(pGuild->GetRankRights(rankId));          // rank rights
                                                            // money per day left
    data << uint32(pGuild->GetMemberMoneyWithdrawRem(GetPlayer()->GetGUID().GetCounter()));
    data << uint8(pGuild->GetPurchasedTabs());              // tabs count
    for(int i = 0; i < GUILD_BANK_MAX_TABS; ++i)
    {
        data << uint32(pGuild->GetBankRights(rankId, uint8(i)));
        data << uint32(pGuild->GetMemberSlotWithdrawRem(GetPlayer()->GetGUID().GetCounter(), uint8(i)));
    }
    SendPacket(&data);
}

/* Called when clicking on Guild bank gameobject */
void WorldSession::HandleGuildBankerActivate( WorldPacket & recvData )
{
    ObjectGuid GoGuid;
    uint8  unk;
    recvData >> GoGuid >> unk;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(GoGuid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    if (uint32 GuildId = GetPlayer()->GetGuildId())
    {
        if(Guild *pGuild = sObjectMgr->GetGuildById(GuildId))
        {
            pGuild->DisplayGuildBankTabsInfo(this);
            return;
        }
    }

    SendGuildCommandResult(GUILD_BANK_S, "", GUILD_PLAYER_NOT_IN_GUILD);
}

/* Called when opening guild bank tab only (first one) */
void WorldSession::HandleGuildBankerActivateTab( WorldPacket & recvData )
{
    ObjectGuid GoGuid;
    uint8 TabId, unk1;
    recvData >> GoGuid >> TabId >> unk1;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(GoGuid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (GuildId == 0)
        return;

    Guild *pGuild = sObjectMgr->GetGuildById(GuildId);
    if(!pGuild)
        return;

    // Let's update the amount of gold the player can withdraw before displaying the content
    // This is usefull if money withdraw right has changed
    pGuild->SendMoneyInfo(this, GetPlayer()->GetGUID().GetCounter());

    pGuild->DisplayGuildBankContent(this, TabId);
}

void WorldSession::HandleGuildBankDepositMoney( WorldPacket & recvData )
{
    ObjectGuid GoGuid;
    uint32 money;
    recvData >> GoGuid >> money;

    if (!money)
        return;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(GoGuid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (GuildId == 0)
        return;

    Guild *pGuild = sObjectMgr->GetGuildById(GuildId);
    if(!pGuild)
        return;

    if (GetPlayer()->GetMoney() < money)
        return;

    SQLTransaction trans = CharacterDatabase.BeginTransaction();

    pGuild->SetBankMoney(pGuild->GetGuildBankMoney()+money, trans);
    GetPlayer()->ModifyMoney(-int(money));
    GetPlayer()->SaveGoldToDB(trans);

    CharacterDatabase.CommitTransaction(trans);

    // log
    pGuild->LogBankEvent(GUILD_BANK_LOG_DEPOSIT_MONEY, uint8(0), GetPlayer()->GetGUID().GetCounter(), money);

    pGuild->DisplayGuildBankTabsInfo(this);
    pGuild->DisplayGuildBankContent(this, 0);
    pGuild->DisplayGuildBankMoneyUpdate();

    LogsDatabaseAccessor::GuildMoneyTransfer(GetPlayer(), GuildId, money);
}

void WorldSession::HandleGuildBankWithdrawMoney( WorldPacket & recvData )
{
    ObjectGuid GoGuid;
    uint32 money;
    recvData >> GoGuid >> money;

    if (!money)
        return;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(GoGuid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (GuildId == 0)
        return;

    Guild *pGuild = sObjectMgr->GetGuildById(GuildId);
    if(!pGuild)
        return;

    if (pGuild->GetGuildBankMoney()<money)                  // not enough money in bank
        return;

    if (!pGuild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_WITHDRAW_GOLD))
        return;

    SQLTransaction trans = CharacterDatabase.BeginTransaction();

    if (!pGuild->HandleMemberWithdrawMoney(money, GetPlayer()->GetGUID().GetCounter(), trans))
    {
        //CharacterDatabase.RollbackTransaction();
        return;
    }

    GetPlayer()->ModifyMoney(money);
    GetPlayer()->SaveGoldToDB(trans);                       // contains money

    CharacterDatabase.CommitTransaction(trans);

    // Log
    pGuild->LogBankEvent(GUILD_BANK_LOG_WITHDRAW_MONEY, uint8(0), GetPlayer()->GetGUID().GetCounter(), money);

    pGuild->SendMoneyInfo(this, GetPlayer()->GetGUID().GetCounter());
    pGuild->DisplayGuildBankTabsInfo(this);
    pGuild->DisplayGuildBankContent(this, 0);
    pGuild->DisplayGuildBankMoneyUpdate();

    LogsDatabaseAccessor::GuildMoneyTransfer(GetPlayer(), GuildId, -int32(money));
}

void WorldSession::HandleGuildBankSwapItems( WorldPacket & recvData )
{
    ObjectGuid GoGuid;
    uint8 BankToBank;

    uint8 BankTab, BankTabSlot, AutoStore = 0, AutoStoreCount, PlayerSlot = 0, PlayerBag = 0, SplitedAmount = 0;
    uint8 BankTabDst = 0, BankTabSlotDst = 0, unk2, ToChar = 1;
    uint32 ItemEntry, unk1;

    
    recvData >> GoGuid >> BankToBank;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(GoGuid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    if (BankToBank)
    {
        recvData >> BankTabDst;
        recvData >> BankTabSlotDst;
        recvData >> unk1;                                  // always 0
        recvData >> BankTab;
        recvData >> BankTabSlot;
        recvData >> ItemEntry;
        recvData >> unk2;                                  // always 0
        recvData >> SplitedAmount;

        if (BankTabSlotDst >= GUILD_BANK_MAX_SLOTS)
            return;
        if (BankTabDst == BankTab && BankTabSlotDst == BankTabSlot)
            return;
    }
    else
    {
        recvData >> BankTab;
        recvData >> BankTabSlot;
        recvData >> ItemEntry;
        recvData >> AutoStore;
        if (AutoStore)
        {
            recvData >> AutoStoreCount;
        }
        recvData >> PlayerBag;
        recvData >> PlayerSlot;
        if (!AutoStore)
        {
            recvData >> ToChar;
            recvData >> SplitedAmount;
        }

        if (BankTabSlot >= GUILD_BANK_MAX_SLOTS && BankTabSlot != 0xFF)
            return;
    }

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (GuildId == 0)
        return;

    Guild *pGuild = sObjectMgr->GetGuildById(GuildId);
    if(!pGuild)
        return;

    Player *pl = GetPlayer();

    // player->bank or bank->bank check if tab is correct to prevent crash
    if(!ToChar && !pGuild->GetBankTab(BankTab))
        return;

    // Bank <-> Bank
    if (BankToBank)
    {
        // empty operation
        if(BankTab == BankTabDst && BankTabSlot==BankTabSlotDst)
            return;

        Item *pItemSrc = pGuild->GetItem(BankTab, BankTabSlot);
        if (!pItemSrc)                                      // may prevent crash
            return;

        if(SplitedAmount > pItemSrc->GetCount())
            return;                                         // cheating?
        else if(SplitedAmount == pItemSrc->GetCount())
            SplitedAmount = 0;                              // no split

        Item *pItemDst = pGuild->GetItem(BankTabDst, BankTabSlotDst);

        if(BankTab!=BankTabDst)
        {
            // check dest pos rights (if different tabs)
            if(!pGuild->IsMemberHaveRights(pl->GetGUID().GetCounter(), BankTabDst, GUILD_BANK_RIGHT_DEPOSIT_ITEM))
                return;

            // check source pos rights (if different tabs)
            uint32 remRight = pGuild->GetMemberSlotWithdrawRem(pl->GetGUID().GetCounter(), BankTab);
            if(remRight <= 0)
                return;
        }

        if (SplitedAmount)
        {                                                   // Bank -> Bank item split (in empty or non empty slot
            GuildItemPosCountVec dest;
            uint8 msg = pGuild->CanStoreItem(BankTabDst,BankTabSlotDst,dest,SplitedAmount,pItemSrc,false);
            if( msg != EQUIP_ERR_OK )
            {
                pl->SendEquipError( msg, pItemSrc, nullptr );
                return;
            }

            Item *pNewItem = pItemSrc->CloneItem( SplitedAmount );
            if( !pNewItem )
            {
                pl->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, pItemSrc, nullptr );
                return;
            }

            SQLTransaction trans = CharacterDatabase.BeginTransaction();
            pGuild->LogBankEvent(GUILD_BANK_LOG_MOVE_ITEM, BankTab, pl->GetGUID().GetCounter(), pItemSrc->GetEntry(), SplitedAmount, BankTabDst);

            pl->ItemRemovedQuestCheck( pItemSrc->GetEntry(), SplitedAmount );
            pItemSrc->SetCount( pItemSrc->GetCount() - SplitedAmount );
            pItemSrc->FSetState(ITEM_CHANGED);
            pItemSrc->SaveToDB(trans);                           // not in inventory and can be save standalone
            pGuild->StoreItem(BankTabDst,dest,pNewItem, trans);
            CharacterDatabase.CommitTransaction(trans);
        }
        else                                                // non split
        {
            GuildItemPosCountVec gDest;
            uint8 msg = pGuild->CanStoreItem(BankTabDst,BankTabSlotDst,gDest,pItemSrc->GetCount(),pItemSrc,false);
            if( msg == EQUIP_ERR_OK )                       // merge to
            {
                // TODO: SQL Transaction
                SQLTransaction trans = CharacterDatabase.BeginTransaction();
                pGuild->LogBankEvent(GUILD_BANK_LOG_MOVE_ITEM, BankTab,    pl->GetGUID().GetCounter(), pItemSrc->GetEntry(), pItemSrc->GetCount(), BankTabDst);

                pGuild->RemoveItem(BankTab, BankTabSlot, trans);
                pGuild->StoreItem(BankTabDst, gDest, pItemSrc, trans);
                CharacterDatabase.CommitTransaction(trans);
            }
            else                                            // swap
            {
                gDest.clear();
                uint8 _msg = pGuild->CanStoreItem(BankTabDst,BankTabSlotDst,gDest,pItemSrc->GetCount(),pItemSrc,true);
                if(_msg != EQUIP_ERR_OK )
                {
                    pl->SendEquipError( msg, pItemSrc, nullptr );
                    return;
                }

                GuildItemPosCountVec gSrc;
                msg = pGuild->CanStoreItem(BankTab,BankTabSlot,gSrc,pItemDst->GetCount(),pItemDst,true);
                if( msg != EQUIP_ERR_OK )
                {
                    pl->SendEquipError( msg, pItemDst, nullptr );
                    return;
                }

                if(BankTab!=BankTabDst)
                {
                    // check source pos rights (item swapped to src)
                    if(!pGuild->IsMemberHaveRights(pl->GetGUID().GetCounter(), BankTab, GUILD_BANK_RIGHT_DEPOSIT_ITEM))
                        return;

                    // check dest pos rights (item swapped to src)
                    uint32 remRightDst = pGuild->GetMemberSlotWithdrawRem(pl->GetGUID().GetCounter(), BankTabDst);
                    if(remRightDst <= 0)
                        return;
                }

                SQLTransaction trans = CharacterDatabase.BeginTransaction();
                pGuild->LogBankEvent(GUILD_BANK_LOG_MOVE_ITEM, BankTab,    pl->GetGUID().GetCounter(), pItemSrc->GetEntry(), pItemSrc->GetCount(), BankTabDst);
                pGuild->LogBankEvent(GUILD_BANK_LOG_MOVE_ITEM, BankTabDst, pl->GetGUID().GetCounter(), pItemDst->GetEntry(), pItemDst->GetCount(), BankTab);

                pGuild->RemoveItem(BankTab, BankTabSlot, trans);
                pGuild->RemoveItem(BankTabDst, BankTabSlotDst, trans);
                pGuild->StoreItem(BankTab, gSrc, pItemDst, trans);
                pGuild->StoreItem(BankTabDst, gDest, pItemSrc, trans);
                CharacterDatabase.CommitTransaction(trans);
            }
        }
        pGuild->DisplayGuildBankContentUpdate(BankTab,BankTabSlot,BankTab==BankTabDst ? BankTabSlotDst : -1);
        if(BankTab!=BankTabDst)
            pGuild->DisplayGuildBankContentUpdate(BankTabDst,BankTabSlotDst);
        return;
    }

    // Player <-> Bank

    // char->bank autostore click return BankTabSlot = 255 = NULL_SLOT
    // do similar for bank->char
    if(AutoStore && ToChar)
    {
        PlayerBag = NULL_BAG;
        PlayerSlot = NULL_SLOT;
    }

    // allow work with inventory only
    if(!Player::IsInventoryPos(PlayerBag,PlayerSlot) && !(PlayerBag == NULL_BAG && PlayerSlot == NULL_SLOT) )
    {
        _player->SendEquipError( EQUIP_ERR_NONE, nullptr, nullptr );
        return;
    }

    Item *pItemBank = pGuild->GetItem(BankTab, BankTabSlot);
    Item *pItemChar = GetPlayer()->GetItemByPos(PlayerBag, PlayerSlot);
    if (!pItemChar && !pItemBank)                           // Nothing to do
        return;

    if (!pItemChar && !ToChar)                              // Problem to get item from player
        return;

    if (!pItemBank && ToChar)                               // Problem to get bank item
        return;

    // BankToChar swap or char to bank remaining

    if (ToChar)                                             // Bank -> Char cases
    {
        if(SplitedAmount > pItemBank->GetCount())
            return;                                         // cheating?
        else if(SplitedAmount == pItemBank->GetCount())
            SplitedAmount = 0;                              // no split

        if (SplitedAmount)
        {                                                   // Bank -> Char split to slot (patly move)
            Item *pNewItem = pItemBank->CloneItem( SplitedAmount );
            if( !pNewItem )
            {
                pl->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, pItemBank, nullptr );
                return;
            }

            ItemPosCountVec dest;
            uint8 msg = pl->CanStoreItem(PlayerBag, PlayerSlot, dest, pNewItem, false);
            if( msg != EQUIP_ERR_OK )
            {
                pl->SendEquipError( msg, pNewItem, nullptr );
                delete pNewItem;
                return;
            }

            // check source pos rights (item moved to inventory)
            uint32 remRight = pGuild->GetMemberSlotWithdrawRem(pl->GetGUID().GetCounter(), BankTab);
            if(remRight <= 0)
            {
                delete pNewItem;
                return;
            }

            SQLTransaction trans = CharacterDatabase.BeginTransaction();
            pGuild->LogBankEvent(GUILD_BANK_LOG_WITHDRAW_ITEM, BankTab, pl->GetGUID().GetCounter(), pItemBank->GetEntry(), SplitedAmount);

            pItemBank->SetCount(pItemBank->GetCount()-SplitedAmount);
            pItemBank->FSetState(ITEM_CHANGED);
            pItemBank->SaveToDB(trans);                          // not in inventory and can be save standalone
            pl->MoveItemToInventory(dest,pNewItem,true);
            pl->SaveInventoryAndGoldToDB(trans);

            pGuild->MemberItemWithdraw(BankTab, pl->GetGUID().GetCounter());
            CharacterDatabase.CommitTransaction(trans);

            LogsDatabaseAccessor::GuildBankItemTransfer(pl, false, pNewItem->GetGUID(), pNewItem->GetEntry(), pNewItem->GetCount());
        }
        else                                                // Bank -> Char swap with slot (move)
        {
            ItemPosCountVec dest;
            uint8 msg = pl->CanStoreItem(PlayerBag, PlayerSlot, dest, pItemBank, false);
            if( msg == EQUIP_ERR_OK )                       // merge case
            {
                // check source pos rights (item moved to inventory)
                uint32 remRight = pGuild->GetMemberSlotWithdrawRem(pl->GetGUID().GetCounter(), BankTab);
                if(remRight <= 0)
                    return;

                SQLTransaction trans = CharacterDatabase.BeginTransaction();
                pGuild->LogBankEvent(GUILD_BANK_LOG_WITHDRAW_ITEM, BankTab, pl->GetGUID().GetCounter(), pItemBank->GetEntry(), pItemBank->GetCount());

                pGuild->RemoveItem(BankTab, BankTabSlot, trans);
                pl->MoveItemToInventory(dest,pItemBank,true);
                pl->SaveInventoryAndGoldToDB(trans);

                pGuild->MemberItemWithdraw(BankTab, pl->GetGUID().GetCounter());
                CharacterDatabase.CommitTransaction(trans);

                LogsDatabaseAccessor::GuildBankItemTransfer(pl, false, pItemBank->GetGUID(), pItemBank->GetEntry(), pItemBank->GetCount());
            }
            else                                            // Bank <-> Char swap items
            {
                // check source pos rights (item swapped to bank)
                if(!pGuild->IsMemberHaveRights(pl->GetGUID().GetCounter(), BankTab, GUILD_BANK_RIGHT_DEPOSIT_ITEM))
                    return;

                if(pItemChar)
                {
                    if(!pItemChar->CanBeTraded())
                    {
                        _player->SendEquipError( EQUIP_ERR_ITEMS_CANT_BE_SWAPPED, pItemChar, nullptr );
                        return;
                    }
                }

                ItemPosCountVec iDest;
                msg = pl->CanStoreItem(PlayerBag, PlayerSlot, iDest, pItemBank, true);
                if( msg != EQUIP_ERR_OK )
                {
                    pl->SendEquipError( msg, pItemBank, nullptr );
                    return;
                }

                GuildItemPosCountVec gDest;
                if(pItemChar)
                {
                    msg = pGuild->CanStoreItem(BankTab,BankTabSlot,gDest,pItemChar->GetCount(),pItemChar,true);
                    if( msg != EQUIP_ERR_OK )
                    {
                        pl->SendEquipError( msg, pItemChar, nullptr );
                        return;
                    }
                }

                // check source pos rights (item moved to inventory)
                uint32 remRight = pGuild->GetMemberSlotWithdrawRem(pl->GetGUID().GetCounter(), BankTab);
                if(remRight <= 0)
                    return;

                pGuild->LogBankEvent(GUILD_BANK_LOG_WITHDRAW_ITEM, BankTab, pl->GetGUID().GetCounter(), pItemBank->GetEntry(), pItemBank->GetCount());
                LogsDatabaseAccessor::GuildBankItemTransfer(pl, false, pItemBank->GetGUID(), pItemBank->GetEntry(), pItemBank->GetCount());
                if (pItemChar)
                {
                    pGuild->LogBankEvent(GUILD_BANK_LOG_DEPOSIT_ITEM, BankTab, pl->GetGUID().GetCounter(), pItemChar->GetEntry(), pItemChar->GetCount());
                    LogsDatabaseAccessor::GuildBankItemTransfer(pl, true, pItemChar->GetGUID(), pItemChar->GetEntry(), pItemChar->GetCount());
                }


                SQLTransaction trans = CharacterDatabase.BeginTransaction();
                pGuild->RemoveItem(BankTab, BankTabSlot, trans);
                if(pItemChar)
                {
                    pl->MoveItemFromInventory(PlayerBag, PlayerSlot, true);
                    pItemChar->DeleteFromInventoryDB(trans);
                }

                if(pItemChar)
                    pGuild->StoreItem(BankTab, gDest, pItemChar, trans);
                pl->MoveItemToInventory(iDest,pItemBank,true);
                pl->SaveInventoryAndGoldToDB(trans);

                pGuild->MemberItemWithdraw(BankTab, pl->GetGUID().GetCounter());
                CharacterDatabase.CommitTransaction(trans);

            }
        }
        pGuild->DisplayGuildBankContentUpdate(BankTab,BankTabSlot);
        return;
    }                                                       // End "To char" part

    // Char -> Bank cases

    if(!pItemChar->CanBeTraded())
    {
        _player->SendEquipError( EQUIP_ERR_ITEMS_CANT_BE_SWAPPED, pItemChar, nullptr );
        return;
    }

    // check source pos rights (item moved to bank)
    if(!pGuild->IsMemberHaveRights(pl->GetGUID().GetCounter(), BankTab, GUILD_BANK_RIGHT_DEPOSIT_ITEM))
        return;

    if(SplitedAmount > pItemChar->GetCount())
        return;                                             // cheating?
    else if(SplitedAmount == pItemChar->GetCount())
        SplitedAmount = 0;                                  // no split

    if (SplitedAmount)
    {                                                       // Char -> Bank split to empty or non-empty slot (partly move)
        GuildItemPosCountVec dest;
        uint8 msg = pGuild->CanStoreItem(BankTab,BankTabSlot,dest,SplitedAmount,pItemChar,false);
        if( msg != EQUIP_ERR_OK )
        {
            pl->SendEquipError( msg, pItemChar, nullptr );
            return;
        }

        Item *pNewItem = pItemChar->CloneItem( SplitedAmount );
        if( !pNewItem )
        {
            pl->SendEquipError( EQUIP_ERR_ITEM_NOT_FOUND, pItemChar, nullptr );
            return;
        }
       
        SQLTransaction trans = CharacterDatabase.BeginTransaction();
        pGuild->LogBankEvent(GUILD_BANK_LOG_DEPOSIT_ITEM, BankTab, pl->GetGUID().GetCounter(), pItemChar->GetEntry(), SplitedAmount);
        LogsDatabaseAccessor::GuildBankItemTransfer(pl, true, pItemChar->GetGUID(), pItemChar->GetEntry(), SplitedAmount);

        pl->ItemRemovedQuestCheck( pItemChar->GetEntry(), SplitedAmount );
        pItemChar->SetCount(pItemChar->GetCount()-SplitedAmount);
        pItemChar->SetState(ITEM_CHANGED);
        pl->SaveInventoryAndGoldToDB(trans);
        pGuild->StoreItem(BankTab, dest, pNewItem, trans);
        CharacterDatabase.CommitTransaction(trans);

        pGuild->DisplayGuildBankContentUpdate(BankTab,dest);

    }
    else                                                    // Char -> Bank swap with empty or non-empty (move)
    {
        GuildItemPosCountVec dest;
        uint8 msg = pGuild->CanStoreItem(BankTab,BankTabSlot,dest,pItemChar->GetCount(),pItemChar,false);
        if( msg == EQUIP_ERR_OK )                           // merge
        {
            pGuild->LogBankEvent(GUILD_BANK_LOG_DEPOSIT_ITEM, BankTab, pl->GetGUID().GetCounter(), pItemChar->GetEntry(), pItemChar->GetCount());
            LogsDatabaseAccessor::GuildBankItemTransfer(pl, true, pItemChar->GetGUID(), pItemChar->GetEntry(), pItemChar->GetCount());

            SQLTransaction trans = CharacterDatabase.BeginTransaction();

            pl->MoveItemFromInventory(PlayerBag, PlayerSlot, true);
            pItemChar->DeleteFromInventoryDB(trans);

            pGuild->StoreItem(BankTab,dest,pItemChar, trans);
            pl->SaveInventoryAndGoldToDB(trans);
            CharacterDatabase.CommitTransaction(trans);

            pGuild->DisplayGuildBankContentUpdate(BankTab,dest);

        }
        else                                                // Char <-> Bank swap items (posible NULL bank item)
        {
            ItemPosCountVec iDest;
            if(pItemBank)
            {
                msg = pl->CanStoreItem(PlayerBag, PlayerSlot, iDest, pItemBank, true);
                if( msg != EQUIP_ERR_OK )
                {
                    pl->SendEquipError( msg, pItemBank, nullptr );
                    return;
                }
            }

            GuildItemPosCountVec gDest;
            msg = pGuild->CanStoreItem(BankTab,BankTabSlot,gDest,pItemChar->GetCount(),pItemChar,true);
            if( msg != EQUIP_ERR_OK )
            {
                pl->SendEquipError( msg, pItemChar, nullptr );
                return;
            }

            if(pItemBank)
            {
                // check bank pos rights (item swapped with inventory)
                uint32 remRight = pGuild->GetMemberSlotWithdrawRem(pl->GetGUID().GetCounter(), BankTab);
                if(remRight <= 0)
                    return;
            }

            SQLTransaction trans = CharacterDatabase.BeginTransaction();
            if (pItemBank)
            {
                pGuild->LogBankEvent(GUILD_BANK_LOG_WITHDRAW_ITEM, BankTab, pl->GetGUID().GetCounter(), pItemBank->GetEntry(), pItemBank->GetCount());
                LogsDatabaseAccessor::GuildBankItemTransfer(pl, false, pItemBank->GetGUID(), pItemBank->GetEntry(), pItemBank->GetCount());
            }

            pGuild->LogBankEvent(GUILD_BANK_LOG_DEPOSIT_ITEM, BankTab, pl->GetGUID().GetCounter(), pItemChar->GetEntry(), pItemChar->GetCount());
            LogsDatabaseAccessor::GuildBankItemTransfer(pl, true, pItemChar->GetGUID(), pItemChar->GetEntry(), pItemChar->GetCount());

            pl->MoveItemFromInventory(PlayerBag, PlayerSlot, true);
            pItemChar->DeleteFromInventoryDB(trans);
            if(pItemBank)
                pGuild->RemoveItem(BankTab, BankTabSlot, trans);

            pGuild->StoreItem(BankTab,gDest,pItemChar, trans);
            if(pItemBank)
                pl->MoveItemToInventory(iDest,pItemBank,true);
            pl->SaveInventoryAndGoldToDB(trans);
            if (pItemBank)
            {
                pGuild->MemberItemWithdraw(BankTab, pl->GetGUID().GetCounter());
            }
            CharacterDatabase.CommitTransaction(trans);

            pGuild->DisplayGuildBankContentUpdate(BankTab,gDest);
        }
    }
}

void WorldSession::HandleGuildBankBuyTab( WorldPacket & recvData )
{
    ObjectGuid GoGuid;
    uint8 TabId;

    recvData >> GoGuid;
    recvData >> TabId;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(GoGuid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (GuildId==0)
        return;

    Guild *pGuild = sObjectMgr->GetGuildById(GuildId);
    if(!pGuild)
        return;

    uint32 TabCost = sObjectMgr->GetGuildBankTabPrice(TabId) * GOLD;
    if (!TabCost)
        return;

    if (pGuild->GetPurchasedTabs() >= GUILD_BANK_MAX_TABS)
        return;

    if (TabId != pGuild->GetPurchasedTabs())                // purchased_tabs = 0 when buying Tab 0, that is why this check can be made
    {
        TC_LOG_ERROR("FIXME","Error: trying to buy a tab non contigous to owned ones");
        return;
    }

    if (GetPlayer()->GetMoney() < TabCost)                  // Should not happen, this is checked by client
        return;

    // Go on with creating tab
    pGuild->CreateNewBankTab();
    GetPlayer()->ModifyMoney(-int(TabCost));
    pGuild->SetBankMoneyPerDay(GetPlayer()->GetRank(), WITHDRAW_MONEY_UNLIMITED);
    pGuild->SetBankRightsAndSlots(GetPlayer()->GetRank(), TabId, GUILD_BANK_RIGHT_FULL, WITHDRAW_SLOT_UNLIMITED, true);
    pGuild->Roster();
    pGuild->DisplayGuildBankTabsInfo(this);
}

void WorldSession::HandleGuildBankUpdateTab( WorldPacket & recvData )
{
    ObjectGuid GoGuid;
    uint8 TabId;
    std::string Name;
    std::string IconIndex;

    recvData >> GoGuid;
    recvData >> TabId;
    recvData >> Name;
    recvData >> IconIndex;

    if(Name.empty())
        return;

    if(IconIndex.empty())
        return;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(GoGuid, GAMEOBJECT_TYPE_GUILD_BANK))
        return;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (GuildId==0)
        return;

    Guild *pGuild = sObjectMgr->GetGuildById(GuildId);
    if(!pGuild)
        return;

    pGuild->SetGuildBankTabInfo(TabId, Name, IconIndex);
    pGuild->DisplayGuildBankTabsInfo(this);
    pGuild->DisplayGuildBankContent(this, TabId);
}

void WorldSession::HandleGuildBankLogQuery( WorldPacket & recvData )
{
    
    
    

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (GuildId == 0)
        return;

    Guild *pGuild = sObjectMgr->GetGuildById(GuildId);
    if(!pGuild)
        return;

    uint8 TabId;
    recvData >> TabId;

    pGuild->DisplayGuildBankLogs(this, TabId);
}

void WorldSession::HandleQueryGuildBankTabText(WorldPacket &recvData)
{
    
    
    

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (GuildId == 0)
        return;

    Guild *pGuild = sObjectMgr->GetGuildById(GuildId);
    if(!pGuild)
        return;

    uint8 TabId;
    recvData >> TabId;

    pGuild->SendGuildBankTabText(this, TabId);
}

void WorldSession::HandleGuildBankSetTabText(WorldPacket &recvData)
{
    
    
    

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (GuildId == 0)
        return;

    Guild *pGuild = sObjectMgr->GetGuildById(GuildId);
    if(!pGuild)
        return;

    uint8 TabId;
    std::string Text;
    recvData >> TabId;
    recvData >> Text;

    pGuild->SetGuildBankTabText(TabId, Text);
}

void WorldSession::SendSaveGuildEmblem( uint32 msg )
{
    WorldPacket data(MSG_SAVE_GUILD_EMBLEM, 4);
    data << uint32(msg);                                    // not part of guild
    SendPacket( &data );
}

