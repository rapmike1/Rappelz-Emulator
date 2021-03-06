#ifndef PROJECT_WORLDLOCATION_H
#define PROJECT_WORLDLOCATION_H

#include "Common.h"
#include "SharedMutex.h"

class Player;

enum WL_Time : int {
    Dawn    = 0,
    Daytime = 1,
    Evening = 2,
    Night   = 3,
    WLT_Max = 4,
};

enum WL_Weather : int {
    Clear     = 0,
    LightRain = 1,
    Rain      = 2,
    HeavyRain = 3,
    LightSnow = 4,
    Snow      = 5,
    HeavySnow = 6,
    WLW_Max   = 7
};

enum WL_Type : int {
    WT_Etc               = 0,
    Town              = 1,
    WL_Field          = 2,
    NonPkField        = 3,
    Dungeon           = 4,
    BattleField       = 5,
    EventMap          = 7,
    HuntaholicLobby   = 8,
    HuntaholicDungeon = 9,
    FleaMarket        = 10,
};

enum WL_SpecLocId : int {
    Abyss           = 110900,
    SecRoute1       = 130100,
    SecRoute2       = 130101,
    SecRouteAuction = 130107,
};

class WorldLocation {
public:
    WorldLocation() = default;
    WorldLocation(const WorldLocation& src);
    ~WorldLocation() = default;

    uint                  idx{ };
    uint8_t               location_type{ };
    uint8_t               weather_ratio[7][4]{ };
    uint8_t               current_weather{ };
    uint                  weather_change_time{ };
    uint                  last_changed_time{ };
    int                   shovelable_item{ };
    std::vector<Player *> m_vIncludeClient{ };
};

class WorldLocationManager {
public:
    WorldLocationManager() = default;
    ~WorldLocationManager() = default;

    WorldLocation *AddToLocation(uint idx, Player *player);
    bool RemoveFromLocation(Player *player);
    void SendWeatherInfo(uint idx, Player *player);
    int GetShovelableItem(uint idx);
    uint GetShovelableMonster(uint idx);
    void RegisterWorldLocation(uint idx, uint8_t location_type, uint time_id, uint weather_id, uint8_t ratio, uint weather_change_time, int shovelable_item);
    void RegisterMonsterLocation(uint idx, uint monster_id);

private:
    MX_SHARED_MUTEX i_lock;
    std::vector<WorldLocation> m_vWorldLocation{ };
    UNORDERED_MAP<uint, std::vector<uint>> m_hsMonsterID{ };
};
#define sWorldLocationMgr ACE_Singleton<WorldLocationManager, ACE_Thread_Mutex>::instance()
#endif // PROJECT_WORLDLOCATION_H
