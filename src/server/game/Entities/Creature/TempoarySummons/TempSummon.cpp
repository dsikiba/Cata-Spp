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

#include "TempSummon.h"
#include "CreatureAI.h"
#include "DBCStructure.h"
#include "Log.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"

TempSummon::TempSummon(SummonPropertiesEntry const* properties, Unit* owner, bool isWorldObject) :
    Creature(isWorldObject), m_Properties(properties), m_type(TEMPSUMMON_MANUAL_DESPAWN),
    m_timer(0), m_lifetime(0), m_visibleBySummonerOnly(false)
{
    if (owner)
        m_summonerGUID = owner->GetGUID();

    m_unitTypeMask |= UNIT_MASK_SUMMON;
}

void TempSummon::Update(uint32 diff)
{
    Creature::Update(diff);

    if (m_deathState == DEAD)
    {
        UnSummon();
        return;
    }
    switch (m_type)
    {
        case TEMPSUMMON_MANUAL_DESPAWN:
        case TEMPSUMMON_DEAD_DESPAWN:
            break;
        case TEMPSUMMON_TIMED_DESPAWN:
        {
            if (m_timer <= diff)
            {
                UnSummon();
                return;
            }

            m_timer -= diff;
            break;
        }
        case TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT:
        {
            if (!IsInCombat())
            {
                if (m_timer <= diff)
                {
                    UnSummon();
                    return;
                }

                m_timer -= diff;
            }
            else if (m_timer != m_lifetime)
                m_timer = m_lifetime;

            break;
        }

        case TEMPSUMMON_CORPSE_TIMED_DESPAWN:
        {
            if (m_deathState == CORPSE)
            {
                if (m_timer <= diff)
                {
                    UnSummon();
                    return;
                }

                m_timer -= diff;
            }
            break;
        }
        case TEMPSUMMON_CORPSE_DESPAWN:
        {
            // if m_deathState is DEAD, CORPSE was skipped
            if (m_deathState == CORPSE)
            {
                UnSummon();
                return;
            }

            break;
        }
        case TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN:
        {
            if (m_deathState == CORPSE)
            {
                UnSummon();
                return;
            }

            if (!IsInCombat())
            {
                if (m_timer <= diff)
                {
                    UnSummon();
                    return;
                }
                else
                    m_timer -= diff;
            }
            else if (m_timer != m_lifetime)
                m_timer = m_lifetime;
            break;
        }
        case TEMPSUMMON_TIMED_OR_DEAD_DESPAWN:
        {
            if (!IsInCombat() && IsAlive())
            {
                if (m_timer <= diff)
                {
                    UnSummon();
                    return;
                }
                else
                    m_timer -= diff;
            }
            else if (m_timer != m_lifetime)
                m_timer = m_lifetime;
            break;
        }
        default:
            UnSummon();
            TC_LOG_ERROR("entities.unit", "Temporary summoned creature (entry: %u) have unknown type %u of ", GetEntry(), m_type);
            break;
    }
}

void TempSummon::UpdateObjectVisibilityOnCreate()
{
    WorldObject::UpdateObjectVisibility(true);
}

void TempSummon::RemoveFromWorld()
{
    if (!IsInWorld())
        return;

    // Clear the summon slot guid of our summoner if we have been assigned to one.
    if (m_Properties && m_Properties->Slot > 0)
    {
        if (Unit* owner = GetSummoner())
            if (owner->m_SummonSlot[m_Properties->Slot] == GetGUID())
                owner->m_SummonSlot[m_Properties->Slot].Clear();
    }

    Creature::RemoveFromWorld();
}

void TempSummon::InitStats(uint32 duration)
{
    ASSERT(!IsPet());

    m_timer = duration;
    m_lifetime = duration;

    if (m_type == TEMPSUMMON_MANUAL_DESPAWN)
        m_type = (duration == 0) ? TEMPSUMMON_DEAD_DESPAWN : TEMPSUMMON_TIMED_DESPAWN;


    // Todo: this code is a hack and belongs to gameobject casting. This needs to be removed after gameobject casting has been ported.
    Unit* owner = GetSummoner();
    if (owner && IsTrigger() && m_spells[0])
    {
        SetFaction(owner->GetFaction());
        SetLevel(owner->getLevel());
        if (owner->IsPlayer())
            m_ControlledByPlayer = true;
    }

    if (!m_Properties)
        return;

    if (owner)
    {
        // Unsummon previous tempsummon when it occupies the same summon slot as we do.
        if (m_Properties->Slot > 0)
        {
            if (owner->m_SummonSlot[m_Properties->Slot] && owner->m_SummonSlot[m_Properties->Slot] != GetGUID())
            {
                Creature* oldSummon = GetMap()->GetCreature(owner->m_SummonSlot[m_Properties->Slot]);
                if (oldSummon && oldSummon->IsSummon())
                    oldSummon->ToTempSummon()->UnSummon();
            }
            owner->m_SummonSlot[m_Properties->Slot] = GetGUID();
        }
    }

    // Override faction defined in SummonPropertiesEntry
    if (m_Properties->Faction)
        SetFaction(m_Properties->Faction);
}

void TempSummon::InitSummon()
{
    if (Unit* owner = GetSummoner())
    {
        if (owner->IsCreature() && owner->ToCreature()->IsAIEnabled())
            owner->ToCreature()->AI()->JustSummoned(this);
        if (IsAIEnabled())
            AI()->IsSummonedBy(owner);
    }
}

void TempSummon::UnSummon(uint32 msTime)
{
    if (msTime)
    {
        ForcedUnsummonDelayEvent* pEvent = new ForcedUnsummonDelayEvent(*this);
        m_Events.AddEvent(pEvent, m_Events.CalculateTime(msTime));
        return;
    }

    if (IsPet())
    {
        ToPet()->Remove(PET_SAVE_DISMISS);
        ASSERT(!IsInWorld());
        return;
    }

    if (Unit* owner = GetSummoner())
        if (owner->IsCreature() && owner->ToCreature()->IsAIEnabled())
            owner->ToCreature()->AI()->SummonedCreatureDespawn(this);

    AddObjectToRemoveList();
}

Unit* TempSummon::GetSummoner() const
{
    return m_summonerGUID ? ObjectAccessor::GetUnit(*this, m_summonerGUID) : nullptr;
}

Creature* TempSummon::GetSummonerCreatureBase() const
{
    return m_summonerGUID ? ObjectAccessor::GetCreature(*this, m_summonerGUID) : nullptr;
}

/*static*/ bool TempSummon::IsPersonalSummonOfAnotherPlayer(Creature const* summon, ObjectGuid playerToCheck)
{
    if (TempSummon const* tempSummon = summon->ToTempSummon())
        if (tempSummon->IsVisibleBySummonerOnly() && playerToCheck != tempSummon->GetSummonerGUID())
            return true;

    return false;
}

bool ForcedUnsummonDelayEvent::Execute(uint64 /*e_time*/, uint32 /*p_time*/)
{
    m_owner.UnSummon();
    return true;
}
