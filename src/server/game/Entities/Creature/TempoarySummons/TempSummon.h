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

#ifndef TRINITYCORE_TEMPSUMMON_H
#define TRINITYCORE_TEMPSUMMON_H

#include "Creature.h"

struct SummonPropertiesEntry;

enum PetEntries
{
    // Warlock Pet/Minions
    ENTRY_IMP                       = 416,
    ENTRY_VOIDWALKER                = 1860,
    ENTRY_SUCCUBUS                  = 1863,
    ENTRY_FELHUNTER                 = 417,
    ENTRY_FELGUARD                  = 17252,
    ENTRY_INFERNAL                  = 89,
    ENTRY_EBON_IMP                  = 50675,

    // Mage Pet
    ENTRY_WATER_ELEMENTAL           = 510,

    // Druid Minion
    ENTRY_TREANT                    = 1964,

    // Shaman Minions
    ENTRY_EARTH_ELEMENTAL           = 15352,
    ENTRY_FIRE_ELEMENTAL            = 15438,
    ENTRY_SPIRIT_WOLF               = 29264,

    // Death Knight Pet/Minions
    ENTRY_GHOUL                     = 26125,
    ENTRY_RISEN_ALLY                = 30230,
    ENTRY_BLOODWORM                 = 28017,
    ENTRY_RUNIC_WEAPON              = 27893,
    ENTRY_GARGOYLE                  = 27829,
    ENTRY_SHADOWFIEND               = 19668,
    ENTRY_ARMY_OF_THE_DEAD_GHOUL    = 24207,

    // Hunter Trap Adds
    ENTRY_VENOMOUS_SNAKE            = 19833,
    ENTRY_VIPER                     = 19921
};

enum PlayerPetSpells
{
    // Risen Ghoul
    SPELL_PET_RISEN_GHOUL_SPAWN_IN  = 47448,
    SPELL_PET_RISEN_GHOUL_SELF_STUN = 47466,

    // Hunter Pets
    SPELL_PET_ENERGIZE              = 99289
};

class TC_GAME_API TempSummon : public Creature
{
    public:
        explicit TempSummon(SummonPropertiesEntry const* properties, Unit* owner, bool isWorldObject);
        virtual ~TempSummon() { }

        void Update(uint32 time) override;
        void UpdateObjectVisibilityOnCreate() override;
        void RemoveFromWorld() override;
        void SaveToDB(uint32 /*mapid*/, uint8 /*spawnMask*/) override { }

        virtual void InitStats(uint32 lifetime);
        virtual void InitSummon();
        virtual void UnSummon(uint32 msTime = 0);

        Unit* GetSummoner() const;
        Creature* GetSummonerCreatureBase() const;

        ObjectGuid GetSummonerGUID() const { return m_summonerGUID; }
        TempSummonType const& GetSummonType() { return m_type; }
        uint32 GetTimer() const { return m_timer; }

        void SetTempSummonType(TempSummonType type) { m_type = type; }
        void SetVisibleBySummonerOnly(bool visibleBySummonerOnly) { m_visibleBySummonerOnly = visibleBySummonerOnly; }
        bool IsVisibleBySummonerOnly() const { return m_visibleBySummonerOnly; }
        static bool IsPersonalSummonOfAnotherPlayer(Creature const* summon, ObjectGuid playerToCheck);

        SummonPropertiesEntry const* const m_Properties;
    private:
        TempSummonType m_type;
        uint32 m_timer;
        uint32 m_lifetime;
        ObjectGuid m_summonerGUID;
        bool m_visibleBySummonerOnly;
};

class TC_GAME_API ForcedUnsummonDelayEvent : public BasicEvent
{
public:
    ForcedUnsummonDelayEvent(TempSummon& owner) : BasicEvent(), m_owner(owner) { }
    bool Execute(uint64 e_time, uint32 p_time) override;

private:
    TempSummon& m_owner;
};
#endif
