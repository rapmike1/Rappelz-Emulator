/*
  *  Copyright (C) 2017-2017 Xijezu <http://xijezu.com/>
  *
  *  This program is free software; you can redistribute it and/or modify it
  *  under the terms of the GNU General Public License as published by the
  *  Free Software Foundation; either version 3 of the License, or (at your
  *  option) any later version.
  *
  *  This program is distributed in the hope that it will be useful, but WITHOUT
  *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
  *  more details.
  *
  *  You should have received a copy of the GNU General Public License along
  *  with this program. If not, see <http://www.gnu.org/licenses/>.
  */

#include "GameList.h"
#include "PlayerList.h"
#include "WorldSocket.h"
#include "XPacket.h"

#include "AuthGame/AuthGameSession.h"

// Constructor - set the default server name to <null>, also give it a socket
AuthGameSession::AuthGameSession(GameSocket *pSocket) : m_pSocket(pSocket), m_pGame(new Game{}), m_bIsAuthed(false)
{
    if(pSocket)
    {
        pSocket->AddReference();
    }
    m_pGame->nIDX = 255;
    m_pGame->szName = "<null>";
}

AuthGameSession::~AuthGameSession()
{
    if(m_pGame)
    {
        if(m_bIsAuthed)
        {
            auto g = sGameMapList->GetGame(m_pGame->nIDX);
            if (g != nullptr)
            {
                sGameMapList->RemoveGame(g->nIDX);

            }
        }
        delete m_pGame;
        m_pGame = nullptr;
    }
    if(m_pSocket)
    {
        m_pSocket->RemoveReference();
    }
}


void AuthGameSession::OnClose()
{
    if(m_pGame == nullptr)
        return;
    auto g = sGameMapList->GetGame(m_pGame->nIDX);
    if(g != nullptr && g->szName == m_pGame->szName)
    {
        {
            MX_UNIQUE_GUARD writeGuard(*sPlayerMapList->GetGuard());
            auto map = sPlayerMapList->GetMap();
            for(auto& player : *map)
            {
                if(player.second->nGameIDX == g->nIDX)
                {
                    map->erase(player.second->szLoginName);
                    delete player.second;
                }
            }
        }
        sGameMapList->RemoveGame(g->nIDX);
        MX_LOG_INFO("gameserver", "Gameserver <%s> [Idx: %d] has disconnected.", m_pGame->szName.c_str(), m_pGame->nIDX);
    }
}

enum eStatus {
    STATUS_CONNECTED = 0,
    STATUS_AUTHED
};

typedef struct GameHandler {
    uint16_t cmd;
    uint8_t status;

    void (AuthGameSession::*handler)(XPacket *);
} AuthHandler;


const AuthHandler packetHandler[] =
        {
                {TS_GA_LOGIN,              STATUS_CONNECTED, &AuthGameSession::HandleGameLogin},
                {TS_GA_CLIENT_LOGIN,       STATUS_AUTHED,    &AuthGameSession::HandleClientLogin},
                {TS_GA_CLIENT_LOGOUT,      STATUS_AUTHED,    &AuthGameSession::HandleClientLogout},
                {TS_GA_CLIENT_KICK_FAILED, STATUS_AUTHED,    &AuthGameSession::HandleClientKickFailed}
        };

const int tableSize = (sizeof(packetHandler) / sizeof(GameHandler));

// Handler for incoming packets
void AuthGameSession::ProcessIncoming(XPacket *pGamePct)
{
    ASSERT(pGamePct);

    auto _cmd = pGamePct->GetPacketID();
    int i = 0;

    for (i = 0; i < tableSize; i++) {
        if ((uint16_t) packetHandler[i].cmd == _cmd && (packetHandler[i].status == STATUS_CONNECTED || (m_bIsAuthed && packetHandler[i].status == STATUS_AUTHED))) {
            pGamePct->read_skip(7); // ignoring header
            (*this.*packetHandler[i].handler)(pGamePct);
            break;
        }
    }

    // Report unknown packets in the error log
    if (i == tableSize) {
        MX_LOG_DEBUG("network", "Got unknown packet '%d' from '%s'", pGamePct->GetPacketID(), m_pSocket->GetRemoteAddress().c_str());
        return;
    }
}

void AuthGameSession::HandleGameLogin(XPacket *pGamePct)
{
    m_pGame->nIDX = pGamePct->read<uint16>();
    m_pGame->szName = pGamePct->ReadString(21);
    m_pGame->szSSU = pGamePct->ReadString(256);
    m_pGame->bIsAdultServer = pGamePct->read<bool>() != 0;
    m_pGame->szIP = pGamePct->ReadString(16);
    m_pGame->nPort = pGamePct->read<int>();
    m_pGame->m_pSession = this;

    auto pGame = sGameMapList->GetGame(m_pGame->nIDX);

    if(pGame == nullptr)
    {
        m_bIsAuthed = true;
        sGameMapList->AddGame(m_pGame);
        MX_LOG_INFO("server.authserver", "Gameserver <%s> [Idx: %d] at %s:%d registered.",m_pGame->szName.c_str(), m_pGame->nIDX, m_pGame->szIP.c_str(), m_pGame->nPort);
        XPacket resultPct(TS_AG_LOGIN_RESULT);
        resultPct << TS_RESULT_SUCCESS;
        m_pSocket->SendPacket(resultPct);
    }
    else
    {
        MX_LOG_INFO("server.authserver", "Gameserver <%s> [Idx: %d] at %s:%d already in list!", m_pGame->szName.c_str(), m_pGame->nIDX, m_pGame->szIP.c_str(), m_pGame->nPort);
        XPacket resultPct(TS_AG_LOGIN_RESULT);
        resultPct << TS_RESULT_ACCESS_DENIED;
        m_pSocket->SendPacket(resultPct);
        m_pSocket->CloseSocket();
    }
}

void AuthGameSession::HandleClientLogin(XPacket *pGamePct)
{
    auto szAccount = pGamePct->ReadString(61);
    auto nOneTimeKey = pGamePct->read<uint64>();

    auto p = sPlayerMapList->GetPlayer(szAccount);
    uint16 result = TS_RESULT_ACCESS_DENIED;

    if(p != nullptr)
    {
        if(nOneTimeKey == p->nOneTimeKey)
        {
            p->bIsInGame = true;
            result = TS_RESULT_SUCCESS;
        }
        else
        {
            MX_LOG_ERROR("network", "AuthGameSession::HandleClientLogin: Client [%d:%s] tried to login with wrong key!!!", p->nAccountID, p->szLoginName.c_str());
        }
    }

    XPacket resultPct(TS_AG_CLIENT_LOGIN);
    resultPct.fill((p != nullptr ? p->szLoginName : ""), 61);
    resultPct << (p != nullptr ? p->nAccountID : 0);
    resultPct << result;
    resultPct << (uint8)0;  // PC Bang Mode
    resultPct << (uint32)0; // Age
    resultPct << (uint32)0; // Event Code
    resultPct << (uint32)0; // Continuous Playtime
    resultPct << (uint32)0; // Continuous Logouttime

    m_pSocket->SendPacket(resultPct);
}

void AuthGameSession::HandleClientLogout(XPacket *pGamePct)
{
    auto szPlayer = pGamePct->ReadString(61);
    auto p = sPlayerMapList->GetPlayer(szPlayer);
    if(p != nullptr) {
        sPlayerMapList->RemovePlayer(szPlayer);
        delete p;
    }
}

void AuthGameSession::HandleClientKickFailed(XPacket *pGamePct)
{
    auto szPlayer = pGamePct->ReadString(61);
    auto p = sPlayerMapList->GetPlayer(szPlayer);
    if(p != nullptr)
    {
        sPlayerMapList->RemovePlayer(szPlayer);
        delete p;
    }
}

void AuthGameSession::KickPlayer(Player *pPlayer)
{
    if(pPlayer == nullptr)
        return;

    XPacket kickPct(TS_AG_KICK_CLIENT);
    kickPct.fill(pPlayer->szLoginName, 61);
    m_pSocket->SendPacket(kickPct);
}
