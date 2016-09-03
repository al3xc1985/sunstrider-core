/*
* Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
* Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __WORLDSOCKET_H__
#define __WORLDSOCKET_H__

#include "AuthCrypt.h"
#include "Socket.h"
#include "WorldPacket.h"
#include "MPSCQueue.h"
#include <chrono>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/buffer.hpp>

using boost::asio::ip::tcp;
class EncryptablePacket;

class WorldSession;

#pragma pack(push, 1)

struct ClientPktHeader
{
    uint16 size;
    uint32 cmd;

    bool IsValidSize() const { return size >= 4 && size < 10240; }
    bool IsValidOpcode() const { return cmd < NUM_OPCODE_HANDLERS; }
};

#pragma pack(pop)

struct AuthSession;

class WorldSocket : public Socket<WorldSocket>
{
    typedef Socket<WorldSocket> BaseSocket;

public:
    WorldSocket(tcp::socket&& socket);
    ~WorldSocket();

    WorldSocket(WorldSocket const& right) = delete;
    WorldSocket& operator=(WorldSocket const& right) = delete;

    void Start() override;
    bool Update() override;

    void SendPacket(WorldPacket const& packet);

    // see _lastPacketsSent. Use _lastPacketsSent_mutex while using it
    std::list<WorldPacket> const& GetLastPacketsSent();
    boost::shared_mutex& GetLastPacketsSentMutex()
    {
        return _lastPacketsSent_mutex;
    }

    void ClearLastPacketsSent();
protected:
    void OnClose() override;
    void ReadHandler() override;
    bool ReadHeaderHandler();

    enum class ReadDataHandlerResult
    {
        Ok = 0,
        Error = 1,
        WaitingForQuery = 2
    };

    ReadDataHandlerResult ReadDataHandler();

private:
    void CheckIpCallback(PreparedQueryResult result);

    /// writes network.opcode log
    /// accessing WorldSession is not threadsafe, only do it when holding _worldSessionLock
    void LogOpcodeText(OpcodeClient opcode, std::unique_lock<std::mutex> const& guard) const;
    /// sends and logs network.opcode without accessing WorldSession
    void SendPacketAndLogOpcode(WorldPacket const& packet);
    void HandleSendAuthSession(ClientBuild build);
    void HandleAuthSession(WorldPacket& recvPacket);
    void HandleAuthSessionCallback(std::shared_ptr<AuthSession> authSession, PreparedQueryResult result);
    void LoadSessionPermissionsCallback(PreparedQueryResult result);
    void SendAuthResponseError(uint8 code);

    bool HandlePing(WorldPacket& recvPacket);

    uint32 _authSeed;
    AuthCrypt* _authCrypt; //nullptr until initialized

    std::chrono::steady_clock::time_point _LastPingTime;
    uint32 _OverSpeedPings;

    std::mutex _worldSessionLock;
    WorldSession* _worldSession;
    bool _authed;

    MessageBuffer _headerBuffer;
    MessageBuffer _packetBuffer;
    MPSCQueue<EncryptablePacket> _bufferQueue;

    PreparedQueryResultFuture _queryFuture;
    std::function<void(PreparedQueryResult&&)> _queryCallback;
    std::string _ipCountry;

    /* This can be used for debug purpose when clients are experiencing crashes, this contains the last packets sent to it
    after the last client response. CONFIG_DEBUG_LOG_LAST_PACKETS must be enabled for this to be used.
    */
    std::list<WorldPacket> _lastPacketsSent;
    boost::shared_mutex _lastPacketsSent_mutex;
};

#endif