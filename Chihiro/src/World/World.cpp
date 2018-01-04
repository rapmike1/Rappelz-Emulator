#include "World.h"
#include "DatabaseEnv.h"
#include "Map/ArRegion.h"
#include "Globals/ObjectMgr.h"
#include "Timer.h"
#include "Scripting/XLua.h"
#include "Messages.h"
#include "ClientPackets.h"
#include "Maploader.h"
#include "RespawnObject.h"
#include "WorldSession.h"
#include "MemPool.h"
#include "GameRule.h"

ACE_Atomic_Op<ACE_Thread_Mutex, bool> World::m_stopEvent = false;
uint8 World::m_ExitCode = SHUTDOWN_EXIT_CODE;
ACE_Atomic_Op<ACE_Thread_Mutex, uint32> World::m_worldLoopCounter = 0;

World::World() : startTime(getMSTime())
{
    srand((unsigned int)time(NULL));
}


World::~World()
{

}

uint World::GetArTime()
{
	return GetMSTimeDiffToNow(startTime) / 10;
}

void World::InitWorld()
{
    MX_LOG_INFO("server.worldserver", "Initializing world...");

    uint32_t oldTime = getMSTime(), oldFullTime = getMSTime();
	MX_LOG_INFO("server.worldserver", "Initializing region system...");
	sArRegion->InitRegionSystem(sConfigMgr->GetIntDefault("Game.MapWidth", 700000), sConfigMgr->GetIntDefault("Game.MapHeight", 1000000));
	MX_LOG_INFO("server.worldserver", "Initialized region system in %u ms", GetMSTimeDiffToNow(oldTime));

    oldTime = getMSTime();
	MX_LOG_INFO("server.worldserver", "Initializing game content...");

    // Dörti häckz, plz ihgnoar
    s_nItemIndex = CharacterDatabase.Query("SELECT MAX(sid) FROM Item;").get()->Fetch()->GetUInt64();;
    s_nPlayerIndex = CharacterDatabase.Query("SELECT MAX(sid) FROM `Character`;").get()->Fetch()->GetUInt64();
	s_nSkillIndex = CharacterDatabase.Query("SELECT MAX(sid) FROM `Skill`;").get()->Fetch()->GetUInt64();
	s_nSummonIndex = CharacterDatabase.Query("SELECT MAX(sid) FROM `Summon`;").get()->Fetch()->GetUInt64();

	sObjectMgr->InitGameContent();
	MX_LOG_INFO("server.worldserver", "Initialized game content in %u ms", GetMSTimeDiffToNow(oldTime));

    oldTime = getMSTime();
    MX_LOG_INFO("server.worldserver", "Initializing scripting...");
    sScriptingMgr->InitializeLua();
	sMapContent->LoadMapContent();
	sMapContent->InitMapInfo();
    MX_LOG_INFO("server.worldserver", "Initialized scripting in %u ms", GetMSTimeDiffToNow(oldTime));

    for(auto& ri : sObjectMgr->g_vRespawnInfo) {
        MonsterRespawnInfo nri(ri);
        float cx = (nri.right - nri.left) * 0.5f + nri.left;
        float cy = (nri.top - nri.bottom) * 0.5f + nri.bottom;
        auto ro = new RespawnObject{nri};
        m_vRespawnList.emplace_back(ro);
    }

	MX_LOG_INFO("server.worldserver", "World fully initialized in %u ms!", GetMSTimeDiffToNow(oldFullTime));
}

/// Find a session by its id
WorldSession* World::FindSession(uint32 id) const
{
	SessionMap::const_iterator itr = m_sessions.find(id);

	if (itr != m_sessions.end())
		return itr->second;                                 // also can return NULL for kicked session
	else
		return nullptr;
}

/// Remove a given session
bool World::RemoveSession(uint32 id)
{
    if(m_sessions.count(id) == 1)
    {
        m_sessions.erase(id);
        return true;
    }
    return false;
}

void World::AddSession(WorldSession* s)
{
	if(s == nullptr)
		return;
    if(m_sessions.count(s->GetAccountId()) == 0) {
        m_sessions.insert({ (uint32)s->GetAccountId(), s });
    }
}

uint64 World::GetItemIndex()
{
    return ++s_nItemIndex;
}

uint64 World::GetPlayerIndex()
{
    return ++s_nPlayerIndex;
}

uint64 World::GetPetIndex()
{
    return ++s_nPetIndex;
}

uint64 World::GetSummonIndex()
{
    return ++s_nSummonIndex;
}

uint64 World::GetSkillIndex()
{
	return ++s_nSkillIndex;
}

bool World::SetMultipleMove(Unit *pUnit, Position curPos, std::vector<Position> newPos, uint8_t speed, bool bAbsoluteMove, uint t, bool bBroadcastMove)
{
    Position oldPos{};
    Position lastpos = newPos.back();

	bool result = false;
	if(bAbsoluteMove || true/* onSetMove Quadtreepotato*/) {
		oldPos.m_positionX = pUnit->GetPositionX();
		oldPos.m_positionY = pUnit->GetPositionY();
		oldPos.m_positionZ = pUnit->GetPositionZ();
		oldPos._orientation = pUnit->GetOrientation();

		pUnit->SetCurrentXY(curPos.GetPositionX(), curPos.GetPositionY());
		curPos.m_positionX = pUnit->GetPositionX();
        curPos.m_positionY = pUnit->GetPositionY();
        curPos.m_positionZ = pUnit->GetPositionZ();
        curPos.SetOrientation(pUnit->GetOrientation());

		onMoveObject(pUnit, oldPos, curPos);
		enterProc(pUnit, (uint)(oldPos.GetPositionX() / g_nRegionSize), (uint)(oldPos.GetPositionY() / g_nRegionSize));
		pUnit->SetMultipleMove(newPos, speed, t);

		if(bBroadcastMove) {
			sArRegion->DoEachVisibleRegion((uint) (pUnit->GetPositionX() / g_nRegionSize), (uint) (pUnit->GetPositionY() / g_nRegionSize), pUnit->GetLayer(),
										   [=](ArRegion *region) {
											   region->DoEachClient([=](WorldObject *obj) {
												   Messages::SendMoveMessage(dynamic_cast<Player *>(obj), pUnit);
											   });
										   });
		}
        result = true;
	}
    return result;
}

bool World::SetMove(Unit *obj, Position curPos, Position newPos, uint8 speed, bool bAbsoluteMove, uint t, bool bBroadcastMove)
{
    Position oldPos{};
    Position curPos2{};

    if(bAbsoluteMove) {
        if(obj->bIsMoving && obj->IsInWorld()) {
            oldPos = *dynamic_cast<Position*>(obj);
            obj->SetCurrentXY(curPos.GetPositionX(), curPos.GetPositionY());
            curPos2 = *dynamic_cast<Position*>(obj);
            onMoveObject(obj, oldPos, curPos2);
            enterProc(obj, (uint)(oldPos.GetPositionX() / g_nRegionSize), (uint)(oldPos.GetPositionY() / g_nRegionSize));
            obj->SetMove(newPos, speed, t);
        } else {
            obj->SetMove(newPos, speed, t);
        }
        if(bBroadcastMove) {
            sArRegion->DoEachVisibleRegion((uint) (obj->GetPositionX() / g_nRegionSize),
                                           (uint) (obj->GetPositionY() / g_nRegionSize),
                                           obj->GetLayer(),
                                           [=](ArRegion *region) {
                                               region->DoEachClient([=](WorldObject *pObj) {
                                                   Messages::SendMoveMessage(dynamic_cast<Player *>(pObj), obj);
                                               });
                                           });

        }
        return true;
    }
    return false;
}


void World::onMoveObject(WorldObject *pUnit, Position oldPos, Position newPos)
{
	auto prev_rx = (uint)(oldPos.m_positionX / g_nRegionSize);
	auto prev_ry = (uint)(oldPos.m_positionY / g_nRegionSize);
	if(prev_rx != (uint)(newPos.GetPositionX() / g_nRegionSize) || prev_ry != (uint)(newPos.GetPositionY() / g_nRegionSize)) {
		sArRegion->GetRegion(prev_rx, prev_ry, (uint32)pUnit->GetLayer())->RemoveObject(pUnit);
		sArRegion->GetRegion(pUnit)->AddObject(pUnit);
	}
}

void World::enterProc(WorldObject *pUnit, uint prx, uint pry)
{
	auto rx = (uint)(pUnit->GetPositionX() / g_nRegionSize);
	auto ry = (uint)(pUnit->GetPositionY() / g_nRegionSize);
	if(rx != prx || ry != pry) {
		sArRegion->DoEachNewRegion(rx, ry, prx, pry, pUnit->GetLayer(), [=](ArRegion* rgn) {
			rgn->DoEachClient([=](Unit* client) { // enterProc
				// BEGIN Send Enter Message to each other
				if(client->GetHandle() != pUnit->GetHandle()) {
					Messages::sendEnterMessage(dynamic_cast<Player*>(pUnit), client, false);
					if(client->GetSubType() == ST_Player) {
						Messages::sendEnterMessage(dynamic_cast<Player *>(client), pUnit, false);
					}
				}
			});	// END Send Enter Message to each other
			auto func = [=](WorldObject* obj) {
				Messages::sendEnterMessage(dynamic_cast<Player*>(pUnit), obj, false);
			};
			rgn->DoEachMovableObject(func);
			rgn->DoEachStaticObject(func);
		});
	}
}

void World::AddObjectToWorld(WorldObject *obj)
{
    auto region = sArRegion->GetRegion(obj);
    if (region == nullptr)
        return;

    sArRegion->DoEachVisibleRegion((uint) (obj->GetPositionX() / g_nRegionSize), (uint) (obj->GetPositionY() / g_nRegionSize), obj->GetLayer(),
                                   [=](ArRegion *rgn) {
                                       rgn->DoEachClient([=](Unit *client) { // enterProc
                                           // BEGIN Send Enter Message to each other
                                           if (client->GetHandle() != obj->GetHandle()) {
                                               // Enter message FROM doEachRegion-Client TO obj
                                               if (client->IsInWorld() && obj->GetSubType() == ST_Player)
                                                   Messages::sendEnterMessage(dynamic_cast<Player *>(obj), client, false);
                                               // Enter message FROM obj TO doEachRegion-Client
                                               if (client->GetSubType() == ST_Player)
                                                   Messages::sendEnterMessage(dynamic_cast<Player *>(client), obj, false);
                                           }
                                       });    // END Send Enter Message to each other
									   /// Sending enter messages of NPCs and pets to the object - if it's an player
                                       rgn->DoEachMovableObject([=](WorldObject *lbObj) {
                                           if (lbObj->IsInWorld() && obj->GetSubType() == ST_Player)
                                               Messages::sendEnterMessage(dynamic_cast<Player *>(obj), lbObj, false);
                                       });
                                       rgn->DoEachStaticObject([=](WorldObject *lbObj) {
                                           if (lbObj->IsInWorld() && obj->GetSubType() == ST_Player)
                                               Messages::sendEnterMessage(dynamic_cast<Player *>(obj), lbObj, false);
                                       });
                                   });
    region->AddObject(obj);
}

void World::onRegionChange(WorldObject *obj, uint update_time, bool bIsStopMessage)
{
    auto oldx = (uint)(obj->GetPositionX() / g_nRegionSize);
    auto oldy = (uint)(obj->GetPositionY() / g_nRegionSize);
    step(obj, (uint)(update_time + obj->lastStepTime + (bIsStopMessage ? 0xA : 0)));

    if((uint)(obj->GetPositionX() / g_nRegionSize) != oldx || (uint)(obj->GetPositionY() / g_nRegionSize) != oldy) {
        enterProc(obj, oldx, oldy);
        obj->prevX = oldx;
        obj->prevY = oldy;
    }
}

void World::RemoveObjectFromWorld(WorldObject *obj)
{
    // Get Region
    auto    region = sArRegion->GetRegion(obj);
    // Create & set leave packet
    XPacket leavePct(TS_SC_LEAVE);
    leavePct << obj->GetHandle();
    // Send one to each player in visible region
    sArRegion->DoEachVisibleRegion((uint) (obj->GetPositionX() / g_nRegionSize),
                                   (uint) (obj->GetPositionY() / g_nRegionSize),
                                   obj->GetLayer(),
                                   [&leavePct,&obj](ArRegion *lbRegion) {
                                       lbRegion->DoEachClient([&leavePct,&obj](WorldObject *lbPlayer) {
                                           if (lbPlayer != nullptr && lbPlayer->IsInWorld() && lbPlayer->GetHandle() != obj->GetHandle()) {
                                               dynamic_cast<Player *>(lbPlayer)->SendPacket(leavePct);
                                           }
                                       });
                                   });
    // Finally, remove object from map
    if (region != nullptr)
        region->RemoveObject(obj);
}

void World::step(WorldObject *obj, uint tm)
{
    Position oldPos = obj->GetPosition();
    obj->Step(tm);
    Position newPos = obj->GetPosition();

    onMoveObject(obj, oldPos, newPos);
    obj->lastStepTime = tm;
}

bool World::onSetMove(WorldObject *pObject, Position curPos, Position lastpos)
{
    return true;
}

void World::Update(uint diff)
{
    for (auto& session : m_sessions) {
        if (session.second != nullptr && session.second->GetPlayer() != nullptr) {
            session.second->GetPlayer()->Update(diff);
        } else {
            m_sessions.erase(session.first);
        }
    }
    for(auto& ro : m_vRespawnList) {
        ro->Update(diff);
        m_vRespawnList.erase(std::remove(m_vRespawnList.begin(), m_vRespawnList.end(), ro), m_vRespawnList.end());
    }

    sMemoryPool->Update(diff);
}

void World::Broadcast(uint rx1, uint ry1, uint rx2, uint ry2, uint8 layer, XPacket packet)
{
    sArRegion->DoEachVisibleRegion(rx1, ry1, rx2, ry2, layer, [&packet](ArRegion* rgn) {
        rgn->DoEachClient([&packet](WorldObject* obj) {
            dynamic_cast<Player*>(obj)->SendPacket(packet);
        });
    });
}

void World::Broadcast(uint rx, uint ry, uint8 layer, XPacket packet)
{
    sArRegion->DoEachVisibleRegion(rx, ry, layer, [&packet](ArRegion* rgn) {
       rgn->DoEachClient([&packet](WorldObject* obj) {
           dynamic_cast<Player*>(obj)->SendPacket(packet);
       });
    });
}

void World::AddSummonToWorld(Summon *pSummon)
{
    pSummon->SetFlag(UNIT_FIELD_STATUS, StatusFlags::FirstEnter);
    //pSummon->AddToWorld();
    AddObjectToWorld(pSummon);
    //pSummon->m_bIsSummoned = true;
    pSummon->RemoveFlag(UNIT_FIELD_STATUS, StatusFlags::FirstEnter);
}

void World::WarpBegin(Player * pPlayer)
{
	if(pPlayer->IsInWorld())
		RemoveObjectFromWorld(pPlayer);
	if(pPlayer->m_pMainSummon != nullptr)
		RemoveObjectFromWorld(pPlayer->m_pMainSummon);
	// Same for sub summon
	// same for pet
}

void World::WarpEnd(Player *pPlayer, Position pPosition, uint8_t layer)
{
	if(pPlayer == nullptr)
		return;

	uint ct = GetArTime();

	if(layer != pPlayer->GetLayer()) {
		// TODO Layer management
	}
	pPlayer->SetCurrentXY(pPosition.GetPositionX(), pPosition.GetPositionY());
	pPlayer->StopMove();

	Messages::SendWarpMessage(pPlayer);
	if(pPlayer->m_pMainSummon != nullptr)
		WarpEndSummon(pPlayer, pPosition, layer, pPlayer->m_pMainSummon, 0);

	((Unit*)pPlayer)->SetFlag(UNIT_FIELD_STATUS, StatusFlags::FirstEnter);
	AddObjectToWorld(pPlayer);
	pPlayer->RemoveFlag(UNIT_FIELD_STATUS, StatusFlags::FirstEnter);
	Position pos = pPlayer->GetCurrentPosition(ct);
	// Set Move
	Messages::SendPropertyMessage(pPlayer,pPlayer, "channel", 0);
	pPlayer->ChangeLocation(pPlayer->GetPositionX(), pPlayer->GetPositionY(), false, true);
	pPlayer->Save(true);
}

void World::WarpEndSummon(Player *pPlayer , Position pos, uint8_t layer, Summon *pSummon, bool)
{
	uint ct = GetArTime();
	if(pSummon == nullptr)
		return;
	pSummon->SetCurrentXY(pos.GetPositionX(), pos.GetPositionY());
	pSummon->SetLayer(layer);
	pSummon->StopMove();
	pSummon->SetFlag(UNIT_FIELD_STATUS, StatusFlags::FirstEnter);
	pSummon->AddNoise(rand32(), rand32(), 35);
	AddObjectToWorld(pSummon);
	pSummon->RemoveFlag(UNIT_FIELD_STATUS, StatusFlags::FirstEnter);
	auto position = pSummon->GetCurrentPosition(ct);
	// Set move
}

void World::AddMonsterToWorld(Monster *pMonster)
{
    pMonster->SetFlag(UNIT_FIELD_STATUS, StatusFlags::FirstEnter);
    AddObjectToWorld(pMonster);
    pMonster->RemoveFlag(UNIT_FIELD_STATUS, StatusFlags::FirstEnter);
}

void World::KickAll()
{
    for(auto& itr : m_sessions) {
        itr.second->KickPlayer();
    }
}

void World::addEXP(Unit *pCorpse, Player *pPlayer, float exp, float jp)
{
    float fJP = 0;
    if(pPlayer->GetHealth() != 0) {
        float fJP = jp;
        // remove some immorality points here
        if(pPlayer->GetInt32Value(UNIT_FIELD_IP) > 0) {
            if(pCorpse->GetLevel() >= pPlayer->GetLevel()) {
                float fIPDec = -1.0f;
            }
        }
    }

    int levelDiff = pPlayer->GetLevel() - pCorpse->GetLevel();
    if(levelDiff > 0) {
        exp = (1.0f - (float)levelDiff * 0.05f) * exp;
        fJP = (1.0f - (float)levelDiff * 0.05f) * jp;
    }

    uint ct = GetArTime();
    Position posPlayer = pPlayer->GetCurrentPosition(ct);
    Position posCorpse = pCorpse->GetCurrentPosition(ct);
    if(posCorpse.GetExactDist2d(&posPlayer) <= 500.0f) {
        if(exp < 1.0f)
            exp = 1.0f;
        if(fJP < 0.0f)
            fJP = 0.0f;

        auto mob = dynamic_cast<Monster*>(pCorpse);
        //if(pCorpse->GetSubType() == ST_Mob && mob.m_h)
        /// TAMING
    }

    pPlayer->AddEXP(exp, jp, false);
}

void World::BroadcastStatusMessage(Unit *unit)
{
    if(unit == nullptr)
        return;
    XPacket packet(TS_SC_STATUS_CHANGE);
    packet << unit->GetHandle();
    packet << 0; // Get Status Message

    sArRegion->DoEachVisibleRegion((uint)(unit->GetPositionX() / g_nRegionSize),
                                   (uint)(unit->GetPositionY() / g_nRegionSize),
                                   unit->GetLayer(),
                                   [&packet](ArRegion* rgn) {
                                       rgn->DoEachClient([&packet](WorldObject* obj) {
                                           dynamic_cast<Player*>(obj)->SendPacket(packet);
                                       });
                                   });
}

void World::MonsterDropItemToWorld(Unit *pUnit, Item *pItem)
{
    if(pUnit == nullptr || pItem == nullptr)
        return;
    XPacket itemPct(TS_SC_ITEM_DROP_INFO);
    itemPct << pUnit->GetHandle();
    itemPct << pItem->GetHandle();
    Broadcast((uint)(pItem->GetPositionX() / g_nRegionSize), (uint)(pItem->GetPositionY() / g_nRegionSize), pItem->GetLayer(), itemPct);
    AddItemToWorld(pItem);
}

void World::AddItemToWorld(Item *pItem)
{
    AddObjectToWorld(pItem);
    pItem->m_nDropTime = GetArTime();
}
// TODO: ItemCollector
bool World::RemoveItemFromWorld(Item *pItem)
{
    RemoveObjectFromWorld(pItem);
    pItem->m_nDropTime = 0;
    return true;
}

uint World::procAddItem(Player *pClient, Item *pItem, bool bIsPartyProcess)
{
    uint item_handle = 0;
    int code = pItem->m_Instance.Code;
    if(code != 0 || (pClient->GetGold() + pItem->m_Instance.nCount) < MAX_GOLD_FOR_INVENTORY) {
        pItem->m_Instance.nIdx = 0;
        pItem->m_bIsNeedUpdateToDB = true;
        pClient->PushItem(pItem, pItem->m_Instance.nCount, false);
        if(pItem != nullptr)
            item_handle = pItem->GetHandle();
    }
    return item_handle;
}

/*
 bool __usercall checkDrop@<al>(StructCreature *pKiller@<esi>, int code, int percentage, float fDropRatePenalty, float fPCBangDropRateBonus)
{
  float fMod; // [sp+4h] [bp-8h]@1
  float fCreatureCardMod; // [sp+8h] [bp-4h]@1
  float fItemDropRate; // [sp+14h] [bp+8h]@4

  fCreatureCardMod = 1.0;
  fMod = (double)pKiller->vfptr[10].IsDeleteable((ArSchedulerObject *)pKiller) * 0.009999999776482582 + 1.0;
  if ( code > 0 && StructItem::GetItemBase(code)->nGroup == 13 )
    fCreatureCardMod = pKiller->m_fCreatureCardChance;
  fItemDropRate = GameRule::fItemDropRate;
  return (double)percentage * fMod * fItemDropRate * fDropRatePenalty * fPCBangDropRateBonus * fCreatureCardMod >= (double)XRandom(1u, 0x5F5E100u);
}
 */

bool World::checkDrop(Unit *pKiller, int code, int percentage, float fDropRatePenalty, float fPCBangDropRateBonus)
{
    float fMod;
    float fCreatureCardMod;

    fCreatureCardMod = 1.0f;
    fMod = pKiller->GetItemChance() * 0.01f + 1.0f;
    if (code > 0)
    {
        if (sObjectMgr->GetItemBase(code)->group == 13)
            fCreatureCardMod = /*pKiller->m_fCreatureCardChance;*/ 1.0f; // actual wtf?
        /*if (sObjectMgr->GetItemBase(code)->flaglist[FLAG_QUEST] != 0)
            fDropRatePenalty = 1.0f;*/
    }

    return (percentage * 1) * fMod * GameRule::GetItemDropRate() * fDropRatePenalty * fPCBangDropRateBonus * fCreatureCardMod >= irand(1, 0x5F5E100u);
}
