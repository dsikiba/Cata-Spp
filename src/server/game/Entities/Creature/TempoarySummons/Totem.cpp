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

#include "Totem.h"
#include "Group.h"
#include "Opcodes.h"
#include "Player.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "WorldPacket.h"

Totem::Totem(SummonPropertiesEntry const* properties, Unit* owner) : Minion(properties, owner, false)
{
    m_unitTypeMask |= UNIT_MASK_TOTEM;
    m_duration = 0;
    m_type = TOTEM_PASSIVE;
}

void Totem::Update(uint32 diff)
{
    if (!m_owner->IsAlive() || !IsAlive())
    {
        UnSummon();
        return;
    }

    Minion::Update(diff);
}

void Totem::InitStats(uint32 duration)
{
    Minion::InitStats(duration);

    // client requires SMSG_TOTEM_CREATED to be sent before adding to world and before removing old totem
    if (m_owner->IsPlayer()  && SummonSlot(m_Properties->Slot) >= SummonSlot::TotemFire && SummonSlot(m_Properties->Slot) < SummonSlot::Max)
    {
        WorldPacket data(SMSG_TOTEM_CREATED, 1 + 8 + 4 + 4);
        data << uint8(m_Properties->Slot - 1);
        data << uint64(GetGUID());
        data << uint32(duration);
        data << uint32(GetUInt32Value(UNIT_CREATED_BY_SPELL));
        m_owner->ToPlayer()->SendDirectMessage(&data);

        // set display id depending on caster's race
        SetDisplayId(m_owner->GetModelForTotem(PlayerTotemType(m_Properties->ID)));
    }

    // Get spell cast by totem
    if (SpellInfo const* totemSpell = sSpellMgr->GetSpellInfo(GetSpell()))
        if (totemSpell->CalcCastTime(getLevel()))   // If spell has cast time -> its an active totem
            m_type = TOTEM_ACTIVE;

    SetLevel(m_owner->getLevel());
}

void Totem::InitSummon()
{
    if (m_type == TOTEM_PASSIVE && GetSpell())
        CastSpell(this, GetSpell());

    // Some totems can have both instant effect and passive spell
    if (GetSpell(1))
        CastSpell(this, GetSpell(1));

    if (m_Properties->ID == SUMMON_TYPE_TOTEM_FIRE && m_owner->HasAura(SPELL_TOTEMIC_WRATH_TALENT))
        CastSpell(this, SPELL_TOTEMIC_WRATH);

    Minion::InitSummon();
}

void Totem::UnSummon(uint32 msTime)
{
    CombatStop();
    RemoveAurasDueToSpell(GetSpell(), GetGUID());

    // clear owner's totem slot
    for (uint8 i = AsUnderlyingType(SummonSlot::TotemFire); i < AsUnderlyingType(SummonSlot::Max); ++i)
    {
        if (m_owner->m_SummonSlot[i] == GetGUID())
        {
            m_owner->m_SummonSlot[i].Clear();
            break;
        }
    }

    m_owner->RemoveAurasDueToSpell(GetSpell(), GetGUID());

    // remove aura all party members too
    if (Player* owner = m_owner->ToPlayer())
    {
        owner->SendAutoRepeatCancel(this);

        if (SpellInfo const* spell = sSpellMgr->GetSpellInfo(GetUInt32Value(UNIT_CREATED_BY_SPELL)))
            GetSpellHistory()->SendCooldownEvent(spell, 0, nullptr, false);

        if (Group* group = owner->GetGroup())
        {
            for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* target = itr->GetSource();
                if (target && target->IsInMap(owner) && group->SameSubGroup(owner, target))
                    target->RemoveAurasDueToSpell(GetSpell(), GetGUID());
            }
        }
    }

    Minion::UnSummon(msTime);
}

bool Totem::IsImmunedToSpellEffect(SpellInfo const* spellInfo, uint32 index, Unit* caster) const
{
    /// @todo possibly all negative auras immune?
    if (GetEntry() == 5925)
        return false;

    switch (spellInfo->Effects[index].ApplyAuraName)
    {
        case SPELL_AURA_PERIODIC_DAMAGE:
        case SPELL_AURA_PERIODIC_LEECH:
        case SPELL_AURA_MOD_FEAR:
        case SPELL_AURA_TRANSFORM:
            return true;
        default:
            break;
    }

    return Creature::IsImmunedToSpellEffect(spellInfo, index, caster);
}
