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
#include "CreatureData.h"
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
    uint8 level = getLevel();
    bool referenceOwner = false;
    if (m_Properties->Slot < AsUnderlyingType(SummonSlot::Companion))
    {
        level = m_owner->getLevel();
        referenceOwner = true;
    }

    InitStatsForLevel(level, referenceOwner);

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

void Minion::InitSummon()
{
    // Some minions have passive spells as well
    if (CreatureTemplate const* cInfo = GetCreatureTemplate())
    {
        if (CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cInfo->family))
        {
            PetFamilySpellsStore::const_iterator petStore = sPetFamilySpellsStore.find(cFamily->ID);
            if (petStore != sPetFamilySpellsStore.end())
            {
                for (uint32 spellId : petStore->second)
                    CastSpell(nullptr, spellId);
            }
        }
    }

    TempSummon::InitSummon();
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

// This function is a modified version of Creature::UpdateLevelDependantStats to support dynamic level and expansion scaling.
bool Minion::InitStatsForLevel(uint8 level, bool referenceOwner /*= false*/)
{
    CreatureTemplate const* cInfo = GetCreatureTemplate();
    ASSERT(cInfo);

    SetLevel(level);
    uint8 expansion = IsHunterPet() ? EXPANSION_CLASSIC : cInfo->expansion;

    // If we inherit the level of our owner, we change our expansion based on his reached level
    uint8 ownerLevel = m_owner->getLevel();
    if (referenceOwner && !IsHunterPet())
    {
        if (ownerLevel <= DBCManager::GetMaxLevelForExpansion(EXPANSION_CLASSIC))
            expansion = EXPANSION_CLASSIC;
        else if (ownerLevel > DBCManager::GetMaxLevelForExpansion(EXPANSION_CLASSIC) && ownerLevel <= DBCManager::GetMaxLevelForExpansion(EXPANSION_THE_BURNING_CRUSADE))
            expansion = EXPANSION_THE_BURNING_CRUSADE;
        else if (ownerLevel > DBCManager::GetMaxLevelForExpansion(EXPANSION_THE_BURNING_CRUSADE) && ownerLevel <= DBCManager::GetMaxLevelForExpansion(EXPANSION_CATACLYSM))
            expansion = EXPANSION_WRATH_OF_THE_LICH_KING;
        else if (ownerLevel >= DBCManager::GetMaxLevelForExpansion(EXPANSION_CATACLYSM))
            expansion = EXPANSION_CATACLYSM;
    }

    uint32 rank = IsHunterPet() ? 0 : cInfo->rank;
    CreatureBaseStats const* stats = sObjectMgr->GetCreatureBaseStats(getLevel(), IsHunterPet() ? UNIT_CLASS_WARRIOR : cInfo->unit_class);
    if (!stats)
        return false;

    // Health
    float healthmod = IsHunterPet() ? 1.f : _GetHealthMod(rank);

    uint32 basehp = uint32(std::ceil(stats->BaseHealth[expansion] * cInfo->ModHealth * cInfo->ModHealthExtra));
    uint32 health = uint32(basehp * healthmod);
    AddPct(health, GetMaxHealthModifier());

    SetCreateHealth(health);
    SetMaxHealth(health);
    SetHealth(health);
    ResetPlayerDamageReq();

    // Mana
    switch (getClass())
    {
        case UNIT_CLASS_PALADIN:
        case UNIT_CLASS_MAGE:
        {
            uint32 mana = 0;
            if (uint32 basemana = stats->BaseMana)
                mana = uint32(std::ceil(basemana * cInfo->ModMana * cInfo->ModManaExtra));
            SetCreateMana(mana);
            SetMaxPower(POWER_MANA, mana);
            SetFullPower(POWER_MANA);
            break;
        }
        default: // We don't set max power here, 0 makes power bar hidden
            break;
    }

    SetModifierValue(UNIT_MOD_HEALTH, BASE_VALUE, (float)health);

    // Attack
    float basedamage = stats->BaseDamage[expansion];
    float weaponBaseMinDamage = basedamage;
    float weaponBaseMaxDamage = basedamage * 1.5f;

    SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, weaponBaseMinDamage);
    SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, weaponBaseMaxDamage);
    SetBaseWeaponDamage(OFF_ATTACK, MINDAMAGE, weaponBaseMinDamage);
    SetBaseWeaponDamage(OFF_ATTACK, MAXDAMAGE, weaponBaseMaxDamage);
    SetBaseWeaponDamage(RANGED_ATTACK, MINDAMAGE, weaponBaseMinDamage);
    SetBaseWeaponDamage(RANGED_ATTACK, MAXDAMAGE, weaponBaseMaxDamage);
    SetModifierValue(UNIT_MOD_ATTACK_POWER, BASE_VALUE, stats->AttackPower);
    SetModifierValue(UNIT_MOD_ATTACK_POWER_RANGED, BASE_VALUE, stats->RangedAttackPower);

    // Defense
    float armor = (float)stats->GenerateArmor(cInfo);
    SetModifierValue(UNIT_MOD_ARMOR, BASE_VALUE, armor);

    // From here on we do own additional steps
    SetMeleeDamageSchool(SpellSchools(cInfo->dmgschool));
    SetAttackTime(BASE_ATTACK, IsHunterPet() ? BASE_ATTACK_TIME : cInfo->BaseAttackTime);
    SetAttackTime(OFF_ATTACK, IsHunterPet() ? BASE_ATTACK_TIME : cInfo->BaseAttackTime);
    SetAttackTime(RANGED_ATTACK, IsHunterPet() ? BASE_ATTACK_TIME : cInfo->BaseAttackTime);
    SetCanModifyStats(true);

    // Power
    if (IsHunterPet())
        SetPowerType(POWER_FOCUS);
    else if (IsPetGhoul() || IsRisenAlly())
        SetPowerType(POWER_ENERGY);
    else
        SetPowerType(POWER_MANA);

    // Speed Mods
    SetFloatValue(UNIT_MOD_CAST_SPEED, 1.0f);
    SetFloatValue(UNIT_MOD_CAST_HASTE, 1.0f);

    // Scale
    SetObjectScale(GetNativeObjectScale());

    // Resistances
    for (uint8 i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        SetModifierValue(UnitMods(UNIT_MOD_RESISTANCE_START + i), BASE_VALUE, IsHunterPet() ? 0.f : cInfo->resistance[i]);

    // @todo: this should be for ALL creatures, not just minions, pets, whatever
    if (PetLevelInfo const* pInfo = sObjectMgr->GetPetLevelInfo(GetEntry(), level))
    {
        for (uint8 stat = 0; stat < MAX_STATS; ++stat)
            SetCreateStat(Stats(stat), float(pInfo->stats[stat]));

        UpdateAllStats();
    }

    return true;
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
