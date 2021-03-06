#ifndef PROJECT_SUMMON_H
#define PROJECT_SUMMON_H

#include "Common.h"
#include "Unit.h"
#include "SummonBase.h"

class Player;
class Summon : public Unit
{
    public:
        friend class Player;
        static Summon *AllocSummon(Player *, uint);
        explicit Summon(uint, uint);
        ~Summon();

        static void DB_InsertSummon(Player *, Summon *);
        static void DB_UpdateSummon(Player *, Summon *);
        static void EnterPacket(XPacket &, Summon *, Player *pPlayer);

        CreatureStat *GetBaseStat() const override;

        uint GetCreatureGroup() const override { return 9; }

        void OnAfterReadSummon();
        uint32_t GetCardHandle();

        bool IsSummon() const override { return true; }
        bool IsAlly(const Unit *pTarget) override;

        int32_t GetSummonCode();
        float GetFCM() const override;

        Player *GetMaster() const { return m_pMaster; }

        void Update(uint/*diff*/) override;

        bool TranslateWearPosition(ItemWearType &pos, Item *item, std::vector<int> &ItemList) override;

        float GetSize() const override;
        float GetScale() const override;

        void SetSummonInfo(int);
        bool DoEvolution();

        int m_nSummonInfo{ };
        int m_nCardUID{ };
        int m_nTransform{ };
        uint8_t m_cSlotIdx{ };
        Item    *m_pItem{nullptr};
    protected:
        void onCantAttack(uint handle, uint t) override;
        void onBeforeCalculateStat() override;
        void applyJobLevelBonus() override;
        uint16 putonItem(ItemWearType pos, Item *pItem) override;
        uint16 putoffItem(ItemWearType pos) override;
        void onRegisterSkill(int64 skillUID, int skill_id, int prev_level, int skill_level) override;
        void processWalk(uint t);
        void onExpChange() override;
        //void onAfterApplyStat() override;
        void applyPassiveSkillEffect() override;
        void applyPassiveSkillAmplifyEffect(Skill *) override;
        void onModifyStatAndAttribute() override;
        void onItemWearEffect(Item *pItem, bool bIsBaseVar, int type, float var1, float var2, float fRatio) override;
        //void onApplyStat
        void onCompleteCalculateStat() override;
    private:
        SummonResourceTemplate *m_tSummonBase{nullptr};
        Player *m_pMaster{nullptr};

        int m_nAccountID{};
};


#endif // PROJECT_SUMMON_H
