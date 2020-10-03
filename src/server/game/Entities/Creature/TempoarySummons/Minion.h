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

#ifndef TRINITYCORE_MINION_H
#define TRINITYCORE_MINION_H

#include "TempSummon.h"

class TC_GAME_API Minion : public TempSummon
{
    public:
        Minion(SummonPropertiesEntry const* properties, Unit* owner, bool isWorldObject);
        void InitStats(uint32 duration) override;
        void RemoveFromWorld() override;

        Unit* GetOwner() const { return m_owner; }

        bool IsPetGhoul() const { return GetEntry() == ENTRY_GHOUL; } // Ghoul may be guardian or pet
        bool IsRisenAlly() const { return GetEntry() == ENTRY_RISEN_ALLY; }
        bool IsSpiritWolf() const { return GetEntry() == ENTRY_SPIRIT_WOLF; } // Spirit wolf from feral spirits
        bool IsWarlockMinion() const;
    protected:
        Unit* const m_owner;
};

#endif
