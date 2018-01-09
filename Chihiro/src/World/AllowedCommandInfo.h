#ifndef PROJECT_ALLOWEDCOMMANDINFO_H
#define PROJECT_ALLOWEDCOMMANDINFO_H

#include "Common.h"
#include "Player.h"

class AllowedCommandInfo {
public:
    AllowedCommandInfo() = default;
    ~AllowedCommandInfo() = default;

    void Run(Player* pClient, const std::string& szMessage);
    void onCheatPosition(Player* pClient, const std::string&);
    void onRunScript(Player *pClient, const std::string& pScript);
    void onCheatSitdown(Player* pClient, const std::string&);
    void onBattleMode(Player* pClient, const std::string&);
    void onCheatNotice(Player* pClient, const std::string&);
    void onCheatParty(Player* pClient, const std::string&);
};

#define sAllowedCommandInfo ACE_Singleton<AllowedCommandInfo, ACE_Null_Mutex>::instance()

#endif // PROJECT_ALLOWEDCOMMANDINFO_H