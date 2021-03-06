/*
  *  Copyright (C) 2018 Xijezu <http://xijezu.com/>
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

#include "Item.h"
#include "MixManager.h"
#include "Messages.h"
#include "Player.h"
#include "GameRule.h"
#include "MemPool.h"
#include "ObjectMgr.h"

void MixManager::RegisterEnhanceInfo(const EnhanceInfo &info)
{
    for(auto& ei : m_vEnhanceInfo)
    {
        if(ei.nSID == info.nSID)
            return;
    }
    m_vEnhanceInfo.emplace_back(info);
}

void MixManager::RegisterMixInfo(const MixBase &info)
{
    for(auto& mi : m_vMixInfo)
    {
        if(mi.id == info.id)
            return;
    }
    m_vMixInfo.emplace_back(info);
}


bool MixManager::EnhanceItem(MixBase *pMixInfo, Player *pPlayer, Item *pMainMaterial, int nSubMaterialCountItem, std::vector<Item*> &pSubMaterial, std::vector<uint16> &pCountList)
{
    if(pMixInfo == nullptr || pPlayer == nullptr || pMainMaterial == nullptr)
        return false;

    int nCurrentEnhance = pMainMaterial->m_Instance.nEnhance;
    auto pInfo = getenhanceInfo(pMixInfo->value[0]);
    if(pInfo == nullptr)
    {
        Messages::SendResult(pPlayer, 256, TS_RESULT_INVALID_ARGUMENT, 0);
        return false;
    }

    Item* pCube{nullptr};
    for(auto& ti : pSubMaterial)
    {
        if(ti != nullptr && ti->m_Instance.Code == pInfo->nNeedItemCode)
        {
            pCube = ti;
            break;
        }
    }
    Item* pPowder{nullptr};
    if(pMixInfo->type == MIX_TYPE::MIX_ENHANCE_WITHOUT_FAIL)
    {
        if(pSubMaterial[0] != nullptr && pSubMaterial[0]->m_pItemBase->type == TYPE_CUBE)
        {
            pPowder = pSubMaterial[1];
        }
        else
        {
            pPowder = pSubMaterial[0];
            pCube = pSubMaterial[1];
        }
    }
    if(pCube == nullptr
       || pCube->m_Instance.Code != pInfo->nNeedItemCode
       || (pMixInfo->type == MIX_TYPE::MIX_ENHANCE_WITHOUT_FAIL && pPowder == nullptr)
        /*|| pInfo->nMaxEnhance <= pMainMaterial->m_Instance.nEnhance*/)
    {
        Messages::SendResult(pPlayer, 256, TS_RESULT_INVALID_ARGUMENT, 0);
        return false;
    }

    // Maybe do logging here like retail?
    pPlayer->EraseItem(pCube, 1);
    if(pMixInfo->type == MIX_TYPE::MIX_ENHANCE_WITHOUT_FAIL && pPowder != nullptr)
        pPlayer->EraseItem(pPowder, 1);

    int itemEnhance{1};
    /*if(pMixInfo->type == MIX_TYPE::MIX_ENHANCE)
        itemEnhance = irand(pMixInfo->value[1], pMixInfo->value[2]);*/

    auto nRandom = (uint)(pInfo->fPercentage[nCurrentEnhance] * 100000.0f);

    bool bResult = false;
    if(irand(0, 100000) > nRandom)
    {
        // failed
        if(pMixInfo->type != 103)
        {
            procEnhanceFail(pPlayer, pMainMaterial, pInfo->nFailResult);
            Messages::SendMixResult(pPlayer, nullptr);
        }
        else
        {
            if(GameRule::nEnhanceFailType == 0)
            {
                pMainMaterial->m_Instance.nEnhance = ((irand(nCurrentEnhance * pMixInfo->value[1], nCurrentEnhance * pMixInfo->value[2]) / 1000) + 5) / 10;
            }
            else if(GameRule::nEnhanceFailType == 1)
            {
                pMainMaterial->m_Instance.nEnhance = nCurrentEnhance - 1;
            }
            Messages::SendItemMessage(pPlayer, pMainMaterial);
            Messages::SendMixResult(pPlayer, nullptr);
        }
    }
    else
    {
        pMainMaterial->m_Instance.nEnhance = nCurrentEnhance + itemEnhance;
        pMainMaterial->m_bIsNeedUpdateToDB = true;
        Messages::SendItemMessage(pPlayer, pMainMaterial);
        std::vector<uint> handles{};
        handles.emplace_back(pMainMaterial->GetHandle());
        Messages::SendMixResult(pPlayer, &handles);
        bResult = true;
    }
    pMainMaterial->DBUpdate();
    return bResult;
}

bool MixManager::EnhanceSkillCard(MixBase *pMixInfo, Player *pPlayer, Item *pMainMaterial, int nSubMaterialCount,std::vector<Item*> &pSubItem, std::vector<uint16> &pCountList)
{
    return false;
}

bool MixManager::MixItem(MixBase *pMixInfo, Player *pPlayer, Item *pMainMaterial, int nSubMaterialCountItem, std::vector<Item *> &pSubItem, std::vector<uint16> &pCountList)
{
    if(pMixInfo == nullptr || pPlayer == nullptr || pMainMaterial == nullptr)
        return false;

    for(int i = 0; i < nSubMaterialCountItem; ++i)
    {
        pPlayer->EraseItem(pSubItem[i], pCountList[i]);
    }

    pMainMaterial->m_Instance.Flag |= pMixInfo->value[2];
    Messages::SendItemMessage(pPlayer, pMainMaterial);
    std::vector<uint> handles{};
    handles.emplace_back(pMainMaterial->GetHandle());
    Messages::SendMixResult(pPlayer, &handles);
    pMainMaterial->DBUpdate();
    return true;
}

bool MixManager::CreateItem(MixBase *pMixInfo, Player *pPlayer, Item *pMainMaterial, int nSubMaterialCount, std::vector<Item*> &pSubItem, std::vector<uint16> &pCountList)
{
    if(pMainMaterial != nullptr)
    {
        pPlayer->EraseItem(pMainMaterial, pMainMaterial->m_Instance.nCount);
    }

    for(int i = 0; i < nSubMaterialCount; ++i)
    {
        pPlayer->EraseItem(pSubItem[i], pCountList[i]);
    }
    bool bResult = false;
    if(pMixInfo->value[2] <= irand(0, 99))
    {
        Messages::SendMixResult(pPlayer, nullptr);
        return bResult;
    }

    int64 nRandom = irand(pMixInfo->value[3], pMixInfo->value[4]);
    int64 nItemCount = 0;
    if(pMixInfo->value[0] < 0)
        nItemCount = nRandom;
    else
        nItemCount = 1;

    if(nItemCount > 0)
    {
        bResult = true;
        Item* pItem{nullptr};
        while(nItemCount > 0)
        {
            int nItemID = pMixInfo->value[0];
            nItemCount = nRandom;
            while(nItemID < 0)
            {
                sObjectMgr->SelectItemIDFromDropGroup(nItemID, nItemID, nItemCount);
            }
            pItem = Item::AllocItem(0, nItemID, nItemCount, BY_MIX, pMixInfo->value[1], -1, -1, 0,0,0,0,0);
            if(!pItem->m_pItemBase->flaglist[FLAG_DUPLICATE])
            {
                //chatmsg
            }
            else
            {
                //chatmsg
            }

            pPlayer->PushItem(pItem, pItem->m_Instance.nCount, false);
            pItem->DBInsert();
            std::vector<uint> handles{};
            handles.emplace_back(pItem->GetHandle());
            Messages::SendMixResult(pPlayer, &handles);
            nItemCount--;
        }
    }
    return bResult;
}

MixBase *MixManager::GetProperMixInfo(Item *pMainMaterial, int nSubMaterialCount, std::vector<Item*> &pSubItem, std::vector<uint16> &pCountList)
{
    for(int i = 0; i < m_vMixInfo.size(); i++)
    {
        if(m_vMixInfo[i].sub_material_cnt == nSubMaterialCount)
        {
            if(getProperMixInfoSub(&m_vMixInfo[i], nSubMaterialCount, pSubItem, pCountList))
                return &m_vMixInfo[i];
        }
    }
    return nullptr;
}

bool MixManager::getProperMixInfoSub(MixBase *mb, int SubMaterialCount, std::vector<Item*> &pSubItem, std::vector<uint16> &pCountList)
{
    bool vCheckInfo[9]{false};
    for(int i = 0; i < SubMaterialCount; ++i)
    {
        bool ok{false};
        for(int j = 0; j < SubMaterialCount; ++j)
        {
            if(!vCheckInfo[j])
            {
                if(check_material_info(mb->sub_material[j], pSubItem[i], pCountList[i]))
                {
                    vCheckInfo[j] = true;
                    ok = true;
                    break;
                }
            }
        }
        if(!ok)
            return false;
    }
    return true;
}
Item *MixManager::check_mixable_item(Player *pPlayer, uint hItem, int64 nItemCount)
{
    auto retItem = sMemoryPool->GetObjectInWorld<Item>(hItem);
    if(retItem == nullptr)
        return nullptr;
    if(!retItem->IsItem())
    {
        Messages::SendResult(pPlayer, 256, TS_RESULT_ACCESS_DENIED, 0);
        return nullptr;
    }
    if(retItem->m_Instance.nCount < nItemCount)
    {
        Messages::SendResult(pPlayer, 256, TS_RESULT_NOT_EXIST, 0);
        return nullptr;
    }
    if((retItem->m_pItemBase->status_flag & 4) != 0)
    {
        Messages::SendResult(pPlayer, 256, TS_RESULT_ACCESS_DENIED, 0);
        return nullptr;
    }

    // todo: is mixable
    return retItem;
}

bool MixManager::check_material_info(const MaterialInfo &info, Item *pItem, uint16 &pItemCount)
{
    bool bIsCountChecked = false;
    if(info.type[0] == 0)
        return pItem == nullptr;

    if(pItem == nullptr)
        return false;

    for(int i = 0; i < MATERIAL_INFO_COUNT; ++i)
    {
        switch(info.type[i])
        {
            case (int)CheckType::CT_ItemGroup:
                if (pItem->m_pItemBase->group != info.value[i])
                    return false;
                break;

            case (int)CheckType::CT_ItemClass:
                if ((int)pItem->m_pItemBase->iclass != info.value[i])
                    return false;
                break;
            case (int)CheckType::CT_ItemId:
                if (pItem->m_Instance.Code != info.value[i])
                    return false;
                break;

            case (int)CheckType::CT_ItemRank:
                if (pItem->GetItemRank() != info.value[i])
                    return false;
                break;

            case (int)CheckType::CT_ItemLevel:
                if (pItem->m_Instance.nLevel != info.value[i])
                    return false;
                break;

            case (int)CheckType::CT_FlagOn:
                if (((1 << (info.value[i] & 0x1F)) & pItem->m_Instance.Flag) == 0)
                    return false;
                break;

            case (int)CheckType::CT_FlagOff:
                if(((1 << (info.value[i] & 0x1F)) & pItem->m_Instance.Flag) != 0)
                    return false;
                break;

            case (int)CheckType::CT_EnhanceMatch:
                if (pItem->m_Instance.nEnhance != info.value[i])
                    return false;
                break;

            case (int)CheckType::CT_EnhanceMismatch:
                if (pItem->m_Instance.nEnhance == info.value[i])
                    return false;
                break;

            case (int)CheckType::CT_ItemCount:
                bIsCountChecked = true;
                if (pItemCount != info.value[i])
                    return false;
                break;

            case (int)CheckType::CT_ElementalEffectMatch:
                /*tv = ((int)pow(2.0,(double)pItem->GetElementalEffectType())) >> 1;
                if (tv != 0)
                {
                    if ((tv & info.value[i]) != tv)
                        return false;
                }
                else
                {
                    if (info.value[i] != 0)
                        return false;
                }
                break;*/

            /*case (int)MixBase.CheckType.ElementalEffectMismatch:
                tv = ((int)Math.Pow(2.0, (double)pItem.GetElementalEffectType())) >> 1;
                if (tv != 0)
                {
                    if ((tv & info.value[i]) == tv)
                        return false;
                }
                else
                {
                    if (info.value[i] == 0)
                        return false;
                }
                break;*/

            case (int)CheckType::CT_ItemWearPositionMatch:
                if ((int)pItem->GetWearType() != info.value[i])
                    return false;
                break;

            case (int)CheckType::CT_ItemWearPositionMismatch:
                if ((int)pItem->GetWearType() == info.value[i])
                    return false;
                break;
            default:
                break;
        }
    }
    if (!bIsCountChecked)
        pItemCount = 1;
    return true;
}

EnhanceInfo *MixManager::getenhanceInfo(int sid)
{
    for(int i = 0; i < m_vEnhanceInfo.size(); i++)
    {
        if(m_vEnhanceInfo[i].nSID == sid)
            return &m_vEnhanceInfo[i];
    }
    return nullptr;
}

void MixManager::procEnhanceFail(Player *pPlayer, Item *pItem, int nFailResult)
{

}