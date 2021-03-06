#include "Chat.h"
#include "Language.h"

bool ChatHandler::HandleGroupLeaderCommand(const char* args)
{
    Player* plr  = nullptr;
    Group* group = nullptr;
    ObjectGuid guid;
    char* cname  = strtok((char*)args, " ");

    if(GetPlayerGroupAndGUIDByName(cname, plr, group, guid))
        if(group && group->GetLeaderGUID() != guid)
            group->ChangeLeader(guid);

    return true;
}

bool ChatHandler::HandleGroupDisbandCommand(const char* args)
{
    Player* plr  = nullptr;
    Group* group = nullptr;
    ObjectGuid guid;
    char* cname  = strtok((char*)args, " ");

    if(GetPlayerGroupAndGUIDByName(cname, plr, group, guid))
        if(group)
            group->Disband();

    return true;
}

bool ChatHandler::HandleGroupRemoveCommand(const char* args)
{
    Player* plr  = nullptr;
    Group* group = nullptr;
    ObjectGuid guid;
    char* cname  = strtok((char*)args, " ");

    if(GetPlayerGroupAndGUIDByName(cname, plr, group, guid, true))
        if(group)
            group->RemoveMember(guid);

    return true;
}
