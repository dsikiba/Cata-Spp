/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Guardian.h"
#include "Pet.h"
#include "Player.h"

Guardian::Guardian(SummonPropertiesEntry const* properties, Unit* owner, bool isWorldObject) : Minion(properties, owner, isWorldObject),
    m_bonusSpellDamage(0)
{
    memset(m_statFromOwner, 0, sizeof(float) * MAX_STATS);
    m_unitTypeMask |= UNIT_MASK_GUARDIAN;
    InitCharmInfo();
}

void Guardian::InitStats(uint32 duration)
{
    Minion::InitStats(duration);

    // Guardians get their summoner's guid assigned as owner guid
    m_owner->SetOwnerOfMinion(this, true);

    InitStatsForLevel(m_owner->getLevel());

    if (m_owner->IsPlayer())
        m_charmInfo->InitCharmCreateSpells();

    SetReactState(REACT_ASSIST);
}

void Guardian::RemoveFromWorld()
{
    if (!IsInWorld())
        return;

    // Unregister guardian from owner's owned unit set
    m_owner->SetOwnerOfMinion(this, false);

    // Unset active guardian from owner
    m_owner->SetActiveGuardian(this, false);

    // All done, continue with default procedure
    Minion::RemoveFromWorld();
}

void Guardian::InitSummon()
{
    TempSummon::InitSummon();

    // Only one guardian can be active at a time
    UnsummonActiveGuardian();
    m_owner->SetActiveGuardian(this, true);

    if (m_owner->IsPlayer() && m_owner->GetActiveGuardianGUID() == GetGUID() && !m_owner->GetCharmedGUID())
        m_owner->ToPlayer()->CharmSpellInitialize();
}

void Guardian::UnsummonActiveGuardian()
{
    if (Guardian* activeGuardian = m_owner->GetActiveGuardian())
    {
        // Sanity check, never unsummon ourselves
        if (activeGuardian == this || activeGuardian->GetEntry() == GetEntry())
            return;

        // Pets will get dismisses
        if (activeGuardian->IsPet())
            activeGuardian->ToPet()->Remove(PET_SAVE_DISMISS);
        else
            activeGuardian->UnSummon();
    }
}
