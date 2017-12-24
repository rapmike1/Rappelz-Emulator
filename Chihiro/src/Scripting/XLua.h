#ifndef PROJECT_XLUA_H
#define PROJECT_XLUA_H

#include "Common.h"
#include "sol.hpp"
#include "Player.h"

class XLua {
public:
    XLua();
    bool InitializeLua();
    ~XLua() = default;

    bool RunString(Unit *, std::string, std::string &);
    bool RunString(Unit *, std::string);
    bool RunString(std::string);

private:
    template <typename T>
    sol::object return_object( T&& value ) {
        sol::stack::push(m_pState.lua_state(), std::forward<T>(value));
        sol::object r = sol::stack::pop<sol::object>(m_pState.lua_state());
        return r;
    }

    // Monster
    void SCRIPT_SetWayPointType(int, int);
    void SCRIPT_AddWayPoint(int, int, int);
    void SCRIPT_RespawnRareMob(int, int, int, int, int, int, int, int);
    void SCRIPT_RespawnRareMob2(int, int, int, int, int, int, int);
    void SCRIPT_RespawnRoamingMob(int, int, int, int, int);
    void SCRIPT_RespawnGuardian(int, int, int, int, int, int, int, int);

    // NPC
    int SCRIPT_GetNPCID();
    void SCRIPT_DialogTitle(std::string);
    void SCRIPT_DialogText(std::string);
    void SCRIPT_DialogMenu(std::string, std::string);
    void SCRIPT_DialogShow();

    // Values
    int SCRIPT_GetLocalFlag();
    int SCRIPT_GetServerCategory();

    // Blacksmith
    int SCRIPT_GetWearItemHandle(int);
    int SCRIPT_GetItemLevel(uint);
    int SCRIPT_GetItemEnhance(uint);
    int SCRIPT_SetItemLevel(uint, int);
    int SCRIPT_GetItemPrice(uint);
    int SCRIPT_GetItemRank(uint);
    int SCRIPT_GetItemNameID(int);
    int SCRIPT_GetItemCode(uint);
    int SCRIPT_UpdateGoldChaos();
    void SCRIPT_SavePlayer();

    sol::object SCRIPT_GetValue(std::string);
    void SCRIPT_SetValue(std::string,int64);

    std::string SCRIPT_GetFlag(std::string);
    void SCRIPT_SetFlag(sol::variadic_args args);

    sol::object SCRIPT_GetEnv(std::string);
    void SCRIPT_ShowMarket(std::string);

    int SCRIPT_GetProperChannelNum(int) { return 0; }
    int SCRIPT_GetLayerOfChannel(int,int) { return 0; }

    std::string SCRIPT_Conv(sol::variadic_args);
    void SCRIPT_Message(std::string);
    void SCRIPT_SetCurrentLocationID(int);

    void SCRIPT_Warp(sol::variadic_args);

    Unit *m_pUnit{nullptr};
    sol::state m_pState{ };
};

#define sScriptingMgr ACE_Singleton<XLua, ACE_Thread_Mutex>::instance()
#endif // PROJECT_XLUA_H
