/*
  *  Copyright (C) 2016-2016 Xijezu <http://xijezu.com>
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

#ifndef _AUTHCLIENTSESSION_H_
#define _AUTHCLIENTSESSION_H_

#include <Lists/PlayerList.h>
#include "Common.h"
#include "Log.h"
#include "WorldSocket.h"
#include "Encryption/XRc4Cipher.h"
#include "XDes.h"

struct Player;

// Handle the player network
class AuthClientSession
{
    public:
        typedef WorldSocket<AuthClientSession> AuthSocket;
        explicit AuthClientSession(AuthSocket *socket);
        virtual ~AuthClientSession();

        void OnClose();
        void ProcessIncoming(XPacket *);

        /// \brief Handler for the Login packet - checks accountname and password
        /// \param packet received packet
        void HandleLoginPacket(XPacket *packet);
        /// \brief Handler for the Version packet, not used yet
        void HandleVersion(XPacket *);
        /// \brief Handler for the server list packet - sends the client the currently connected gameserver(s)
        void HandleServerList(XPacket *);
        /// \brief Handler for the select server packet - generates one time key for gameserver login
        void HandleSelectServer(XPacket *);
        /// \brief Just a function to ignore packets
        void HandleNullPacket(XPacket *) {}
        /// \brief Sends a result message to the client
        /// \param pctID Response to received pctID
        /// \param result TS_RESULT code
        /// \param value More informations
        void SendResultMsg(uint16 pctID, uint16 result, uint value);

        /// \brief Gets the current players AccountID, used in WorldSocket
        /// \return account id
        int GetAccountId() const { return m_pPlayer != nullptr ? m_pPlayer->nAccountID : 0; }
        /// \brief Gets the current players account name, used in WorldSocket
        /// \return account name
        std::string GetAccountName() const { return m_pPlayer != nullptr ? m_pPlayer->szLoginName : "<null>"; }
        /// \brief Gets the Socket
        /// \return AuthSocket
        AuthSocket *GetSocket() const { return _socket != nullptr ? _socket : nullptr; }
    private:
        AuthSocket *_socket{nullptr};
        XDes       _desCipther{ };

        Player *m_pPlayer{nullptr};
        bool _isAuthed{false};
};

#endif // _AUTHCLIENTSESSION_H_