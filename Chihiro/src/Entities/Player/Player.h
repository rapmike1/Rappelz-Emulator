#ifndef _PLAYER_H_
#define _PLAYER_H_

#include "Common.h"
#include "Entities/Unit/Unit.h"
#include "Item/Item.h"
#include "Summon/Summon.h"
#include "WorldLocation.h"
#include "TimeSync.h"
class WorldSession;


class Player : public Unit {
public:
    explicit Player(uint32);
    ~Player() override;

    static void EnterPacket(XPacket &, Player *);

    int32 GetPermission()
    { return GetInt32Value(UNIT_FIELD_PERMISSION); }

    int32 GetGold()
    { return GetInt32Value(UNIT_FIELD_GOLD); }

    int32 GetPartyID()
    { return GetInt32Value(UNIT_FIELD_PARTY_ID); }

    int32 GetGuildID()
    { return GetInt32Value(UNIT_FIELD_GUILD_ID); }

    uint GetJobDepth();

    // Database stuff
    bool ReadCharacter(std::string, int);
    bool ReadItemList(int);
    bool ReadSummonList(int);
    bool ReadEquipItem();
    bool ReadSkillList(int);

    // Warping
    void PendWarp(int x, int y, uint8_t layer);

    std::string GetFlag(std::string flag)
    { return m_lFlagList[flag]; }

    void SetFlag(std::string key, std::string value)
    { m_lFlagList[key] = value; }

    // Network
    void SendPropertyMessage(std::string key, std::string value);
    void SendPropertyMessage(std::string key, int64 value);

    // Summon
    void LogoutNow(int callerIdx);
    void RemoveAllSummonFromWorld();

    void SendLoginProperties();
    void SendGoldChaosMessage();
    void SendJobInfo();
    void SendWearInfo();

    Summon *GetSummon(int);
    Summon *GetSummonByHandle(uint);
    void AddSummon(Summon*,bool);

    // Database
    void Update(uint diff) override;
    void OnUpdate() override;
    void Save(bool);

    // Item relevant
    Item *FindItemByCode(int);
    Item *FindItemBySID(uint64_t);
    Item *FindItemByHandle(uint32_t);
    void PushItem(Item *, int, bool);
    void PopItem(Item*,bool);

    bool Erase(Item*,uint64,bool);
    void SetItemCount(Item* pItem, uint64 nc, bool bSkipUpdateToDB);
    uint16_t putonItem(ItemWearType, Item *) override;
    uint16_t putoffItem(ItemWearType) override;
    void SendItemWearInfoMessage(Item* item, Unit *u);
    void ChangeLocation(float x, float y, bool bByRequest, bool bBroadcast);

    // Dialog Relevant
    void SetLastContact(std::string, uint32_t);
    void SetLastContact(std::string, std::string);
    std::string GetLastContactStr(std::string);
    uint32_t GetLastContactLong(std::string);
    void SetDialogTitle(std::string, int);
    void SetDialogText(std::string);
    void AddDialogMenu(std::string, std::string);
    void ClearDialogMenu();

    bool HasDialog()
    { return m_szDialogTitle.length() > 0; }

    void ShowDialog();
    bool IsValidTrigger(std::string);
    void onChangeProperty(std::string, int);

    bool IsFixedDialogTrigger(std::string szTrigger)
    { return false; }

    void DoSummon(Summon* pSummon, Position pPosition);
    void DoUnSummon(Summon* pSummon);

    bool IsHunter();
    bool IsFighter();
    bool IsMagician();
    bool IsSummoner();

    bool TranslateWearPosition(ItemWearType& pos, Item* item, std::vector<int>* ItemList) override;

    ushort_t ChangeGold(long);

    void SendPacket(XPacket& pPacket);

    WorldSession& GetSession() const
    { return *m_session; }

    void SetClientInfo(std::string value)
    { m_szClientInfo = value; }

    void SetSession(WorldSession *session)
    { m_session = session; }

    void applyJobLevelBonus() override;

    UNORDERED_MAP<long, Item *>             m_lInventory;
    Item                                    *m_aBindSummonCard[6]{nullptr};
    WorldLocation *m_WorldLocation{nullptr};
    int m_nWorldLocationId{0};
    TimeSynch m_TS{200,2,10};

    Summon* m_pMainSummon{nullptr};

protected:
    void onRegisterSkill(int skillUID, int skill_id, int prev_level, int skill_level) override;
    void onExpChange() override;
    void onCantAttack(uint target, uint t) override;

private:
    WorldSession *m_session{nullptr};
    std::string  m_szAccount;

    UNORDERED_MAP<std::string, std::string> m_lFlagList{ };
    UNORDERED_MAP<std::string, std::string> m_hsContact{ };

    std::vector<Summon *> m_vSummonList{nullptr};

    uint m_nLastSaveTime{ }, m_nLastCantAttackTime{ };

    // Dialog stuff
    int         m_nDialogType{ };
    bool        m_bNonNPCDialog{false};
    std::string m_szDialogTitle{ };
    std::string m_szDialogText{ };
    std::string m_szDialogMenu{ };
    std::string m_szSpecialDialogMenu{ };
    std::string m_szClientInfo{ };
};

#endif // _PLAYER_H_
