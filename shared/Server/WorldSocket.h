/*
 * Copyright (C) 2011-2017 Project SkyFire <http://www.projectskyfire.org/>
 * Copyright (C) 2008-2017 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2017 MaNGOS <https://www.getmangos.eu/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
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

#ifndef SF_WORLDSOCKET_H
#define SF_WORLDSOCKET_H

#include <ace/Basic_Types.h>
#include <ace/Synch_Traits.h>
#include <ace/Svc_Handler.h>
#include <ace/SOCK_Stream.h>
#include <ace/Thread_Mutex.h>
#include <ace/Guard_T.h>
#include <ace/Unbounded_Queue.h>
#include <ace/Message_Block.h>

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

#include "Common.h"
#include "XRc4Cipher.h"
#include "XPacket.h"
#include "TS_MESSAGE.h"
#include "Util.h"

template<class T> class WorldSocketMgr;
class ACE_Message_Block;

/*class WorldSession;
class GameAuthSession;
class AuthClientSession;
class AuthGameSession;*/

/// Handler that can communicate over stream sockets.
typedef ACE_Svc_Handler<ACE_SOCK_STREAM, ACE_NULL_SYNCH> WorldHandler;

/**
 * WorldSocket.
 *
 * This class is responsible for the communication with
 * remote clients.
 * Most methods return -1 on failure.
 * The class uses reference counting.
 *
 * For output the class uses one buffer (64K usually) and
 * a queue where it stores packet if there is no place on
 * the queue. The reason this is done, is because the server
 * does really a lot of small-size writes to it, and it doesn't
 * scale well to allocate memory for every. When something is
 * written to the output buffer the socket is not immediately
 * activated for output (again for the same reason), there
 * is 10ms celling (thats why there is Update() method).
 * This concept is similar to TCP_CORK, but TCP_CORK
 * uses 200ms celling. As result overhead generated by
 * sending packets from "producer" threads is minimal,
 * and doing a lot of writes with small size is tolerated.
 *
 * The calls to Update() method are managed by WorldSocketMgr
 * and ReactorRunnable.
 *
 * For input, the class uses one 4096 bytes buffer on stack
 * to which it does recv() calls. And then received data is
 * distributed where its needed. 4096 matches pretty well the
 * traffic generated by client for now.
 *
 * The input/output do speculative reads/writes (AKA it tryes
 * to read all data available in the kernel buffer or tryes to
 * write everything available in userspace buffer),
 * which is ok for using with Level and Edge Triggered IO
 * notification.
 *
 */

template<class T>
class WorldSocket : public WorldHandler
{
    public:
        WorldSocket(void) : WorldHandler(),
                            m_LastPingTime(ACE_Time_Value::zero), m_OverSpeedPings(0), m_Session(nullptr),
                            m_RecvWPct(0), m_RecvPct(), m_Header(sizeof(TS_MESSAGE)),
                            m_WorldHeader(sizeof(TS_MESSAGE)), m_OutBuffer(0),
                            m_OutBufferSize(65536), m_OutActive(false),

                            m_Seed(static_cast<uint32> (rand32()))
        {
            reference_counting_policy().value(ACE_Event_Handler::Reference_Counting_Policy::ENABLED);

            m_Crypt.SetKey("}h79q~B%al;k'y $E");
            m_Decrypt.SetKey("}h79q~B%al;k'y $E");

            msg_queue()->high_water_mark(8 * 1024 * 1024);
            msg_queue()->low_water_mark(8 * 1024 * 1024);
        }

        virtual ~WorldSocket(void)
        {
            delete m_RecvWPct;

            if (m_OutBuffer)
                m_OutBuffer->release();

            closing_ = true;

            peer().close();
        }

        friend class WorldSocketMgr<T>;

        /// Mutex type used for various synchronizations.
        typedef ACE_Thread_Mutex    LockType;
        typedef ACE_Guard<LockType> GuardType;

        /// Check if socket is closed.
        bool IsClosed(void) const
        {
            return closing_;
        }

        /// Close the socket.
        void CloseSocket(void)
        {
            {
                ACE_GUARD (LockType, Guard, m_OutBufferLock);

                if (closing_)
                    return;

                closing_ = true;
                peer().close_writer();
            }

            {
                ACE_GUARD (LockType, Guard, m_SessionLock);

                if (m_Session)
                    m_Session->OnClose();

                m_Crypt.Clear();
                m_Decrypt.Clear();

                m_Session = NULL;
            }
        }

        /// Return the session
        T *GetSession() { return m_Session; }

        /// Get address of connected peer.
        const std::string &GetRemoteAddress(void) const
        {
            return m_Address;
        }

        /// Send A packet on the socket, this function is reentrant.
        /// @param pct packet to send
        /// @return -1 of failure
        int SendPacket(XPacket &pct)
        {
            ACE_GUARD_RETURN (LockType, Guard, m_OutBufferLock, -1);

            if (closing_)
                return -1;

            pct.FinalizePacket();
            // Encrypt the packet before sending
            m_Crypt.Encode((void *)&pct.contents()[0], (void *)&pct.contents()[0], pct.size(), false);

            // Dump outgoing packet
//    if (sPacketLog->CanLogPacket())
//        sPacketLog->LogPacket(pct, SERVER_TO_CLIENT);

            XPacket const *pkt = &pct;

            // Empty buffer used in case packet should be compressed
            // Disable compression for now :)
            /* WorldPacket buff;
             if (m_Session && pkt->size() > 0x400)
             {
                 buff.Compress(m_Session->GetCompressionStream(), pkt);
                 pkt = &buff;
             }*/


            //if (m_Session)
            //SF_LOG_TRACE("network.opcode", "S->C: %s %s", m_Session->GetPlayerInfo().c_str(), GetOpcodeNameForLogging(pkt->GetOpcode(), true).c_str());


            if (m_OutBuffer->space() >= pkt->size() && msg_queue()->is_empty())
            {
                if (!pkt->empty())
                    if (m_OutBuffer->copy((char *)pkt->contents(), pkt->size()) == -1)
                        ACE_ASSERT (false);
            }
            else
            {
                // Enqueue the packet.
                ACE_Message_Block *mb;

                ACE_NEW_RETURN(mb, ACE_Message_Block(pkt->size()), -1);

                if (!pkt->empty())
                    mb->copy((const char *)pkt->contents(), pkt->size());

                if (msg_queue()->enqueue_tail(mb, (ACE_Time_Value *)&ACE_Time_Value::zero) == -1)
                {
                    MX_LOG_ERROR("network", "WorldSocket::SendPacket enqueue_tail failed");
                    mb->release();
                    return -1;
                }
            }

            return 0;
        }

        /// Add reference to this object.
        long AddReference(void)
        {
            return static_cast<long> (add_reference());
        }

        /// Remove reference to this object.
        long RemoveReference(void)
        {
            return static_cast<long> (remove_reference());
        }

        /// things called by ACE framework.

        /// Called on open, the void* is the acceptor.
        virtual int open(void *a)
        {
            ACE_UNUSED_ARG (a);

            // Prevent double call to this func.
            if (m_OutBuffer)
                return -1;

            // This will also prevent the socket from being Updated
            // while we are initializing it.
            m_OutActive = true;

            // Hook for the manager.
            if (ACE_Singleton<WorldSocketMgr<T>, ACE_Thread_Mutex>::instance()->OnSocketOpen(this) == -1)
                return -1;

            // Allocate the buffer.
            ACE_NEW_RETURN (m_OutBuffer, ACE_Message_Block (m_OutBufferSize), -1);

            // Store peer address.
            ACE_INET_Addr remote_addr;

            if (peer().get_remote_addr(remote_addr) == -1)
            {
                MX_LOG_ERROR("network", "WorldSocket::open: peer().get_remote_addr errno = %s", ACE_OS::strerror(errno));
                return -1;
            }

            m_Address = remote_addr.get_host_addr();

            // Register with ACE Reactor
            if (reactor()->register_handler(this, ACE_Event_Handler::READ_MASK | ACE_Event_Handler::WRITE_MASK) == -1)
            {
                MX_LOG_ERROR("network", "WorldSocket::open: unable to register client handler errno = %s", ACE_OS::strerror(errno));
                return -1;
            }

            {
                ACE_GUARD_RETURN(LockType, Guard, m_SessionLock, -1);
                // NOTE ATM the socket is single-threaded, have this in mind ...
                ACE_NEW_RETURN(m_Session, T(this), -1);
            }

            // reactor takes care of the socket from now on
            remove_reference();

            return 0;
        }

        /// Called on failures inside of the acceptor, don't call from your code.
        virtual int close(u_long)
        {
            shutdown();

            closing_ = true;

            remove_reference();

            return 0;
        }

        /// Called when we can read from the socket.
        virtual int handle_input(ACE_HANDLE = ACE_INVALID_HANDLE)
        {
            if (closing_)
                return -1;

            switch (handle_input_missing_data())
            {
                case -1 :
                {
                    if ((errno == EWOULDBLOCK) ||
                        (errno == EAGAIN))
                    {
                        return Update();                           // interesting line, isn't it ?
                    }

                    MX_LOG_TRACE("network", "WorldSocket::handle_input: Peer error closing connection errno = %s", ACE_OS::strerror(errno));

                    errno = ECONNRESET;
                    return -1;
                }
                case 0:
                {
                    MX_LOG_TRACE("network", "WorldSocket::handle_input: Peer has closed connection");

                    errno = ECONNRESET;
                    return -1;
                }
                case 1:
                    return 1;
                default:
                    return Update();                               // another interesting line ;)
            }

            ACE_NOTREACHED(return -1);
        }

        /// Called when the socket can write.
        virtual int handle_output(ACE_HANDLE = ACE_INVALID_HANDLE)
        {
            ACE_GUARD_RETURN (LockType, Guard, m_OutBufferLock, -1);

            if (closing_)
                return -1;

            size_t send_len = m_OutBuffer->length();

            if (send_len == 0)
                return handle_output_queue(Guard);

#ifdef MSG_NOSIGNAL
            ssize_t n = peer().send(m_OutBuffer->rd_ptr(), send_len, MSG_NOSIGNAL);
#else
            ssize_t n = peer().send (m_OutBuffer->rd_ptr(), send_len);
#endif // MSG_NOSIGNAL

            if (n == 0)
                return -1;
            else if (n == -1)
            {
                if (errno == EWOULDBLOCK || errno == EAGAIN)
                    return schedule_wakeup_output(Guard);

                return -1;
            }
            else if (n < (ssize_t)send_len) //now n > 0
            {
                m_OutBuffer->rd_ptr(static_cast<size_t> (n));

                // move the data to the base of the buffer
                m_OutBuffer->crunch();

                return schedule_wakeup_output(Guard);
            }
            else //now n == send_len
            {
                m_OutBuffer->reset();

                return handle_output_queue(Guard);
            }

            ACE_NOTREACHED (return 0);
        }

        /// Called when connection is closed or error happens.
        virtual int handle_close(ACE_HANDLE h = ACE_INVALID_HANDLE, ACE_Reactor_Mask = ACE_Event_Handler::ALL_EVENTS_MASK)
        {
            // Critical section
            {
                ACE_GUARD_RETURN (LockType, Guard, m_OutBufferLock, -1);

                closing_ = true;

                if (h == ACE_INVALID_HANDLE)
                    peer().close_writer();
            }

            // Critical section
            {
                ACE_GUARD_RETURN (LockType, Guard, m_SessionLock, -1);
                if (m_Session)
                    m_Session->OnClose();
                m_Session = NULL;
            }

            reactor()->remove_handler(this, ACE_Event_Handler::DONT_CALL | ACE_Event_Handler::ALL_EVENTS_MASK);
            return 0;
        }

        /// Called by WorldSocketMgr/ReactorRunnable.
        int Update(void)
        {
            if (closing_)
                return -1;

            if (m_OutActive)
                return 0;

            {
                ACE_GUARD_RETURN (LockType, Guard, m_OutBufferLock, 0);
                if (m_OutBuffer->length() == 0 && msg_queue()->is_empty())
                    return 0;
            }

            int ret;
            do
                ret = handle_output(get_handle());
            while (ret > 0);

            return ret;
        }

    private:
        /// Helper functions for processing incoming data.
        int handle_input_header(void)
        {
            ACE_ASSERT(m_RecvWPct == NULL);
            ACE_ASSERT(m_WorldHeader.length() == sizeof(TS_MESSAGE));

            m_Decrypt.Decode((uint8 *)m_WorldHeader.rd_ptr(), (uint8 *)m_WorldHeader.rd_ptr(), sizeof(TS_MESSAGE), true);
            TS_MESSAGE &header = *(TS_MESSAGE *)m_WorldHeader.rd_ptr();

            if (header.size > 10236)
            {
                MX_LOG_INFO("network", "WorldSocket::handle_input_header(): client (account: %u, char [Name: %s]) sent malformed packet (size: %d, cmd: %d)",
                            m_Session ? m_Session->GetAccountId() : 0,
                            m_Session ? m_Session->GetAccountName().c_str() : "<none>",
                            header.size, header.id);

                errno = EINVAL;
                return -1;
            }

            ACE_NEW_RETURN(m_RecvWPct, XPacket((uint16_t)
                    header.id, header.size, m_WorldHeader.rd_ptr()), -1);

            if (header.size > 0)
            {
                m_RecvWPct->resize(header.size);
                m_RecvPct.base((char *)m_RecvWPct->contents(), m_RecvWPct->size());
            }
            else
                ACE_ASSERT(m_RecvPct.space() == 0);

            return 0;
        }

        int handle_input_payload(void)
        {
            // set errno properly here on error !!!
            // now have a header and payload

            ACE_ASSERT (m_RecvPct.space() == 0);
            ACE_ASSERT (m_WorldHeader.space() == 0);
            ACE_ASSERT (m_RecvWPct != NULL);

            m_Decrypt.Decode(&m_RecvWPct->contents()[0], &m_RecvWPct->contents()[0], m_RecvWPct->size(), false);
            const int ret = ProcessIncoming(m_RecvWPct);

            m_RecvPct.base(nullptr, 0);
            m_RecvPct.reset();
            m_RecvWPct = nullptr;

            m_WorldHeader.reset();

            if (ret == -1)
                errno = EINVAL;

            return ret;
        }

        int handle_input_missing_data(void)
        {
            char buf[4096];

            ACE_Data_Block db(sizeof(buf),
                              ACE_Message_Block::MB_DATA,
                              buf,
                              0,
                              0,
                              ACE_Message_Block::DONT_DELETE,
                              0);

            ACE_Message_Block message_block(&db,
                                            ACE_Message_Block::DONT_DELETE,
                                            0);

            const size_t recv_size = message_block.space();

            const ssize_t n = peer().recv(message_block.wr_ptr(),
                                          recv_size);

            if (n <= 0)
                return int(n);

            message_block.wr_ptr(n);

            while (message_block.length() > 0)
            {
                if (m_WorldHeader.space() > 0)
                {
                    //need to receive the header
                    const size_t to_header = (message_block.length() > m_WorldHeader.space() ? m_WorldHeader.space() : message_block.length());
                    m_WorldHeader.copy(message_block.rd_ptr(), to_header);
                    //message_block.rd_ptr(to_header);

                    if (m_WorldHeader.space() > 0)
                    {
                        // Couldn't receive the whole header this time.
                        ACE_ASSERT (message_block.length() == 0);
                        errno = EWOULDBLOCK;
                        return -1;
                    }

                    // We just received nice new header
                    if (handle_input_header() == -1)
                    {
                        ACE_ASSERT ((errno != EWOULDBLOCK) && (errno != EAGAIN));
                        return -1;
                    }
                }

                // Its possible on some error situations that this happens
                // for example on closing when epoll receives more chunked data and stuff
                // hope this is not hack, as proper m_RecvWPct is asserted around
                if (!m_RecvWPct)
                {
                    MX_LOG_ERROR("network", "Forcing close on input m_RecvWPct = NULL");
                    errno = EINVAL;
                    return -1;
                }

                // We have full read header, now check the data payload
                if (m_RecvPct.space() > 0)
                {
                    //need more data in the payload
                    const size_t to_data = (message_block.length() > m_RecvPct.space() ? m_RecvPct.space() : message_block.length());
                    m_RecvPct.copy(message_block.rd_ptr(), to_data);
                    message_block.rd_ptr(to_data);

                    if (m_RecvPct.space() > 0)
                    {
                        // Couldn't receive the whole data this time.
                        ACE_ASSERT (message_block.length() == 0);
                        errno = EWOULDBLOCK;
                        return -1;
                    }
                }

                //just received fresh new payload
                if (handle_input_payload() == -1)
                {
                    ACE_ASSERT ((errno != EWOULDBLOCK) && (errno != EAGAIN));
                    return -1;
                }
            }

            return size_t(n) == recv_size ? 1 : 2;
        }

        /// Help functions to mark/unmark the socket for output.
        /// @param g the guard is for m_OutBufferLock, the function will release it
        int cancel_wakeup_output(GuardType &g)
        {
            if (!m_OutActive)
                return 0;

            m_OutActive = false;

            g.release();

            if (reactor()->cancel_wakeup
                                 (this, ACE_Event_Handler::WRITE_MASK) == -1)
            {
                // would be good to store errno from reactor with errno guard
                MX_LOG_ERROR("network", "WorldSocket::cancel_wakeup_output");
                return -1;
            }

            return 0;
        }

        int schedule_wakeup_output(GuardType &g)
        {
            if (m_OutActive)
                return 0;

            m_OutActive = true;

            g.release();

            if (reactor()->schedule_wakeup
                                 (this, ACE_Event_Handler::WRITE_MASK) == -1)
            {
                MX_LOG_ERROR("network", "WorldSocket::schedule_wakeup_output");
                return -1;
            }

            return 0;
        }

        /// Drain the queue if its not empty.
        int handle_output_queue(GuardType &g)
        {
            if (msg_queue()->is_empty())
                return cancel_wakeup_output(g);

            ACE_Message_Block *mblk;

            if (msg_queue()->dequeue_head(mblk, (ACE_Time_Value *)&ACE_Time_Value::zero) == -1)
            {
                MX_LOG_ERROR("network", "WorldSocket::handle_output_queue dequeue_head");
                return -1;
            }

            const size_t send_len = mblk->length();

#ifdef MSG_NOSIGNAL
            ssize_t n = peer().send(mblk->rd_ptr(), send_len, MSG_NOSIGNAL);
#else
            ssize_t n = peer().send(mblk->rd_ptr(), send_len);
#endif // MSG_NOSIGNAL

            if (n == 0)
            {
                mblk->release();

                return -1;
            }
            else if (n == -1)
            {
                if (errno == EWOULDBLOCK || errno == EAGAIN)
                {
                    msg_queue()->enqueue_head(mblk, (ACE_Time_Value *)&ACE_Time_Value::zero);
                    return schedule_wakeup_output(g);
                }

                mblk->release();
                return -1;
            }
            else if (n < (ssize_t)send_len) //now n > 0
            {
                mblk->rd_ptr(static_cast<size_t> (n));

                if (msg_queue()->enqueue_head(mblk, (ACE_Time_Value *)&ACE_Time_Value::zero) == -1)
                {
                    MX_LOG_ERROR("network", "WorldSocket::handle_output_queue enqueue_head");
                    mblk->release();
                    return -1;
                }

                return schedule_wakeup_output(g);
            }
            else //now n == send_len
            {
                mblk->release();

                return msg_queue()->is_empty() ? cancel_wakeup_output(g) : ACE_Event_Handler::WRITE_MASK;
            }

            ACE_NOTREACHED(return -1);
        }

        /// process one incoming packet.
        /// @param new_pct received packet, note that you need to delete it.
        int ProcessIncoming(XPacket *new_pct)
        {
            ACE_ASSERT (new_pct);

            // manage memory ;)
            ACE_Auto_Ptr<XPacket> aptr(new_pct);

            if (closing_)
                return -1;

            // Dump received packet.
            //if (sPacketLog->CanLogPacket())
            //sPacketLog->LogPacket(*new_pct, CLIENT_TO_SERVER);

            //if (m_Session)
            //SF_LOG_TRACE("network.opcode", "C->S: %s %s", m_Session->GetPlayerInfo().c_str(), opcodeName.c_str());

            try
            {
                {
                    ACE_GUARD_RETURN(LockType, Guard, m_SessionLock, -1);
                    if (m_Session == nullptr)
                    {
                        MX_LOG_ERROR("network.opcode", "ProcessIncoming: Client not authed packet_id = %u", uint32(new_pct->GetPacketID()));
                        return -1;
                    }

                    // OK, give the packet to WorldSession
                    aptr.release();
                    // WARNING here we call it with locks held.
                    // Its possible to cause deadlock if QueuePacket calls back
                    m_Session->ProcessIncoming(new_pct);
                    return 0;
                }
            }
            catch (ByteBufferException &)
            {
                MX_LOG_ERROR("network", "WorldSocket::ProcessIncoming ByteBufferException occured while parsing an instant handled packet %d from client %s, accountid=%i. Disconnected client.",
                             new_pct->GetPacketID(), GetRemoteAddress().c_str(), m_Session ? int32(m_Session->GetAccountId()) : -1);
                return -1;
            }

            ACE_NOTREACHED (return 0);
        }

    private:
        /// Time in which the last ping was received
        ACE_Time_Value m_LastPingTime;

        /// Keep track of over-speed pings, to prevent ping flood.
        uint32 m_OverSpeedPings;

        /// Address of the remote peer
        std::string m_Address;

        /// Class used for managing encryption of the headers
        XRC4Cipher m_Crypt, m_Decrypt;

        /// Mutex lock to protect m_Session
        LockType m_SessionLock;

        /// Session to which received packets are routed
        T *m_Session;

        /// here are stored the fragments of the received data
        XPacket *m_RecvWPct;

        /// This block actually refers to m_RecvWPct contents,
        /// which allows easy and safe writing to it.
        /// It wont free memory when its deleted. m_RecvWPct takes care of freeing.
        ACE_Message_Block m_RecvPct;

        /// Fragment of the received header.
        ACE_Message_Block m_Header;
        ACE_Message_Block m_WorldHeader;

        /// Mutex for protecting output related data.
        LockType m_OutBufferLock;

        /// Buffer used for writing output.
        ACE_Message_Block *m_OutBuffer;

        /// Size of the m_OutBuffer.
        size_t m_OutBufferSize;

        /// True if the socket is registered with the reactor for output
        bool m_OutActive;

        uint32 m_Seed;

};

#endif  /* _WORLDSOCKET_H */

/// @}
