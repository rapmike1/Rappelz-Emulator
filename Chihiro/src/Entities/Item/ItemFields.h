//
// Created by xijezu on 17.12.17.
//

#ifndef PROJECT_ITEMFIELDS_H
#define PROJECT_ITEMFIELDS_H

#define MAX_GOLD_FOR_INVENTORY  100000000000
#define MAX_GOLD_FOR_STORAGE    100000000000
#define MAX_OPTION_NUMBER 4
#define MAX_ITEM_WEAR 24
#define MAX_COOLTIME_GROUP 40
#define MAX_SOCKET_NUMBER 4
#define MAX_ITEM_NAME_LENGTH 32

struct ItemPickupOrder {
    uint hPlayer[3];
    int  nPartyID[3];
};

enum ElementalType {
    TYPE_NONE  = 0,
    TYPE_FIRE  = 1,
    TYPE_WATER = 2,
    TYPE_WIND  = 3,
    TYPE_EARTH = 4,
    TYPE_LIGHT = 5,
    TYPE_DARK  = 6,
    TYPE_COUNT = 7
};

enum FlagBits : uint
{
    ITEM_FLAG_CARD            = 0x01,
    ITEM_FLAG_FULL            = 0x02,
    ITEM_FLAG_INSERTED        = 0x04,
    ITEM_FLAG_FAILED          = 0x08,
    ITEM_FLAG_EVENT           = 0x10,
    ITEM_FLAG_CONTAIN_PET     = 0x20,
    ITEM_FLAG_TAMING          = 0x20000000,
    ITEM_FLAG_NON_CHAOS_STONE = 0x40000000,
    ITEM_FLAG_SUMMON          = 0x80000000,
};

/// \brief This is actually the idx for the ItemBase::flaglist
/// Not the retail bitset
enum ItemFlag : int
{
    FLAG_CASHITEM   = 0,
    FLAG_WEAR       = 1,
    FLAG_USE        = 2,
    FLAG_TARGET_USE = 3,
    FLAG_DUPLICATE  = 4,
    FLAG_DROP       = 5,
    FLAG_TRADE      = 6,
    FLAG_SELL       = 7,
    FLAG_STORAGE    = 8,
    FLAG_OVERWEIGHT = 9,
    FLAG_RIDING     = 10,
    FLAG_MOVE       = 11,
    FLAG_SIT        = 12,
    FLAG_ENHANCE    = 13,
    FLAG_QUEST      = 14,
    FLAG_RAID       = 15,
    FLAG_SECROUTE   = 16,
    FLAG_EVENTMAP   = 17,
    FLAG_HUNTAHOLIC = 18
};

enum GenerateCode : int
{
    BY_MONSTER         = 0,
    BY_MARKET          = 1,
    BY_QUEST           = 2,
    BY_SCRIPT          = 3,
    BY_MIX             = 4,
    BY_GM              = 5,
    BY_BASIC           = 6,
    BY_TRADE           = 7,
    BY_DIVIDE          = 8,
    BY_ITEM            = 10,
    BY_FIELD_PROP      = 11,
    BY_AUCTION         = 12,
    BY_SHOVELING       = 13,
    BY_HUNTAHOLIC      = 14,
    BY_DONATION_REWARD = 15,
    BY_UNKNOWN         = 126
};

enum ItemWearType : int16
{
    WEAR_CANTWEAR       = -1,
    WEAR_NONE           = -1,
    WEAR_WEAPON         = 0,
    WEAR_SHIELD         = 1,
    WEAR_ARMOR          = 2,
    WEAR_HELM           = 3,
    WEAR_GLOVE          = 4,
    WEAR_BOOTS          = 5,
    WEAR_BELT           = 6,
    WEAR_MANTLE         = 7,
    WEAR_ARMULET        = 8,
    WEAR_RING           = 9,
    WEAR_SECOND_RING    = 10,
    WEAR_EAR            = 11,
    WEAR_FACE           = 12,
    WEAR_HAIR           = 13,
    WEAR_DECO_WEAPON    = 14,
    WEAR_DECO_SHIELD    = 15,
    WEAR_DECO_ARMOR     = 16,
    WEAR_DECO_HELM      = 17,
    WEAR_DECO_GLOVE     = 18,
    WEAR_DECO_BOOTS     = 19,
    WEAR_DECO_MANTLE    = 20,
    WEAR_DECO_SHOULDER  = 21,
    WEAR_RIDE_ITEM      = 22,
    WEAR_BAG_SLOT       = 23,
    WEAR_TWOFINGER_RING = 94,
    WEAR_TWOHAND        = 99,
    WEAR_SKILL          = 100,
    WEAR_RIGHTHAND      = 0,
    WEAR_LEFTHAND       = 1,
    WEAR_BULLET         = 1,
};

enum ItemClass : int
{
    CLASS_ETC                = 0,
    CLASS_DOUBLE_AXE         = 95,
    CLASS_DOUBLE_SWORD       = 96,
    CLASS_DOUBLE_DAGGER      = 98,
    CLASS_EVERY_WEAPON       = 99,
    CLASS_ETCWEAPON          = 100,
    CLASS_ONEHAND_SWORD      = 101,
    CLASS_TWOHAND_SWORD      = 102,
    CLASS_DAGGER             = 103,
    CLASS_TWOHAND_SPEAR      = 104,
    CLASS_TWOHAND_AXE        = 105,
    CLASS_ONEHAND_MACE       = 106,
    CLASS_TWOHAND_MACE       = 107,
    CLASS_HEAVY_BOW          = 108,
    CLASS_LIGHT_BOW          = 109,
    CLASS_CROSSBOW           = 110,
    CLASS_ONEHAND_STAFF      = 111,
    CLASS_TWOHAND_STAFF      = 112,
    CLASS_ONEHAND_AXE        = 113,
    CLASS_ARMOR              = 200,
    CLASS_FIGHTER_ARMOR      = 201,
    CLASS_HUNTER_ARMOR       = 202,
    CLASS_MAGICIAN_ARMOR     = 203,
    CLASS_SUMMONER_ARMOR     = 204,
    CLASS_SHIELD             = 210,
    CLASS_HELM               = 220,
    CLASS_BOOTS              = 230,
    CLASS_GLOVE              = 240,
    CLASS_BELT               = 250,
    CLASS_MANTLE             = 260,
    CLASS_ETC_ACCESSORY      = 300,
    CLASS_RING               = 301,
    CLASS_EARRING            = 302,
    CLASS_ARMULET            = 303,
    CLASS_EYEGLASS           = 304,
    CLASS_MASK               = 305,
    CLASS_CUBE               = 306,
    CLASS_BOOST_CHIP         = 400,
    CLASS_SOULSTONE          = 401,
    CLASS_DECO_SHIELD        = 601,
    CLASS_DECO_ARMOR         = 602,
    CLASS_DECO_HELM          = 603,
    CLASS_DECO_GLOVE         = 604,
    CLASS_DECO_BOOTS         = 605,
    CLASS_DECO_MALTLE        = 606,
    CLASS_DECO_SHOULDER      = 607,
    CLASS_DECO_HAIR          = 608,
    CLASS_DECO_ONEHAND_SWORD = 609,
    CLASS_DECO_TWOHAND_SWORD = 610,
    CLASS_DECO_DAGGER        = 611,
    CLASS_DECO_TWOHAND_SPEAR = 612,
    CLASS_DECO_TWOHAND_AXE   = 613,
    CLASS_DECO_ONEHAND_MACE  = 614,
    CLASS_DECO_TWOHAND_MACE  = 615,
    CLASS_DECO_HEAVY_BOW     = 616,
    CLASS_DECO_LIGHT_BOW     = 617,
    CLASS_DECO_CROSSBOW      = 618,
    CLASS_DECO_ONEHAND_STAFF = 619,
    CLASS_DECO_TWOHAND_STAFF = 620,
    CLASS_DECO_ONEHAND_AXE   = 621,
};

enum ItemType : int {
    TYPE_ETC       = 0,
    TYPE_ARMOR     = 1,
    TYPE_CARD      = 2,
    TYPE_SUPPLY    = 3,
    TYPE_CUBE      = 4,
    TYPE_CHARM     = 5,
    TYPE_USE       = 6,
    TYPE_SOULSTONE = 7,
    TYPE_USE_CARD   = 8,
};

enum ItemGroup : int
{
    GROUP_ETC               = 0,
    GROUP_WEAPON            = 1,
    GROUP_ARMOR             = 2,
    GROUP_SHIELD            = 3,
    GROUP_HELM              = 4,
    GROUP_GLOVE             = 5,
    GROUP_BOOTS             = 6,
    GROUP_BELT              = 7,
    GROUP_MANTLE            = 8,
    GROUP_ACCESSORY         = 9,
    GROUP_SKILLCARD         = 10,
    GROUP_ITEMCARD          = 11,
    GROUP_SPELLCARD         = 12,
    GROUP_SUMMONCARD        = 13,
    GROUP_FACE              = 15,
    GROUP_UNDERWEAR         = 16,
    GROUP_BAG               = 17,
    GROUP_PET_CAGE          = 18,
    GROUP_STRIKE_CUBE       = 21,
    GROUP_DEFENCE_CUBE      = 22,
    GROUP_SKILL_CUBE        = 23,
    GROUP_RESTORATION_CUBE  = 24,
    GROUP_SOULSTONE         = 93,
    GROUP_BULLET            = 98,
    GROUP_CONSUMABLE        = 99,
    GROUP_NPC_FACE          = 100,
    GROUP_DECO              = 110,
    GROUP_RIDING            = 120,
};

enum LIMIT_FLAG : int
{
    LIMIT_DEVA     = 0x4,
    LIMIT_ASURA    = 0x8,
    LIMIT_GAIA     = 0x10,
    LIMIT_FIGHTER  = 0x400,
    LIMIT_HUNTER   = 0x800,
    LIMIT_MAGICIAN = 0x1000,
    LIMIT_SUMMONER = 0x2000
};

#endif //PROJECT_ITEMFIELDS_H
