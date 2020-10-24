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

    if (m_owner->IsPlayer())
        m_charmInfo->InitCharmCreateSpells();

    SetReactState(REACT_ASSIST);
}

void Guardian::InitSummon()
{
    Minion::InitSummon();

    // Only one guardian can be active at a time
    UnsummonActiveGuardian();
    m_owner->SetActiveGuardian(this, true);

    if (m_owner->IsPlayer() && m_owner->GetActiveGuardianGUID() == GetGUID() && !m_owner->GetCharmedGUID())
        m_owner->ToPlayer()->CharmSpellInitialize();
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

bool Guardian::InitStatsForLevel(uint8 petlevel, bool /*referenceOwner*/)
{
    //Determine pet type
    if (IsPet() && m_owner->IsPlayer())
    {
        switch (m_owner->getClass())
        {
            case CLASS_WARLOCK:
            case CLASS_SHAMAN:
            case CLASS_DEATH_KNIGHT:
            case CLASS_MAGE:
            case CLASS_PRIEST:
                break;
            case CLASS_HUNTER:
                m_unitTypeMask |= UNIT_MASK_HUNTER_PET;
                break;
            default:
                TC_LOG_ERROR("entities.pet", "Unknown type pet %u is summoned by player class %u",
                    GetEntry(), GetOwner()->getClass());
                break;
        }
    }

    // Patch 3.1.0 (2009-04-14): All Cunning, Ferocity, and Tenacity pets now have identical bonuses - +5% Health,+5% Armor,+5% Damage. 
    if (IsHunterPet())
    {
        SetMaxHealthModifier(0.049999952f); // Sniffed value
        SetModifierValue(UNIT_MOD_ARMOR, BASE_PCT, 1.049999952f); // @todo: is this the correct approach or is there some hidden field?
    }

    // Damage
    SetBonusDamage(0);

    Minion::InitStatsForLevel(petlevel, true);

    return true;
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
