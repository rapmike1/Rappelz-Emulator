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

#include "FieldPropManager.h"
#include "ObjectMgr.h"
#include "World.h"

void FieldPropManager::SpawnFieldPropFromScript(FieldPropRespawnInfo prop, int lifeTime)
{
    auto propTemplate = sObjectMgr->GetFieldPropBase(prop.nPropID);
    if(propTemplate == nullptr)
        return;

    {
        MX_UNIQUE_GUARD writeGuard(i_lock);
        m_vRespawnInfo.emplace_back(prop);

        FieldPropRegenInfo ri = FieldPropRegenInfo{0, (uint)lifeTime};
        ri.pRespawnInfo = prop;
        m_vRespawnList.emplace_back(ri);
    }
}

void FieldPropManager::RegisterFieldProp(FieldPropRespawnInfo prop)
{
    FieldPropRespawnInfo info{prop};
    int nPropID = prop.nPropID;
    FieldPropTemplate* propTemplate = sObjectMgr->GetFieldPropBase(nPropID);
    if(propTemplate == nullptr)
        return;

    {
        MX_UNIQUE_GUARD writeGuard(i_lock);
        info.nPropID = nPropID;
        info.layer   = 0; // Layer management
        m_vRespawnInfo.emplace_back(info);
        FieldPropRegenInfo ri = FieldPropRegenInfo{propTemplate->nRegenTime + sWorld->GetArTime(), propTemplate->nLifeTime};
        ri.pRespawnInfo = info;
        m_vRespawnList.emplace_back(ri);
    }
}

/*
 * Do **not** delete the pointer here, this function is called
 * before the fieldprop gets deleted by the MemoryPool class!!!
 *
 */
void FieldPropManager::onFieldPropDelete(FieldProp *prop)
{
    {
        MX_UNIQUE_GUARD writeGuard(i_lock);
        for (int        i = 0; i < m_vExpireObject.size(); i++)
        {
            auto fp = m_vExpireObject[i];
            if (fp->GetHandle() == prop->GetHandle())
            {
                m_vExpireObject.erase(m_vExpireObject.begin() + i);
            }
        }

        if (!prop->m_PropInfo.bOnce)
        {
            FieldPropRegenInfo ri = FieldPropRegenInfo{prop->m_pFieldPropBase->nRegenTime + sWorld->GetArTime(), prop->nLifeTime};
            ri.pRespawnInfo = prop->m_PropInfo;
            m_vRespawnList.emplace_back(ri);
        }
    }
}

void FieldPropManager::Update(uint/* diff*/)
{
    uint ct = sWorld->GetArTime();
    std::vector<FieldPropRegenInfo> vRegenInfo{};
    std::vector<FieldProp*> vDeleteList{};

    // "Critical section" for lock (yeah, I prefer those)
    {
        MX_UNIQUE_GUARD writeGuard(i_lock);
        for (int i = 0; i < m_vRespawnList.size(); ++i)
        {
            FieldPropRegenInfo regen = m_vRespawnList[i];
            if (regen.tNextRegen < ct)
            {
                vRegenInfo.emplace_back(regen);
                m_vRespawnList.erase(m_vRespawnList.begin() + i);
            }
        }

        if (!vRegenInfo.empty())
        {
            for (auto &rg : vRegenInfo)
            {
                FieldProp *pProp = FieldProp::Create(this, rg.pRespawnInfo, rg.nLifeTime);
                if (pProp->nLifeTime != 0)
                {
                    m_vExpireObject.emplace_back(pProp);
                }
            }
        }

        for (auto &fp : m_vExpireObject)
        {
            if (fp->m_nRegenTime + fp->nLifeTime < ct)
                vDeleteList.emplace_back(fp);
        }

        if (!vDeleteList.empty())
        {
            for (auto &fp : vDeleteList)
            {
                if (fp->IsInWorld() && !fp->IsDeleteRequested())
                {
                    sWorld->RemoveObjectFromWorld(fp);
                    fp->DeleteThis();
                }
            }
        }
    } //- Lock end
}
