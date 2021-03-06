#include "Common.h"
#include "GameAuthSession.h"
#include "AuthNetwork.h"
#include "World.h"

GameAuthSession::GameAuthSession(AuthSocket* socket) : m_pSocket(socket)
{
	if(socket)
		socket->AddReference();

	m_nGameIDX           = (uint16)sConfigMgr->GetIntDefault("GameServer.Index", 1);
	m_szGameName         = sConfigMgr->GetStringDefault("GameServer.Name", "Testserver");
	m_szGameSSU          = sConfigMgr->GetStringDefault("GameServer.SSU", "about:blank");
	m_bGameIsAdultServer = sConfigMgr->GetIntDefault("GameServer.AdultServer", 0) != 0;
	m_szGameIP           = sConfigMgr->GetStringDefault("GameServer.IP", "127.0.0.1");
	m_nGamePort          = sConfigMgr->GetIntDefault("GameServer.Port", 4514);
}

GameAuthSession::~GameAuthSession()
{
    if(m_pSocket)
		m_pSocket->RemoveReference();
}

typedef struct AuthHandler {
	uint16_t cmd;
	void (GameAuthSession::*handler)(XPacket *);
} AuthHandler;

const AuthHandler packetHandler[] =
						  {
								  {TS_AG_LOGIN_RESULT, &GameAuthSession::HandleGameLoginResult},
								  {TS_AG_KICK_CLIENT,  &GameAuthSession::HandleClientKick},
								  {TS_AG_CLIENT_LOGIN, &GameAuthSession::HandleClientLoginResult},
						  };

const int tableSize = (sizeof(packetHandler) / sizeof(AuthHandler));

void GameAuthSession::ProcessIncoming(XPacket* pGamePct)
{
			ASSERT(pGamePct);

	auto _cmd = pGamePct->GetPacketID();
	int  i    = 0;

	for (i = 0; i < tableSize; i++)
	{
		if ((uint16_t)packetHandler[i].cmd == _cmd)
		{
			pGamePct->read_skip(7); // ignoring header
			(*this.*packetHandler[i].handler)(pGamePct);
			break;
		}
	}

	// Report unknown packets in the error log
	if (i == tableSize)
	{
		MX_LOG_DEBUG("network", "Got unknown packet '%d' from '%s'", pGamePct->GetPacketID(), m_pSocket->GetRemoteAddress().c_str());
		return;
	}
}

void GameAuthSession::HandleClientLoginResult(XPacket *pRecvPct)
{
	auto szAccount = pRecvPct->ReadString(61);
	pRecvPct->rpos(0);
	if(m_queue.count(szAccount) == 1)
	{
		m_queue[szAccount]->ProcessIncoming(pRecvPct);
		m_queue.erase(szAccount);
	}
}

void GameAuthSession::HandleClientKick(XPacket *pRecvPct)
{
    auto szPlayer = pRecvPct->ReadString(61);
    auto player = Player::FindPlayer(szPlayer);
    if(player != nullptr)
    {
        player->GetSession().KickPlayer();
    }
}

void GameAuthSession::AccountToAuth(WorldSession* pSession, const std::string& szLoginName, uint64 nOneTimeKey)
{
	m_queue[szLoginName] = pSession;
	XPacket packet(TS_GA_CLIENT_LOGIN);
	packet.fill(szLoginName, 61);
	packet << (uint64)nOneTimeKey;
	m_pSocket->SendPacket(packet);
}

int GameAuthSession::GetAccountId() const
{
	return m_nGameIDX;
}

std::string GameAuthSession::GetAccountName()
{
	return m_szGameName;
}

void GameAuthSession::OnClose()
{
    MX_LOG_ERROR("network", "Authserver has closed connection!");
}

void GameAuthSession::HandleGameLoginResult(XPacket *pRecvPct)
{
    auto result = pRecvPct->read<uint16>();
    if(result != TS_RESULT_SUCCESS)
    {
        MX_LOG_ERROR("network", "Authserver refused login! Shutting down...");
        World::StopNow(ERROR_EXIT_CODE);
    }
}

void GameAuthSession::ClientLogoutToAuth(const std::string &szAccount)
{
    XPacket packet(TS_GA_CLIENT_LOGOUT);
    packet.fill(szAccount, 61);
    packet << (uint32)0;
    m_pSocket->SendPacket(packet);
}

void GameAuthSession::SendGameLogin()
{
    XPacket loginPct(TS_GA_LOGIN);
    loginPct << (uint16)m_nGameIDX;
    loginPct.fill(m_szGameName, 21);
    loginPct.fill(m_szGameSSU, 256);
    loginPct << (uint8)(m_bGameIsAdultServer ? 1 : 0);
    loginPct.fill(m_szGameIP, 16);
    loginPct << (uint)m_nGamePort;
    m_pSocket->SendPacket(loginPct);
}
