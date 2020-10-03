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

#include "Puppet.h"

Puppet::Puppet(SummonPropertiesEntry const* properties, Unit* owner) : Minion(properties, owner, false)
{
    ASSERT(m_owner->IsPlayer());
    m_unitTypeMask |= UNIT_MASK_PUPPET;
}

void Puppet::InitStats(uint32 duration)
{
    Minion::InitStats(duration);
    SetLevel(GetOwner()->getLevel());
    SetReactState(REACT_PASSIVE);
}

void Puppet::InitSummon()
{
    Minion::InitSummon();
    if (!SetCharmedBy(GetOwner(), CHARM_TYPE_POSSESS))
        ABORT();
}

void Puppet::Update(uint32 time)
{
    Minion::Update(time);
    //check if caster is channelling?
    if (IsInWorld())
        if (!IsAlive())
            UnSummon();
}

void Puppet::RemoveFromWorld()
{
    if (!IsInWorld())
        return;

    RemoveCharmedBy(nullptr);
    Minion::RemoveFromWorld();
}
