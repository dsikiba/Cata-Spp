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

#include "Minion.h"
#include "DBCStructure.h"

Minion::Minion(SummonPropertiesEntry const* properties, Unit* owner, bool isWorldObject)
    : TempSummon(properties, owner, isWorldObject), m_owner(owner)
{
    ASSERT(m_owner);
    m_unitTypeMask |= UNIT_MASK_MINION;
    // Vehicles can be charmed
    if (m_Properties && m_Properties->Control == AsUnderlyingType(SummonControl::Vehicle))
        InitCharmInfo();
}

void Minion::InitStats(uint32 duration)
{
    TempSummon::InitStats(duration);
    SetReactState(REACT_PASSIVE);

    // Minions get thier summoner's guid assigned as creator guid
    m_owner->SetCreatorOfMinion(this, true);

    // Flag minions as controlled by player when their summoner is a player
    if (m_owner->IsPlayer())
    {
        m_ControlledByPlayer = true;
        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
    }

    if (!m_Properties)
        return;

   // Inherit summoner's faction when not overriden by SummonPropertiesEntry
    if (!m_Properties->Faction)
        SetFaction(m_owner->GetFaction());

    // Allied summons that are no companions or quest npcs will inherit their summoner's level.
    if (m_Properties->Slot < AsUnderlyingType(SummonSlot::Companion))
        SetLevel(m_owner->getLevel());

    // Inherit guild data from summoner
    ObjectGuid guildGUID = m_owner->GetGuidValue(OBJECT_FIELD_DATA);
    if (!guildGUID.IsEmpty())
    {
        SetGuidValue(OBJECT_FIELD_DATA, m_owner->GetGuidValue(OBJECT_FIELD_DATA));
        SetUInt16Value(OBJECT_FIELD_TYPE, 1, 1); // Has guild data
    }

    // Companions are immune to any combat interaction
    if (m_Properties && m_Properties->Slot == AsUnderlyingType(SummonSlot::Companion))
    {
        SelectLevel();       // some summoned creaters have different from 1 DB data for level/hp
        SetUInt32Value(UNIT_NPC_FLAGS, GetCreatureTemplate()->npcflag);
        SetImmuneToAll(true);
        m_owner->SetCritterGUID(GetGUID());
    }
}

void Minion::RemoveFromWorld()
{
    if (!IsInWorld())
        return;

    // First we tell our creator that we are no longer his summon
    m_owner->SetCreatorOfMinion(this, false);

    // Now we free the critter GUID
    m_owner->SetCritterGUID(ObjectGuid::Empty);

    // Now we remove all created summons that we have registered
    while (!_createdMinions.empty())
        (*_createdMinions.begin())->UnSummon();

    // All done, now we can return to the default cleanup procedure
    TempSummon::RemoveFromWorld();
}

bool Minion::IsWarlockMinion() const
{
    switch (GetEntry())
    {
        case ENTRY_IMP:
        case ENTRY_VOIDWALKER:
        case ENTRY_SUCCUBUS:
        case ENTRY_FELHUNTER:
        case ENTRY_FELGUARD:
            return true;
        default:
            return false;
    }
    return false;
}
