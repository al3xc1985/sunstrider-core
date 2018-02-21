#include "../ClassSpellsDamage.h"
#include "../ClassSpellsCoeff.h"
#include "PlayerbotAI.h"
#include "SpellHistory.h"
#include <limits>

class DispelMagicTest : public TestCaseScript
{
public:
    DispelMagicTest() : TestCaseScript("spells priest dispel_magic") { }

    class DispelMagicTestImpt : public TestCase
    {
    public:
        DispelMagicTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* enemy1 = SpawnPlayer(CLASS_PRIEST, RACE_HUMAN);
            TestPlayer* enemy2 = SpawnPlayer(CLASS_DRUID, RACE_NIGHTELF);

            // Fail -- Bug here
            TEST_CAST(priest, priest, ClassSpells::Priest::DISPEL_MAGIC_RNK_2, SPELL_FAILED_NOTHING_TO_DISPEL, TRIGGERED_IGNORE_GCD);
            TEST_CAST(priest, enemy1, ClassSpells::Priest::DISPEL_MAGIC_RNK_2, SPELL_FAILED_NOTHING_TO_DISPEL, TRIGGERED_IGNORE_GCD);

            // Test mana cost
            TEST_CAST(enemy1, priest, ClassSpells::Priest::SHADOW_WORD_PAIN_RNK_10, SPELL_CAST_OK, TRIGGERED_IGNORE_GCD);
            uint32 const expectedDispelMagicMana = 366;
            TEST_POWER_COST(priest, priest, ClassSpells::Priest::DISPEL_MAGIC_RNK_2, POWER_MANA, expectedDispelMagicMana);

            // Setup
            TEST_CAST(enemy1, enemy1, ClassSpells::Priest::DIVINE_SPIRIT_RNK_5, SPELL_CAST_OK, TRIGGERED_IGNORE_GCD);
            TEST_CAST(enemy1, enemy1, ClassSpells::Priest::FEAR_WARD_RNK_1, SPELL_CAST_OK, TRIGGERED_IGNORE_GCD);
            TEST_CAST(enemy1, enemy1, ClassSpells::Priest::INNER_FIRE_RNK_7, SPELL_CAST_OK, TRIGGERED_IGNORE_GCD);
            TEST_CAST(enemy1, priest, ClassSpells::Priest::SHADOW_WORD_PAIN_RNK_10, SPELL_CAST_OK, TRIGGERED_IGNORE_GCD);
            TEST_CAST(enemy2, priest, ClassSpells::Druid::MOONFIRE_RNK_12, SPELL_CAST_OK, TRIGGERED_IGNORE_GCD);
            TEST_CAST(enemy2, priest, ClassSpells::Druid::INSECT_SWARM_RNK_6, SPELL_CAST_OK, TRIGGERED_IGNORE_GCD);

            TEST_HAS_AURA(enemy1, ClassSpells::Priest::DIVINE_SPIRIT_RNK_5);
            TEST_HAS_AURA(enemy1, ClassSpells::Priest::FEAR_WARD_RNK_1);
            TEST_HAS_AURA(enemy1, ClassSpells::Priest::INNER_FIRE_RNK_7);
            TEST_HAS_AURA(priest, ClassSpells::Priest::SHADOW_WORD_PAIN_RNK_10);
            TEST_HAS_AURA(priest, ClassSpells::Druid::MOONFIRE_RNK_12);
            TEST_HAS_AURA(priest, ClassSpells::Druid::INSECT_SWARM_RNK_6);

            // Note: Auras are dispelled in random order (No 100% sure for defensive, it may be last in first out)
            // Defensive dispel
            priest->ResetAllPowers();
            TEST_CAST(priest, priest, ClassSpells::Priest::DISPEL_MAGIC_RNK_2, SPELL_CAST_OK, TRIGGERED_IGNORE_GCD);
            uint32 remainingBuffs =   uint32(priest->HasAura(ClassSpells::Druid::INSECT_SWARM_RNK_6))
                                    + uint32(priest->HasAura(ClassSpells::Druid::MOONFIRE_RNK_12))
                                    + uint32(priest->HasAura(ClassSpells::Priest::SHADOW_WORD_PAIN_RNK_10));
            ASSERT_INFO("Testing defensive dispel, check if priest has correctly lost 2 aura");
            TEST_ASSERT(remainingBuffs == 1);

            // Offensive dispel (random order confirmed in BC arena videos)
            TEST_CAST(priest, enemy1, ClassSpells::Priest::DISPEL_MAGIC_RNK_2, SPELL_CAST_OK, TRIGGERED_IGNORE_GCD);
            remainingBuffs =  uint32(enemy1->HasAura(ClassSpells::Priest::INNER_FIRE_RNK_7))
                            + uint32(enemy1->HasAura(ClassSpells::Priest::FEAR_WARD_RNK_1))
                            + uint32(enemy1->HasAura(ClassSpells::Priest::DIVINE_SPIRIT_RNK_5));
            ASSERT_INFO("Testing offensive dispel, check if target has correctly lost 2 aura");
            TEST_ASSERT(remainingBuffs == 1);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<DispelMagicTestImpt>();
    }
};

class FearWardTest : public TestCaseScript
{
public:
    FearWardTest() : TestCaseScript("spells priest fear_ward") { }

    class FearWardTestImpt : public TestCase
    {
    public:
        FearWardTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* warlock = SpawnPlayer(CLASS_WARLOCK, RACE_HUMAN);

            // Mana cost
            uint32 const expectedFearWardMana = 78;
            TEST_POWER_COST(priest, priest, ClassSpells::Priest::FEAR_WARD_RNK_1, POWER_MANA, expectedFearWardMana);

            // Aura
            TEST_AURA_MAX_DURATION(priest, ClassSpells::Priest::FEAR_WARD_RNK_1, 3 * MINUTE * IN_MILLISECONDS);

            // Cooldown
            TEST_HAS_COOLDOWN(priest, ClassSpells::Priest::FEAR_WARD_RNK_1, 3 * MINUTE);

            // Fear
            TEST_CAST(warlock, priest, ClassSpells::Warlock::FEAR_RNK_3, SPELL_CAST_OK, TRIGGERED_CAST_DIRECTLY);
            TEST_HAS_NOT_AURA(priest, ClassSpells::Priest::FEAR_WARD_RNK_1);
            TEST_CAST(warlock, priest, ClassSpells::Warlock::FEAR_RNK_3, SPELL_CAST_OK, TRIGGERED_CAST_DIRECTLY);
            TEST_HAS_AURA(priest, ClassSpells::Warlock::FEAR_RNK_3);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<FearWardTestImpt>();
    }
};

class InnerFireTest : public TestCaseScript
{
public:
    InnerFireTest() : TestCaseScript("spells priest inner_fire") { }

    class InnerFireTestImpt : public TestCase
    {
    public:
        InnerFireTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* rogue = SpawnPlayer(CLASS_ROGUE, RACE_HUMAN);

            priest->DisableRegeneration(true);

            uint32 innerFireBonus = 1580;
            uint32 expectedArmor = priest->GetArmor() + innerFireBonus;

            // Mana cost
            uint32 const expectedInnerFireMana = 375;
            TEST_POWER_COST(priest, priest, ClassSpells::Priest::INNER_FIRE_RNK_7, POWER_MANA, expectedInnerFireMana);

            // Aura
            TEST_AURA_MAX_DURATION(priest, ClassSpells::Priest::INNER_FIRE_RNK_7, 10 * MINUTE * IN_MILLISECONDS);

            // Charges
            TEST_AURA_CHARGE(priest, ClassSpells::Priest::INNER_FIRE_RNK_7, 20);

            // Armor
            TEST_ASSERT(priest->GetArmor() == expectedArmor);

            // Lose charge
            auto AI = rogue->GetTestingPlayerbotAI();
            int hits = 0;
            for (uint32 i = 0; i < 150; i++)
            {
                rogue->AttackerStateUpdate(priest, BASE_ATTACK);
                auto damageToTarget = AI->GetWhiteDamageDoneInfo(priest);
                TEST_ASSERT(damageToTarget->size() == i + 1);
                auto& data = damageToTarget->back();
                if (data.damageInfo.HitOutCome != MELEE_HIT_NORMAL && data.damageInfo.HitOutCome != MELEE_HIT_CRIT)
                    continue;
                hits++;
                Aura* aura = priest->GetAura(ClassSpells::Priest::INNER_FIRE_RNK_7);
                if (hits < 20)
                {
                    TEST_ASSERT(aura != nullptr);
                    int currentCharges = aura->GetCharges();
                    TEST_ASSERT(currentCharges == 20 - hits);
                }
                else
                {
                    TEST_ASSERT(aura == nullptr);
                    TEST_ASSERT(!priest->HasAura(ClassSpells::Priest::INNER_FIRE_RNK_7));
                    break;
                }
            }
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<InnerFireTestImpt>();
    }
};

class LevitateTest : public TestCaseScript
{
public:
    LevitateTest() : TestCaseScript("spells priest levitate") { }

    class LevitateTestImpt : public TestCase
    {
    public:
        LevitateTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* rogue = SpawnPlayer(CLASS_ROGUE, RACE_HUMAN);

            priest->DisableRegeneration(true);

            // Mana cost
            uint32 const expectedLevitateMana = 100;
            priest->AddItem(17056, 1); // Reagent Light Feather
            TEST_POWER_COST(priest, priest, ClassSpells::Priest::LEVITATE_RNK_1, POWER_MANA, expectedLevitateMana);
            TEST_ASSERT(priest->GetItemCount(17056, false) == 0);

            // Aura
            TEST_AURA_MAX_DURATION(priest, ClassSpells::Priest::LEVITATE_RNK_1, 2 * MINUTE * IN_MILLISECONDS);

            // Cancel on damage taken
            rogue->Attack(priest, true);
            Wait(500);
            rogue->AttackStop();
            TEST_ASSERT(!priest->HasAura(ClassSpells::Priest::LEVITATE_RNK_1));

            // TODO: over water, slow fall
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<LevitateTestImpt>();
    }
};

class ManaBurnTest : public TestCaseScript
{
public:
    ManaBurnTest() : TestCaseScript("spells priest mana_burn") { }

    class ManaBurnTestImpt : public TestCase
    {
    public:
        ManaBurnTestImpt() : TestCase(STATUS_PASSING, true) { }

        void TestManaBurn(TestPlayer* priest, Unit* victim, uint32 expectedMin, uint32 expectedMax)
        {
            uint32 sampleSize;
            uint32 maxPredictionError;
            _GetApproximationParams(sampleSize, maxPredictionError, expectedMin, expectedMax);

            EnableCriticals(priest, true);

            uint32 victimMaxHealth = victim->GetMaxHealth();
            uint32 victimMaxMana = victim->GetMaxPower(POWER_MANA);

            uint32 minBurn = std::numeric_limits<uint32>::max();
            uint32 maxBurn = 0;

            priest->ForceSpellHitResult(SPELL_MISS_NONE);
            for (uint32 i = 0; i < sampleSize; i++)
            {
                TEST_CAST(priest, victim, ClassSpells::Priest::MANA_BURN_RNK_7, SPELL_CAST_OK, TRIGGERED_FULL_MASK);

                uint32 manaBurnt = victimMaxMana - victim->GetPower(POWER_MANA);
                minBurn = std::min(minBurn, manaBurnt);
                maxBurn = std::max(maxBurn, manaBurnt);

                uint32 expectedHealth = victimMaxHealth - manaBurnt / 2;
                ASSERT_INFO("Health: %u, expected: %u", victim->GetHealth(), expectedHealth);
                TEST_ASSERT(victim->GetHealth() == expectedHealth);

                // Reset
                victim->SetFullHealth();
                victim->SetPower(POWER_MANA, victim->GetMaxPower(POWER_MANA));
                Wait(10);
            }
            priest->ResetForceSpellHitResult();
        }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* mage = SpawnPlayer(CLASS_MAGE, RACE_HUMAN);
            TestPlayer* rogue = SpawnPlayer(CLASS_ROGUE, RACE_HUMAN);

            EQUIP_ITEM(priest, 34336); // Sunflare - 292 SP
            priest->DisableRegeneration(true);
            mage->DisableRegeneration(true);
            Wait(5000);

            // Fail
            TEST_CAST(priest, rogue, ClassSpells::Priest::MANA_BURN_RNK_7, SPELL_FAILED_BAD_TARGETS);

            // Mana cost
            uint32 const expectedManaBurnMana = 355;
            TEST_POWER_COST(priest, mage, ClassSpells::Priest::MANA_BURN_RNK_7, POWER_MANA, expectedManaBurnMana);

            // Nothing on rogue
            TEST_ASSERT(rogue->GetHealth() == rogue->GetMaxHealth());

            TestManaBurn(priest, mage, ClassSpellsDamage::Priest::MANA_BURN_RNK_7_MIN, ClassSpellsDamage::Priest::MANA_BURN_RNK_7_MAX);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<ManaBurnTestImpt>();
    }
};

class MassDispelTest : public TestCaseScript
{
public:
    MassDispelTest() : TestCaseScript("spells priest mass_dispel") { }

    class MassDispelTestImpt : public TestCase
    {
    public:
        MassDispelTestImpt() : TestCase(STATUS_INCOMPLETE, true) { }

        TestPlayer* SpawnShaman(Races race, Position spawn)
        {
            TestPlayer* shaman = SpawnPlayer(CLASS_SHAMAN, race, 70, spawn);
            EQUIP_ITEM(shaman, 28523); // Totem
            TEST_CAST(shaman, shaman, ClassSpells::Shaman::MANA_SPRING_TOTEM_RNK_5, SPELL_CAST_OK, TRIGGERED_FULL_MASK);
            TEST_CAST(shaman, shaman, ClassSpells::Shaman::GRACE_OF_AIR_TOTEM_RNK_3, SPELL_CAST_OK, TRIGGERED_FULL_MASK);
            TEST_CAST(shaman, shaman, ClassSpells::Shaman::STRENGTH_OF_EARTH_TOTEM_RNK_6, SPELL_CAST_OK, TRIGGERED_FULL_MASK);
            TEST_CAST(shaman, shaman, ClassSpells::Shaman::FROST_RESISTANCE_TOTEM_RNK_4, SPELL_CAST_OK, TRIGGERED_FULL_MASK);
            return shaman;
        }

        TestPlayer* SpawnHunter(Races race, Position spawn)
        {
            TestPlayer* hunter = SpawnPlayer(CLASS_HUNTER, race, 70, spawn);
            hunter->SummonPet(20673, hunter->GetPositionX(), hunter->GetPositionY(), hunter->GetPositionZ(), 0.0f, HUNTER_PET, 0); // Wind Serpent
            Pet* pet = hunter->GetPet();
            TEST_ASSERT(pet != nullptr);
            return hunter;
        }

        void Test() override
        {
            // Creature
            Creature* creature = SpawnCreature(6);

            // Priest
            TestPlayer* priestA = SpawnPlayer(CLASS_PRIEST, RACE_HUMAN);
            TestPlayer* priestH = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);

            // Setup
            Position spawn(_location);
            // Paladin
            spawn.MoveInFront(spawn, 3.0f);
            SpawnPlayer(CLASS_PALADIN, RACE_HUMAN, 70, spawn);
            TestPlayer* paladinH = SpawnPlayer(CLASS_PALADIN, RACE_BLOODELF, 70, spawn);
            // Mage
            spawn.MoveInFront(spawn, 3.0f);
            TestPlayer* mageA = SpawnPlayer(CLASS_MAGE, RACE_HUMAN, 70, spawn);
            TestPlayer* mageH = SpawnPlayer(CLASS_MAGE, RACE_BLOODELF, 70, spawn);
            // Warlock
            spawn.MoveInFront(spawn, 3.0f);
            SpawnPlayer(CLASS_WARLOCK, RACE_HUMAN, 70, spawn);
            TestPlayer* warlockH = SpawnPlayer(CLASS_WARLOCK, RACE_BLOODELF, 70, spawn);
            // Shaman
            spawn.MoveInFront(spawn, 3.0f);
            TestPlayer* shamanA = SpawnShaman(RACE_DRAENEI, spawn);
            SpawnShaman(RACE_ORC, spawn);
            // Hunter
            spawn.MoveInFront(spawn, 3.0f);
            SpawnHunter(RACE_DWARF, spawn);
            TestPlayer* hunterH = SpawnHunter(RACE_ORC, spawn);
            // Warrior
            spawn.MoveInFront(spawn, 3.0f);
            SpawnPlayer(CLASS_WARRIOR, RACE_HUMAN, 70, spawn);
            SpawnPlayer(CLASS_WARRIOR, RACE_TAUREN, 70, spawn);
            // Druid
            spawn.MoveInFront(spawn, 3.0f);
            SpawnPlayer(CLASS_DRUID, RACE_NIGHTELF, 70, spawn);
            SpawnPlayer(CLASS_DRUID, RACE_TAUREN, 70, spawn);
            // Rogue
            spawn.MoveInFront(spawn, 3.0f);
            SpawnPlayer(CLASS_ROGUE, RACE_NIGHTELF, 70, spawn);
            SpawnPlayer(CLASS_ROGUE, RACE_BLOODELF, 70, spawn);
            // RIP
            spawn.MoveInFront(spawn, 3.0f);
            TestPlayer* ripA = SpawnPlayer(CLASS_ROGUE, RACE_HUMAN, 70, spawn);
            TestPlayer* ripH = SpawnPlayer(CLASS_ROGUE, RACE_ORC, 70, spawn);

            float const dist = 35.0f;
            uint32 const GAS_NOVA = 45855;

            // Get all the units
            std::vector<WorldObject*> targets;
            Trinity::AllWorldObjectsInRange u_check(creature, dist);
            Trinity::WorldObjectListSearcher<Trinity::AllWorldObjectsInRange> searcher(creature, targets, u_check);
            Cell::VisitAllObjects(creature, searcher, dist);

            // Group raid the players
            for (WorldObject* object : targets)
            {
                if (Player* player = object->ToPlayer())
                {                  
                    if (player == priestA || player == priestH)
                        continue;

                    if (player->GetTeam() == Team::ALLIANCE)
                        GroupPlayer(priestA, player);
                    else
                        GroupPlayer(priestH, player);
                }
            }

            // Buff horde raid
            uint32 const SACRED_CANDLE = 17029;
            priestH->AddItem(17029, 2);
            TEST_CAST(priestH, priestH, ClassSpells::Priest::PRAYER_OF_FORTITUDE_RNK_3, SPELL_CAST_OK, TriggerCastFlags(TRIGGERED_CAST_DIRECTLY | TRIGGERED_IGNORE_POWER_AND_REAGENT_COST));
            TEST_CAST(priestH, hunterH, ClassSpells::Priest::PRAYER_OF_FORTITUDE_RNK_3, SPELL_CAST_OK, TriggerCastFlags(TRIGGERED_CAST_DIRECTLY | TRIGGERED_IGNORE_POWER_AND_REAGENT_COST));
            TEST_CAST(priestH, ripH, ClassSpells::Priest::PRAYER_OF_FORTITUDE_RNK_3, SPELL_CAST_OK, TriggerCastFlags(TRIGGERED_CAST_DIRECTLY | TRIGGERED_IGNORE_POWER_AND_REAGENT_COST));

            // Gas Nova for all
            TEST_CAST(creature, creature, GAS_NOVA, SPELL_CAST_OK, TRIGGERED_FULL_MASK);
            // Everybody has the aura except Totems and creature
            for (WorldObject* object : targets)
            {
                if (Unit* unit = object->ToUnit())
                {
                    if(unit->IsTotem() || unit == creature)
                        TEST_HAS_NOT_AURA(unit, GAS_NOVA)
                    else
                        TEST_HAS_AURA(unit, GAS_NOVA)

                    // Buff priest on horde players & pets
                    if (Player* player = object->ToPlayer())
                    {
                        if (player->GetTeam() == Team::HORDE)
                        {   
                            TEST_HAS_AURA(player, ClassSpells::Priest::PRAYER_OF_FORTITUDE_RNK_3);
                            if (Pet* pet = player->GetPet())
                                TEST_HAS_AURA(pet, ClassSpells::Priest::PRAYER_OF_FORTITUDE_RNK_3);
                        }
                    }
                }
            }

            // Mass dispell
            // Alliance: no more gas nova except ripA
            // Horde: gas nova but no more priest buff expect ripH
            TEST_CAST(priestA, shamanA, ClassSpells::Priest::MASS_DISPEL_RNK_1);
            for (WorldObject* object : targets)
            {
                if (Player* player = object->ToPlayer())
                {
                    if (player->GetTeam() == Team::ALLIANCE)
                    {
                        // 10 allies were already dispelled
                        if (player == ripA)
                        {
                            TEST_HAS_AURA(player, GAS_NOVA);
                            continue;
                        }
                        TEST_HAS_NOT_AURA(player, GAS_NOVA)
                        if (Pet* pet = player->ToPet())
                            TEST_HAS_NOT_AURA(pet, GAS_NOVA)
                    }
                    else
                    {
                        // 10 enemies were already dispelled
                        if (player == ripH)
                        {
                            TEST_HAS_AURA(player, GAS_NOVA);
                            TEST_HAS_AURA(player, ClassSpells::Priest::PRAYER_OF_FORTITUDE_RNK_3);
                            continue;
                        }
                        TEST_HAS_AURA(player, GAS_NOVA)
                        TEST_HAS_NOT_AURA(player, ClassSpells::Priest::PRAYER_OF_FORTITUDE_RNK_3);
                        if (Pet* pet = player->ToPet())
                        {
                            TEST_HAS_AURA(pet, GAS_NOVA)
                            TEST_HAS_NOT_AURA(pet, ClassSpells::Priest::PRAYER_OF_FORTITUDE_RNK_3);

                        }
                    }
                }
            }

            // Remove Ice Block, Divine Shield, some banish?
            TEST_CAST(mageH, mageH, ClassSpells::Mage::ICE_BLOCK_RNK_1);
            TEST_CAST(paladinH, paladinH, ClassSpells::Paladin::DIVINE_SHIELD_RNK_2);
            TEST_CAST(mageA, mageA, ClassSpells::Mage::SUMMON_WATER_ELEMENTAL_RNK_1);
            Pet* elem = mageA->GetPet();
            TEST_ASSERT(elem != nullptr);
            TEST_CAST(warlockH, elem, ClassSpells::Warlock::BANISH_RNK_2, SPELL_CAST_OK, TRIGGERED_CAST_DIRECTLY);

            TEST_HAS_AURA(mageH, ClassSpells::Mage::ICE_BLOCK_RNK_1);
            TEST_HAS_AURA(paladinH, ClassSpells::Paladin::DIVINE_SHIELD_RNK_2);
            TEST_HAS_AURA(elem, ClassSpells::Warlock::BANISH_RNK_2);

            TEST_CAST(priestA, mageH, ClassSpells::Priest::MASS_DISPEL_RNK_1);

            TEST_HAS_NOT_AURA(mageH, ClassSpells::Mage::ICE_BLOCK_RNK_1);
            TEST_HAS_NOT_AURA(paladinH, ClassSpells::Paladin::DIVINE_SHIELD_RNK_2);
            TEST_HAS_NOT_AURA(elem, ClassSpells::Warlock::BANISH_RNK_2);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<MassDispelTestImpt>();
    }
};

class PowerWordFortitudeTest : public TestCaseScript
{
public:
    PowerWordFortitudeTest() : TestCaseScript("spells priest power_word_fortitude") { }

    class PowerWordFortitudeTestImpt : public TestCase
    {
    public:
        PowerWordFortitudeTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* warrior = SpawnPlayer(CLASS_WARRIOR, RACE_ORC);

            uint32 powerWordFortitudeBonus = 79;
            uint32 expectedHealth = warrior->GetMaxHealth() + powerWordFortitudeBonus * 10;

            // Mana cost
            uint32 const expectedPowerWordFortitudeMana = 700;
            TEST_POWER_COST(priest, warrior, ClassSpells::Priest::POWER_WORD_FORTITUDE_RNK_7, POWER_MANA, expectedPowerWordFortitudeMana);

            // Health
            ASSERT_INFO("Health: %u, expected: %u", warrior->GetHealth(), expectedHealth);
            TEST_ASSERT(warrior->GetMaxHealth() == expectedHealth);

            // Aura
            TEST_AURA_MAX_DURATION(warrior, ClassSpells::Priest::POWER_WORD_FORTITUDE_RNK_7, 30 * MINUTE * IN_MILLISECONDS);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<PowerWordFortitudeTestImpt>();
    }
};

class PowerWordShieldTest : public TestCaseScript
{
public:
    PowerWordShieldTest() : TestCaseScript("spells priest power_word_shield") { }

    class PowerWordShieldTestImpt : public TestCase
    {
    public:
        PowerWordShieldTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* rogue = SpawnPlayer(CLASS_ROGUE, RACE_HUMAN);

            EQUIP_ITEM(priest, 34335); // Hammer of Sanctification -- 550 BH

            uint32 expectedAbsorb = ClassSpellsDamage::Priest::POWER_WORD_SHIELD_RNK_12 + ClassSpellsCoeff::Priest::POWER_WORD_SHIELD * 550;

            // Mana cost
            uint32 const expectedPowerWordShieldMana = 600;
            TEST_POWER_COST(priest, priest, ClassSpells::Priest::POWER_WORD_SHIELD_RNK_12, POWER_MANA, expectedPowerWordShieldMana);

            // Aura
            TEST_AURA_MAX_DURATION(priest, ClassSpells::Priest::POWER_WORD_SHIELD_RNK_12, 30 * SECOND * IN_MILLISECONDS);
            TEST_AURA_MAX_DURATION(priest, 6788, 15 * SECOND * IN_MILLISECONDS); // Weakened Aura

            // Cooldown
            TEST_HAS_COOLDOWN(priest, ClassSpells::Priest::POWER_WORD_SHIELD_RNK_12, 4 * SECOND);

            // Absorb
            priest->SetFullHealth();
            auto AI = rogue->GetTestingPlayerbotAI();
            int totalDamage = 0;
            for (uint32 i = 0; i < 150; i++)
            {
                rogue->AttackerStateUpdate(priest, BASE_ATTACK);
                auto damageToTarget = AI->GetWhiteDamageDoneInfo(priest);

                TEST_ASSERT(damageToTarget->size() == i + 1);
                auto& data = damageToTarget->back();

                if (data.damageInfo.HitOutCome != MELEE_HIT_NORMAL && data.damageInfo.HitOutCome != MELEE_HIT_CRIT)
                    continue;

                for (uint8 j = 0; j < MAX_ITEM_PROTO_DAMAGES; j++)
                {
                    totalDamage += data.damageInfo.Damages[j].Damage;
                    totalDamage += data.damageInfo.Damages[j].Resist;
                    totalDamage += data.damageInfo.Damages[j].Absorb;
                }
                totalDamage += data.damageInfo.Blocked;

                if (totalDamage < expectedAbsorb)
                {
                    TEST_HAS_AURA(priest, ClassSpells::Priest::POWER_WORD_SHIELD_RNK_12);
                    continue;
                }

                TEST_HAS_NOT_AURA(priest, ClassSpells::Priest::POWER_WORD_SHIELD_RNK_12);
                uint32 expectedHealth = priest->GetMaxHealth() - (totalDamage - expectedAbsorb);
                TEST_ASSERT(priest->GetHealth() == expectedHealth);
                break;
            }
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<PowerWordShieldTestImpt>();
    }
};

class PrayerOfFortitudeTest : public TestCaseScript
{
public:
    PrayerOfFortitudeTest() : TestCaseScript("spells priest prayer_of_fortitude") { }

    class PrayerOfFortitudeTestImpt : public TestCase
    {
    public:
        PrayerOfFortitudeTestImpt() : TestCase(STATUS_PASSING, true) { }

        void TestPrayerOfFortitude(TestPlayer* priest, Unit* warrior, uint32 spellId, uint32 reagentId, uint32 manaCost, uint32 staminaBonus, uint32 priestStartHealth, uint32 warriorStartHealth)
        {
            uint32 expectedPriestHealth = priestStartHealth + staminaBonus * 10;
            uint32 expectedWarriorHealth = warriorStartHealth + staminaBonus * 10;

            // Mana cost
            priest->AddItem(reagentId, 1); // Reagent
            TEST_POWER_COST(priest, warrior, spellId, POWER_MANA, manaCost);
            TEST_ASSERT(priest->GetItemCount(reagentId, false) == 0);

            // Aura
            TEST_AURA_MAX_DURATION(warrior, spellId, 1 * HOUR * IN_MILLISECONDS);
            TEST_AURA_MAX_DURATION(priest, spellId, 1 * HOUR * IN_MILLISECONDS);

            // Health
            TEST_ASSERT(priest->GetMaxHealth() == expectedPriestHealth);
            TEST_ASSERT(warrior->GetMaxHealth() == expectedWarriorHealth);
        }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* warrior = SpawnPlayer(CLASS_WARRIOR, RACE_ORC);

            GroupPlayer(priest, warrior);

            uint32 const startPriestHealth = priest->GetMaxHealth();
            uint32 const startWarriorHealth = warrior->GetMaxHealth();

            uint32 const HOLY_CANDLE = 17028;
            uint32 const SACRED_CANDLE = 17029;

            TestPrayerOfFortitude(priest, warrior, ClassSpells::Priest::PRAYER_OF_FORTITUDE_RNK_1, HOLY_CANDLE, 1200, 43, startPriestHealth, startWarriorHealth);
            TestPrayerOfFortitude(priest, warrior, ClassSpells::Priest::PRAYER_OF_FORTITUDE_RNK_2, SACRED_CANDLE, 1500, 54, startPriestHealth, startWarriorHealth);
            TestPrayerOfFortitude(priest, warrior, ClassSpells::Priest::PRAYER_OF_FORTITUDE_RNK_3, SACRED_CANDLE, 1800, 79, startPriestHealth, startWarriorHealth);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<PrayerOfFortitudeTestImpt>();
    }
};

class ShackleUndeadTest : public TestCaseScript
{
public:
    ShackleUndeadTest() : TestCaseScript("spells priest shackle_undead") { }

    class ShackleUndeadTestImpt : public TestCase
    {
    public:
        ShackleUndeadTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            Creature* creature0 = SpawnCreature(6, true);

            Position spawn(_location);
            spawn.MoveInFront(spawn, 20.0f);
            Creature* creature1 = SpawnCreatureWithPosition(spawn, 16525); // Undead
            Creature* creature2 = SpawnCreatureWithPosition(spawn, 16525); // Undead

            // Fail
            TEST_CAST(priest, creature0, ClassSpells::Priest::SHACKLE_UNDEAD_RNK_3, SPELL_FAILED_BAD_TARGETS);

            Wait(3000);

            // Mana cost
            uint32 const expectedShackleUndeadMana = 150;
            TEST_POWER_COST(priest, creature1, ClassSpells::Priest::SHACKLE_UNDEAD_RNK_3, POWER_MANA, expectedShackleUndeadMana);

            // Aura
            TEST_AURA_MAX_DURATION(creature1, ClassSpells::Priest::SHACKLE_UNDEAD_RNK_3, 50 * SECOND * IN_MILLISECONDS);

            creature2->Attack(priest, false);
            for (int count = 0; count < 10; count++)
            {
                if (!creature2->IsMovementPreventedByCasting())
                {
                    Wait(1000);
                    continue;
                }
                TEST_CAST(priest, creature2, ClassSpells::Priest::SHACKLE_UNDEAD_RNK_3, SPELL_CAST_OK, TriggerCastFlags(TRIGGERED_CAST_DIRECTLY | TRIGGERED_IGNORE_POWER_AND_REAGENT_COST));
                TEST_HAS_AURA(creature2, ClassSpells::Priest::SHACKLE_UNDEAD_RNK_3);
                TEST_HAS_NOT_AURA(creature1, ClassSpells::Priest::SHACKLE_UNDEAD_RNK_3);
            }
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<ShackleUndeadTestImpt>();
    }
};

class AbolishDiseaseTest : public TestCaseScript
{
public:
    AbolishDiseaseTest() : TestCaseScript("spells priest abolish_disease") { }

    class AbolishDiseaseTestImpt : public TestCase
    {
    public:
        AbolishDiseaseTestImpt() : TestCase(STATUS_KNOWN_BUG, true) { }

        void TestDispelDisease(TestPlayer* victim, uint32 Disease1, uint32 Disease2, uint32 Disease3, uint32 Disease4, uint32 Disease5, int8 count)
        {
            victim->SetFullHealth();
            ASSERT_INFO("TestDispelDisease maximum trials reached");
            TEST_ASSERT(count < 10);
            count++;

            uint32 tickTime = 5 * SECOND * IN_MILLISECONDS;

            if (victim->HasAura(Disease5))
            {
                TEST_ASSERT(victim->HasAura(Disease4));
                TEST_ASSERT(victim->HasAura(Disease3));
                TEST_ASSERT(victim->HasAura(Disease2));
                TEST_ASSERT(victim->HasAura(Disease1));
                Wait(tickTime); // wait for next tick
                TestDispelDisease(victim, Disease1, Disease2, Disease3, Disease4, Disease5, count);
            }
            else
            {
                if (victim->HasAura(Disease4))
                {
                    TEST_ASSERT(victim->HasAura(Disease3));
                    TEST_ASSERT(victim->HasAura(Disease2));
                    TEST_ASSERT(victim->HasAura(Disease1));
                    Wait(tickTime); // wait for next tick
                    TestDispelDisease(victim, Disease1, Disease2, Disease3, Disease4, Disease5, count);
                }
                else
                {
                    if (victim->HasAura(Disease3))
                    {
                        TEST_ASSERT(victim->HasAura(Disease2));
                        TEST_ASSERT(victim->HasAura(Disease1));
                        Wait(tickTime); // wait for next tick
                        TestDispelDisease(victim, Disease1, Disease2, Disease3, Disease4, Disease5, count);
                    }
                    else
                    {
                        if (victim->HasAura(Disease2))
                        {
                            TEST_ASSERT(victim->HasAura(Disease1));
                            Wait(tickTime); // wait for next tick
                            TestDispelDisease(victim, Disease1, Disease2, Disease3, Disease4, Disease5, count);
                        }
                        else
                        {
                            if (victim->HasAura(Disease1))
                            {
                                Wait(tickTime); // wait for next tick
                                TestDispelDisease(victim, Disease1, Disease2, Disease3, Disease4, Disease5, count);
                            }
                        }
                    }
                }
            }
        }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* warrior = SpawnPlayer(CLASS_WARRIOR, RACE_TAUREN);

            // setup
            priest->DisableRegeneration(true);

            uint32 const WEAKENING_DISEASE = 18633;
            uint32 const SPORE_DISEASE     = 31423;
            uint32 const FEVERED_DISEASE   = 34363;
            uint32 const DISEASE_BUFFET    = 46481;
            uint32 const VOLATILE_DISEASE  = 46483;

            // Apply
            warrior->AddAura(WEAKENING_DISEASE, warrior);
            Wait(1);
            warrior->AddAura(SPORE_DISEASE, warrior);
            Wait(1);
            warrior->AddAura(FEVERED_DISEASE, warrior);
            Wait(1);
            warrior->AddAura(DISEASE_BUFFET, warrior);
            Wait(1);
            warrior->AddAura(VOLATILE_DISEASE, warrior);
            Wait(1);

            // Mana cost
            uint32 const expectedAbolishDiseaseMana = 314;
            TEST_POWER_COST(priest, warrior, ClassSpells::Priest::ABOLISH_DISEASE_RNK_1, POWER_MANA, expectedAbolishDiseaseMana);

            // Aura duration
            TEST_AURA_MAX_DURATION(warrior, ClassSpells::Priest::ABOLISH_DISEASE_RNK_1, 20 * SECOND * IN_MILLISECONDS);

            Wait(500);
            int8 count = 0;
            TestDispelDisease(warrior, WEAKENING_DISEASE, SPORE_DISEASE, FEVERED_DISEASE, DISEASE_BUFFET, VOLATILE_DISEASE, count);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<AbolishDiseaseTestImpt>();
    }
};

class BindingHealTest : public TestCaseScript
{
public:
    BindingHealTest() : TestCaseScript("spells priest binding_heal") { }

    class BindingHealTestImpt : public TestCase
    {
    public:
        BindingHealTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* rogue = SpawnPlayer(CLASS_ROGUE, RACE_BLOODELF);

            EQUIP_ITEM(priest, 34335); // Hammer of Sanctification -- 550 BH

            // Mana cost
            uint32 const expectedBindingHealMana = 705;
            TEST_POWER_COST(priest, rogue, ClassSpells::Priest::BINDING_HEAL_RNK_1, POWER_MANA, expectedBindingHealMana);
            
            // Heal
            float const bindingHealCastTime = 1.5f;
            float const bindingHealCoeff = bindingHealCastTime / 3.5f;
            uint32 const bonusHeal = 550 * bindingHealCoeff;
            uint32 const bindingHealMin = ClassSpellsDamage::Priest::BINDING_HEAL_RNK_1_MIN + bonusHeal;
            uint32 const bindingHealMax = ClassSpellsDamage::Priest::BINDING_HEAL_RNK_1_MAX + bonusHeal;
            TEST_DIRECT_HEAL(priest, priest, ClassSpells::Priest::BINDING_HEAL_RNK_1, bindingHealMin, bindingHealMax, false);
            TEST_DIRECT_HEAL(priest, priest, ClassSpells::Priest::BINDING_HEAL_RNK_1, bindingHealMin * 1.5f, bindingHealMax * 1.5f, true);
            TEST_DIRECT_HEAL(priest, rogue, ClassSpells::Priest::BINDING_HEAL_RNK_1, bindingHealMin, bindingHealMax, false);
            TEST_DIRECT_HEAL(priest, rogue, ClassSpells::Priest::BINDING_HEAL_RNK_1, bindingHealMin * 1.5f, bindingHealMax * 1.5f, true);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<BindingHealTestImpt>();
    }
};

class CureDiseaseTest : public TestCaseScript
{
public:
    CureDiseaseTest() : TestCaseScript("spells priest cure_disease") { }

    class CureDiseaseTestImpt : public TestCase
    {
    public:
        CureDiseaseTestImpt() : TestCase(STATUS_KNOWN_BUG, true) { }

        void TestDispelDisease(TestPlayer* victim, uint32 Disease1, uint32 Disease2, uint32 Disease3, uint32 Disease4, uint32 Disease5, int8 count)
        {
            victim->SetFullHealth();
            ASSERT_INFO("TestDispelDisease maximum trials reached");
            TEST_ASSERT(count < 10);
            count++;

            TEST_CAST(victim, victim, ClassSpells::Priest::CURE_DISEASE_RNK_1, SPELL_CAST_OK, TRIGGERED_FULL_MASK);

            if (victim->HasAura(Disease5))
            {
                TEST_ASSERT(victim->HasAura(Disease4));
                TEST_ASSERT(victim->HasAura(Disease3));
                TEST_ASSERT(victim->HasAura(Disease2));
                TEST_ASSERT(victim->HasAura(Disease1));
                TestDispelDisease(victim, Disease1, Disease2, Disease3, Disease4, Disease5, count);
            }
            else
            {
                if (victim->HasAura(Disease4))
                {
                    TEST_ASSERT(victim->HasAura(Disease3));
                    TEST_ASSERT(victim->HasAura(Disease2));
                    TEST_ASSERT(victim->HasAura(Disease1));
                    TestDispelDisease(victim, Disease1, Disease2, Disease3, Disease4, Disease5, count);
                }
                else
                {
                    if (victim->HasAura(Disease3))
                    {
                        TEST_ASSERT(victim->HasAura(Disease2));
                        TEST_ASSERT(victim->HasAura(Disease1));
                        TestDispelDisease(victim, Disease1, Disease2, Disease3, Disease4, Disease5, count);
                    }
                    else
                    {
                        if (victim->HasAura(Disease2))
                        {
                            TEST_ASSERT(victim->HasAura(Disease1));
                            TestDispelDisease(victim, Disease1, Disease2, Disease3, Disease4, Disease5, count);
                        }
                        else
                        {
                            if (victim->HasAura(Disease1))
                            {
                                TestDispelDisease(victim, Disease1, Disease2, Disease3, Disease4, Disease5, count);
                            }
                        }
                    }
                }
            }
        }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* warrior = SpawnPlayer(CLASS_WARRIOR, RACE_TAUREN);

            // setup
            priest->DisableRegeneration(true);

            uint32 const WEAKENING_DISEASE = 18633;
            uint32 const SPORE_DISEASE = 31423;
            uint32 const FEVERED_DISEASE = 34363;
            uint32 const DISEASE_BUFFET = 46481;
            uint32 const VOLATILE_DISEASE = 46483;

            // Apply
            warrior->AddAura(WEAKENING_DISEASE, warrior);
            Wait(1);
            warrior->AddAura(SPORE_DISEASE, warrior);
            Wait(1);
            warrior->AddAura(FEVERED_DISEASE, warrior);
            Wait(1);
            warrior->AddAura(DISEASE_BUFFET, warrior);
            Wait(1);
            warrior->AddAura(VOLATILE_DISEASE, warrior);
            Wait(1);

            // Mana cost
            uint32 const expectedCureDiseaseMana = 314;
            TEST_POWER_COST(priest, warrior, ClassSpells::Priest::CURE_DISEASE_RNK_1, POWER_MANA, expectedCureDiseaseMana);

            Wait(500);
            int8 count = 0;
            TestDispelDisease(warrior, WEAKENING_DISEASE, SPORE_DISEASE, FEVERED_DISEASE, DISEASE_BUFFET, VOLATILE_DISEASE, count);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<CureDiseaseTestImpt>();
    }
};

class FlashHealTest : public TestCaseScript
{
public:
    FlashHealTest() : TestCaseScript("spells priest greater_heal") { }

    class FlashHealTestImpt : public TestCase
    {
    public:
        FlashHealTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);

            EQUIP_ITEM(priest, 34335); // Hammer of Sanctification -- 550 BH

            // Mana cost
            uint32 const expectedFlashHealMana = 470;
            TEST_POWER_COST(priest, priest, ClassSpells::Priest::FLASH_HEAL_RNK_9, POWER_MANA, expectedFlashHealMana);

            // Heal
            float const greaterHealCastTime = 1.5f;
            float const greaterHealCoeff = greaterHealCastTime / 3.5f;
            uint32 const bonusHeal = 550 * greaterHealCoeff;
            uint32 const greaterHealMin = ClassSpellsDamage::Priest::FLASH_HEAL_RNK_9_MIN + bonusHeal;
            uint32 const greaterHealMax = ClassSpellsDamage::Priest::FLASH_HEAL_RNK_9_MAX + bonusHeal;
            TEST_DIRECT_HEAL(priest, priest, ClassSpells::Priest::FLASH_HEAL_RNK_9, greaterHealMin, greaterHealMax, false);
            TEST_DIRECT_HEAL(priest, priest, ClassSpells::Priest::FLASH_HEAL_RNK_9, greaterHealMin * 1.5f, greaterHealMax * 1.5f, true);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<FlashHealTestImpt>();
    }
};

class GreaterHealTest : public TestCaseScript
{
public:
    GreaterHealTest() : TestCaseScript("spells priest greater_heal") { }

    class GreaterHealTestImpt : public TestCase
    {
    public:
        GreaterHealTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);

            EQUIP_ITEM(priest, 34335); // Hammer of Sanctification -- 550 BH

            // Mana cost
            uint32 const expectedGreaterHealMana = 825;
            TEST_POWER_COST(priest, priest, ClassSpells::Priest::GREATER_HEAL_RNK_7, POWER_MANA, expectedGreaterHealMana);

            // Heal
            float const greaterHealCastTime = 3.0f;
            float const greaterHealCoeff = greaterHealCastTime / 3.5f;
            uint32 const bonusHeal = 550 * greaterHealCoeff;
            uint32 const greaterHealMin = ClassSpellsDamage::Priest::GREATER_HEAL_RNK_7_MIN + bonusHeal;
            uint32 const greaterHealMax = ClassSpellsDamage::Priest::GREATER_HEAL_RNK_7_MAX + bonusHeal;
            TEST_DIRECT_HEAL(priest, priest, ClassSpells::Priest::GREATER_HEAL_RNK_7, greaterHealMin, greaterHealMax, false);
            TEST_DIRECT_HEAL(priest, priest, ClassSpells::Priest::GREATER_HEAL_RNK_7, greaterHealMin * 1.5f, greaterHealMax * 1.5f, true);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<GreaterHealTestImpt>();
    }
};

class HealTest : public TestCaseScript
{
public:
    HealTest() : TestCaseScript("spells priest heal") { }

    class HealTestImpt : public TestCase
    {
    public:
        HealTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);

            EQUIP_ITEM(priest, 34335); // Hammer of Sanctification -- 550 BH

            // Mana cost
            uint32 const expectedHealMana = 305;
            TEST_POWER_COST(priest, priest, ClassSpells::Priest::HEAL_RNK_4, POWER_MANA, expectedHealMana);

            // Heal
            int const spellMaxLevel = 39;
            int const spellBaseLevel = 34;
            float const spellRealPointsPerLevel = 4.5f;
            float const downrankingCoeff = (spellMaxLevel + 6) / 70.0f;
            uint32 const bonusPoints = (spellMaxLevel - spellBaseLevel) * spellRealPointsPerLevel;
            uint32 const bonusMaxPoints = 93; // effectDieSides1
            float const healCastTime = 3.0f;
            float const healCoeff = healCastTime / 3.5f;
            uint32 const bonusHeal = 550 * healCoeff * downrankingCoeff;
            uint32 const healMin = ClassSpellsDamage::Priest::HEAL_RNK_4_MIN + bonusPoints + bonusHeal;
            uint32 const healMax = ClassSpellsDamage::Priest::HEAL_RNK_4_MIN + bonusPoints + bonusMaxPoints + bonusHeal;
            TEST_DIRECT_HEAL(priest, priest, ClassSpells::Priest::HEAL_RNK_4, healMin, healMax, false);
            TEST_DIRECT_HEAL(priest, priest, ClassSpells::Priest::HEAL_RNK_4, healMin * 1.5f, healMax * 1.5f, true);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<HealTestImpt>();
    }
};

class HolyFireTest : public TestCaseScript
{
public:
    HolyFireTest() : TestCaseScript("spells priest holy_fire") { }

    class HolyFireTestImpt : public TestCase
    {
    public:
        HolyFireTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            Creature* creature = SpawnCreature();

            EQUIP_ITEM(priest, 34336); // Sunflare -- 292 BH

            // Mana cost
            uint32 const expectedHolyFireMana = 290;
            TEST_POWER_COST(priest, creature, ClassSpells::Priest::HOLY_FIRE_RNK_9, POWER_MANA, expectedHolyFireMana);

            // Direct
            float const holyFireCoeff = ClassSpellsCoeff::Priest::HOLY_FIRE;
            uint32 const bonusSP = 292 * holyFireCoeff;
            uint32 const holyFireMin = ClassSpellsDamage::Priest::HOLY_FIRE_RNK_9_MIN + bonusSP;
            uint32 const holyFireMax = ClassSpellsDamage::Priest::HOLY_FIRE_RNK_9_MAX + bonusSP;
            TEST_DIRECT_SPELL_DAMAGE(priest, creature, ClassSpells::Priest::HOLY_FIRE_RNK_9, holyFireMin, holyFireMax, false);
            TEST_DIRECT_SPELL_DAMAGE(priest, creature, ClassSpells::Priest::HOLY_FIRE_RNK_9, holyFireMin * 1.5f, holyFireMax * 1.5f, true);
            
            // DoT -- wrong DoT Coeff
            float const holyFireDoTCoeff = ClassSpellsCoeff::Priest::HOLY_FIRE_DOT;
            uint32 const bonusDoTSP = 292 * holyFireDoTCoeff;
            uint32 const holyFireDoT = (ClassSpellsDamage::Priest::HOLY_FIRE_RNK_9_TOTAL + bonusDoTSP);
            TEST_DOT_DAMAGE(priest, creature, ClassSpells::Priest::HOLY_FIRE_RNK_9, holyFireDoT, true);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<HolyFireTestImpt>();
    }
};

class LesserHealTest : public TestCaseScript
{
public:
    LesserHealTest() : TestCaseScript("spells priest lesser_heal") { }

    class LesserHealTestImpt : public TestCase
    {
    public:
        LesserHealTestImpt() : TestCase(STATUS_INCOMPLETE, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);

            EQUIP_ITEM(priest, 34335); // Hammer of Sanctification -- 550 BH

            // Mana cost
            uint32 const expectedLesserHealMana = 75;
            TEST_POWER_COST(priest, priest, ClassSpells::Priest::LESSER_HEAL_RNK_3, POWER_MANA, expectedLesserHealMana);

            // Lesser Heal
            int const spellMaxLevel = 15;
            int const spellBaseLevel = 10;
            float const spellRealPointsPerLevel = 1.6f;
            float const downrankingCoeff = (spellMaxLevel + 6) / 70.0f; //  (spellMaxLevel + 5) / playerLevel <- downranking coeff formula
            float const below20penaltyCoeff = 1.0f - ((20.0f - spellBaseLevel) * 0.0375f);
            uint32 const bonusPoints = (spellMaxLevel - spellBaseLevel) * spellRealPointsPerLevel;
            uint32 const bonusMaxPoints = 23; // effectDieSides1
            float const healCastTime = 2.5f;
            float const healCoeff = healCastTime / 3.5f;
            uint32 const bonusLesserHeal = 550 * healCoeff * downrankingCoeff * below20penaltyCoeff;
            uint32 const healMin = ClassSpellsDamage::Priest::LESSER_HEAL_RNK_3_MIN + bonusPoints + bonusLesserHeal;
            uint32 const healMax = ClassSpellsDamage::Priest::LESSER_HEAL_RNK_3_MAX + bonusPoints + bonusMaxPoints + bonusLesserHeal;
            TEST_DIRECT_HEAL(priest, priest, ClassSpells::Priest::LESSER_HEAL_RNK_3, healMin, healMax, false);
            TEST_DIRECT_HEAL(priest, priest, ClassSpells::Priest::LESSER_HEAL_RNK_3, healMin * 1.5f, healMax * 1.5f, true);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<LesserHealTestImpt>();
    }
};

class PrayerOfHealingTest : public TestCaseScript
{
public:
    PrayerOfHealingTest() : TestCaseScript("spells priest prayer_of_healing") { }

    class PrayerOfHealingTestImpt : public TestCase
    {
    public:
        PrayerOfHealingTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            Position spawn(_location);
            spawn.MoveInFront(spawn, 3.0f);
            TestPlayer* priest2 = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF, 70, spawn);
            spawn.MoveInFront(spawn, 3.0f);
            TestPlayer* priest3 = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF, 70, spawn);
            spawn.MoveInFront(spawn, 19.0f);
            TestPlayer* priest4 = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF, 70, spawn);
            spawn.MoveInFront(spawn, 10.0f);
            TestPlayer* priest5 = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF, 70, spawn);
            TestPlayer* priest6 = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF, 70, spawn);

            // Get all the units
            std::vector<WorldObject*> targets;
            Trinity::AllWorldObjectsInRange u_check(priest, 50.0f);
            Trinity::WorldObjectListSearcher<Trinity::AllWorldObjectsInRange> searcher(priest, targets, u_check);
            Cell::VisitAllObjects(priest, searcher, 50.0f);

            // Reverse array
            std::reverse(std::begin(targets), std::end(targets));

            // Group raid the players
            for (WorldObject* object : targets)
            {
                if (Player* player = object->ToPlayer())
                {
                    player->DisableRegeneration(true);
                    player->SetHealth(1);

                    if (player == priest)
                        continue;

                    GroupPlayer(priest, player);
                }
            }

            spawn.MoveInFront(spawn, -36.0f);
            priest6->Relocate(spawn);

            TEST_ASSERT(!priest->GetGroup()->SameSubGroup(priest, priest6));

            EQUIP_ITEM(priest, 34335); // Hammer of Sanctification -- 550 BH

            // Mana cost
            uint32 const expectedPrayerOfHealingMana = 1255;
            TEST_POWER_COST(priest, priest, ClassSpells::Priest::PRAYER_OF_HEALING_RNK_6, POWER_MANA, expectedPrayerOfHealingMana);

            // Raid & Range
            TEST_ASSERT(priest->GetHealth() > 1);
            TEST_ASSERT(priest2->GetHealth() > 1);
            TEST_ASSERT(priest3->GetHealth() > 1);
            TEST_ASSERT(priest4->GetHealth() > 1);
            TEST_ASSERT(priest5->GetHealth() == 1); // 35m
            TEST_ASSERT(priest6->GetHealth() == 1); // not in the same group

            // Heal
            float const prayerOfHealingCastTime = 3.0f;
            float const prayerOfHealingCoeff = prayerOfHealingCastTime / 3.5f / 2.0f;
            uint32 const bonusHeal = 550 * prayerOfHealingCoeff;
            uint32 const prayerOfHealingMin = ClassSpellsDamage::Priest::PRAYER_OF_HEALING_RNK_6_MIN + bonusHeal;
            uint32 const prayerOfHealingMax = ClassSpellsDamage::Priest::PRAYER_OF_HEALING_RNK_6_MAX + bonusHeal;
            TEST_DIRECT_HEAL(priest, priest, ClassSpells::Priest::PRAYER_OF_HEALING_RNK_6, prayerOfHealingMin, prayerOfHealingMax, false);
            TEST_DIRECT_HEAL(priest, priest, ClassSpells::Priest::PRAYER_OF_HEALING_RNK_6, prayerOfHealingMin * 1.5f, prayerOfHealingMax * 1.5f, true);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<PrayerOfHealingTestImpt>();
    }
};

class PrayerOfMendingTest : public TestCaseScript
{
public:
    PrayerOfMendingTest() : TestCaseScript("spells priest prayer_of_mending") { }

    class PrayerOfMendingTestImpt : public TestCase
    {
    public:
        PrayerOfMendingTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);

            Position spawn(_location);
            spawn.MoveInFront(spawn, -2.0f);
            TestPlayer* warlock = SpawnPlayer(CLASS_WARLOCK, RACE_BLOODELF, 70, spawn);
            spawn.MoveInFront(spawn, -2.0f);
            TestPlayer* priest2 = SpawnPlayer(CLASS_PRIEST, RACE_HUMAN, 70, spawn);
            EQUIP_ITEM(priest, 34335); // Hammer of Sanctification -- 550 BH

            // Failed
            TEST_CAST(priest, warlock, ClassSpells::Priest::PRAYER_OF_MENDING_RNK_1, SPELL_FAILED_BAD_TARGETS);
            GroupPlayer(priest, warlock);

            uint32 const expectedPrayerOfMendingMana = 390;
            TEST_POWER_COST(priest, warlock, ClassSpells::Priest::PRAYER_OF_MENDING_RNK_1, POWER_MANA, expectedPrayerOfMendingMana);

            TEST_HAS_COOLDOWN(priest, ClassSpells::Priest::PRAYER_OF_MENDING_RNK_1, 10 * SECOND);

            TEST_AURA_MAX_DURATION(warlock, ClassSpells::Priest::PRAYER_OF_MENDING_RNK_1_BUFF, 30 * SECOND * IN_MILLISECONDS);

            TEST_AURA_CHARGE(warlock, ClassSpells::Priest::PRAYER_OF_MENDING_RNK_1_BUFF, 5);

            // Changed target and 1 charge less
            priest2->ForceMeleeHitResult(MELEE_HIT_NORMAL);
            priest2->AttackerStateUpdate(warlock);
            Wait(1);
            TEST_AURA_CHARGE(priest, ClassSpells::Priest::PRAYER_OF_MENDING_RNK_1_BUFF, 4);

            // Heal
            auto AI = priest->GetTestingPlayerbotAI(); //on retail, healing appears done by target on combat journal
            auto healingToTarget = AI->GetHealingDoneInfo(warlock);
            TEST_ASSERT(healingToTarget != nullptr);
            //SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(ClassSpells::Priest::PRAYER_OF_MENDING_RNK_1);
            TEST_ASSERT(healingToTarget->size() == 1);

            uint32 heal = healingToTarget->begin()->healing;

            float const prayerOfMendingCoeff = ClassSpellsCoeff::Priest::PRAYER_OF_MENDING;
            uint32 const bonusHeal = 550 * prayerOfMendingCoeff;
            uint32 const prayerOfMending = ClassSpellsDamage::Priest::PRAYER_OF_MENDING_RNK_1 + bonusHeal;

            ASSERT_INFO("Heal: %u, expected: %u", heal, prayerOfMending);
            TEST_ASSERT(Between(heal, prayerOfMending - 1, prayerOfMending + 1));
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<PrayerOfMendingTestImpt>();
    }
};

class RenewTest : public TestCaseScript
{
public:
    RenewTest() : TestCaseScript("spells priest renew") { }

    class RenewTestImpt : public TestCase
    {
    public:
        RenewTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);

            EQUIP_ITEM(priest, 34335); // Hammer of Sanctification -- 550 BH

            // Mana cost
            uint32 const expectedRenewMana = 450;
            TEST_POWER_COST(priest, priest, ClassSpells::Priest::RENEW_RNK_12, POWER_MANA, expectedRenewMana);

            // Heal
            float const renewDuration = 15.0f;
            float const renewCoeff = renewDuration / 15.0f;
            uint32 const bonusHeal = 550 * renewCoeff;
            uint32 const renewTicks = 5.0f * floor((ClassSpellsDamage::Priest::RENEW_RNK_12_TOTAL + bonusHeal) / 5.0f);
            TEST_DOT_DAMAGE(priest, priest, ClassSpells::Priest::RENEW_RNK_12, renewTicks, true);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<RenewTestImpt>();
    }
};

class ResurrectionTest : public TestCaseScript
{
public:
    ResurrectionTest() : TestCaseScript("spells priest resurrection") { }

    class ResurrectionTestImpt : public TestCase
    {
    public:
        ResurrectionTestImpt() : TestCase(STATUS_INCOMPLETE, true) { }

        void TestResurrection(TestPlayer* caster, TestPlayer* victim, uint32 spellId, uint32 manaCost, uint32 expectedHealth, uint32 expectedMana)
        {
            victim->KillSelf(true);
            TEST_POWER_COST(caster, victim, spellId, POWER_MANA, manaCost);
            //victim->AcceptRessurectRequest();
            TEST_ASSERT(victim->GetHealth() == expectedHealth);
            TEST_ASSERT(victim->GetPower(POWER_MANA) == expectedMana);
        }

        void Test() override
        {
            TestPlayer* druid = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* ally = SpawnPlayer(CLASS_DRUID, RACE_TAUREN);
            TestPlayer* enemy = SpawnPlayer(CLASS_DRUID, RACE_NIGHTELF);

            uint32 manaCost = 1572;

            TestResurrection(druid, ally, ClassSpells::Priest::RESURRECTION_RNK_1, manaCost, 70, 135);
            TestResurrection(druid, enemy, ClassSpells::Priest::RESURRECTION_RNK_2, manaCost, 160, 300);
            TestResurrection(druid, ally, ClassSpells::Priest::RESURRECTION_RNK_3, manaCost, 300, 520);
            TestResurrection(druid, enemy, ClassSpells::Priest::RESURRECTION_RNK_4, manaCost, 500, 750);
            TestResurrection(druid, ally, ClassSpells::Priest::RESURRECTION_RNK_5, manaCost, 750, 1000);
            TestResurrection(druid, enemy, ClassSpells::Priest::RESURRECTION_RNK_6, manaCost, 1100, 1150);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<ResurrectionTestImpt>();
    }
};

class SmiteTest : public TestCaseScript
{
public:
    SmiteTest() : TestCaseScript("spells priest smite") { }

    class SmiteTestImpt : public TestCase
    {
    public:
        SmiteTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            Creature* dummy = SpawnCreature();

            EQUIP_ITEM(priest, 34336); // Sunflare -- 292 SP

            // Mana cost
            uint32 const expectedSmiteMana = 385;
            TEST_POWER_COST(priest, dummy, ClassSpells::Priest::SMITE_RNK_10, POWER_MANA, expectedSmiteMana);

            // Heal
            float const smiteCastTime = 2.5f;
            float const smiteCoeff = smiteCastTime / 3.5f;
            uint32 const bonusHeal = 292 * smiteCoeff;
            uint32 const smiteMin = ClassSpellsDamage::Priest::SMITE_RNK_10_MIN + bonusHeal;
            uint32 const smiteMax = ClassSpellsDamage::Priest::SMITE_RNK_10_MAX + bonusHeal;
            TEST_DIRECT_SPELL_DAMAGE(priest, dummy, ClassSpells::Priest::SMITE_RNK_10, smiteMin, smiteMax, false);
            TEST_DIRECT_SPELL_DAMAGE(priest, dummy, ClassSpells::Priest::SMITE_RNK_10, smiteMin * 1.5f, smiteMax * 1.5f, true);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<SmiteTestImpt>();
    }
};

class FadeTest : public TestCaseScript
{
public:
    FadeTest() : TestCaseScript("spells priest fade") { }

    class FadeTestImpt : public TestCase
    {
    public:
        FadeTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* tank = SpawnPlayer(CLASS_WARRIOR, RACE_HUMAN);
            Creature* creature = SpawnCreature(16878, true); // Shattered Hand Berserker -- Lvl 61

            // Setup
            tank->SetMaxHealth(30000);
            tank->SetFullHealth();
            FORCE_CAST(tank, tank, ClassSpells::Warrior::DEFENSIVE_STANCE_RNK_1);
            EQUIP_ITEM(tank, 34185); // Shield
            tank->SetPower(POWER_RAGE, 100 * 10);
            tank->Attack(creature, true);
            Wait(500);
            TEST_ASSERT(creature->GetTarget() == tank->GetGUID());

            // Threat
            FORCE_CAST(priest, creature, ClassSpells::Priest::SHADOW_WORD_DEATH_RNK_2);
            Wait(2000);
            uint32 startThreat = creature->GetThreatManager().GetThreat(priest);

            TEST_ASSERT(startThreat > creature->GetThreatManager().GetThreat(tank));
            ASSERT_INFO("Wrong target. tank threat: %f, priest threat: %f", creature->GetThreatManager().GetThreat(tank), creature->GetThreatManager().GetThreat(priest));
            TEST_ASSERT(creature->GetTarget() == priest->GetGUID());

            // Fade
            uint32 const expectedFadeMana = 330;
            TEST_POWER_COST(priest, priest, ClassSpells::Priest::FADE_RNK_7, POWER_MANA, expectedFadeMana);

            // Aura duration
            TEST_AURA_MAX_DURATION(priest, ClassSpells::Priest::FADE_RNK_7, 10 * SECOND * IN_MILLISECONDS);

            // Cooldown
            TEST_HAS_COOLDOWN(priest, ClassSpells::Priest::FADE_RNK_7, 30 * SECOND);

            // Effect
            uint32 const fadeFactor = 1500;
            uint32 expectedThreat = startThreat - fadeFactor;
            TEST_ASSERT(creature->GetThreatManager().GetThreat(priest) == expectedThreat);

            // http://wowwiki.wikia.com/wiki/Fade?oldid=1431850
            // When Fade's duration expires, the **next threat-generating action** will generate normal threat plus all the threat that was originally Faded from the target.
            Wait(11000);
            TEST_HAS_NOT_AURA(priest, ClassSpells::Priest::FADE_RNK_7);
            TEST_ASSERT(creature->GetThreatManager().GetThreat(priest) == expectedThreat);

            FORCE_CAST(priest, creature, ClassSpells::Priest::SHADOW_WORD_DEATH_RNK_2);
            Wait(2000);
            TEST_ASSERT(creature->GetThreatManager().GetThreat(priest) > expectedThreat);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<FadeTestImpt>();
    }
};

class MindBlastTest : public TestCaseScript
{
public:
    MindBlastTest() : TestCaseScript("spells priest mind_blast") { }

    class MindBlastTestImpt : public TestCase
    {
    public:
        MindBlastTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            Creature* dummy = SpawnCreature();

            EQUIP_ITEM(priest, 34336); // Sunflare -- 292 SP

            // Mana cost
            uint32 const expectedMindBlastMana = 450;
            TEST_POWER_COST(priest, dummy, ClassSpells::Priest::MIND_BLAST_RNK_11, POWER_MANA, expectedMindBlastMana);

            // Cooldown
            TEST_HAS_COOLDOWN(priest, ClassSpells::Priest::MIND_BLAST_RNK_11, 8 * SECOND);

            // Heal
            float const mindBlastCastTime = 1.5f;
            float const mindBlastCoeff = mindBlastCastTime / 3.5f;
            uint32 const bonusSpell = 292 * mindBlastCoeff;
            uint32 const mindBlastMin = ClassSpellsDamage::Priest::MIND_BLAST_RNK_11_MIN + bonusSpell;
            uint32 const mindBlastMax = ClassSpellsDamage::Priest::MIND_BLAST_RNK_11_MAX + bonusSpell;
            TEST_DIRECT_SPELL_DAMAGE(priest, dummy, ClassSpells::Priest::MIND_BLAST_RNK_11, mindBlastMin, mindBlastMax, false);
            TEST_DIRECT_SPELL_DAMAGE(priest, dummy, ClassSpells::Priest::MIND_BLAST_RNK_11, mindBlastMin * 1.5f, mindBlastMax * 1.5f, true);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<MindBlastTestImpt>();
    }
};

class MindControlTest : public TestCaseScript
{
public:
    MindControlTest() : TestCaseScript("spells priest mind_control") { }

    class MindControlTestImpt : public TestCase
    {
    public:
        MindControlTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* enemy = SpawnPlayer(CLASS_WARRIOR, RACE_HUMAN);

            float const baseAttackSpeed = enemy->GetAttackTimer(BASE_ATTACK);
            float const expectedAttackSpeed = baseAttackSpeed * 1.25f;

            // Mana cost
            uint32 const expectedMindControlMana = 750;
            TEST_POWER_COST(priest, enemy, ClassSpells::Priest::MIND_CONTROL_RNK_3, POWER_MANA, expectedMindControlMana);

            // Aura
            TEST_AURA_MAX_DURATION(enemy, ClassSpells::Priest::MIND_CONTROL_RNK_3, 10 * SECOND * IN_MILLISECONDS);

            // Attack Speed +25%
            TEST_ASSERT(enemy->GetAttackTimer(BASE_ATTACK) == expectedAttackSpeed);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<MindControlTestImpt>();
    }
};

class MindVisionTest : public TestCaseScript
{
public:
    MindVisionTest() : TestCaseScript("spells priest mind_vision") { }

    class MindVisionTestImpt : public TestCase
    {
    public:
        MindVisionTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);

            Position spawn(_location);
            spawn.MoveInFront(spawn, 70.0f);
            TestPlayer* warrior70m = SpawnPlayer(CLASS_WARRIOR, RACE_ORC, 70, spawn); // in priest's sight
            spawn.MoveInFront(spawn, 70.0f);
            TestPlayer* rogue140m = SpawnPlayer(CLASS_ROGUE, RACE_HUMAN, 70, spawn); // out of priest's sight

            // Assert visibility
            TEST_ASSERT(priest->HaveAtClient(warrior70m));
            TEST_ASSERT(!priest->HaveAtClient(rogue140m));
            TEST_ASSERT(warrior70m->HaveAtClient(priest));
            TEST_ASSERT(warrior70m->HaveAtClient(rogue140m));

            // Mana cost
            uint32 const expectedMindVisionMana = 150;
            TEST_POWER_COST(priest, warrior70m, ClassSpells::Priest::MIND_VISION_RNK_2, POWER_MANA, expectedMindVisionMana);

            // Aura
            TEST_AURA_MAX_DURATION(priest, ClassSpells::Priest::MIND_VISION_RNK_2, 1 * MINUTE * IN_MILLISECONDS);
            TEST_AURA_MAX_DURATION(warrior70m, ClassSpells::Priest::MIND_VISION_RNK_2, 1 * MINUTE * IN_MILLISECONDS);

            // Check jumping targets even out of sight works
            FORCE_CAST(priest, rogue140m, ClassSpells::Priest::MIND_VISION_RNK_2, SPELL_MISS_NONE);
            TEST_HAS_AURA(rogue140m, ClassSpells::Priest::MIND_VISION_RNK_2);
            TEST_HAS_AURA(priest, ClassSpells::Priest::MIND_VISION_RNK_2);

            // Aura isnt removed by stealth
            TEST_CAST(rogue140m, rogue140m, ClassSpells::Rogue::STEALTH_RNK_4);
            TEST_HAS_AURA(rogue140m, ClassSpells::Priest::MIND_VISION_RNK_2);

            // Break if in another instance
            rogue140m->TeleportTo(37, 128.205002, 135.136002, 236.025055, 0); // Teleport to Azshara Crater
            Wait(1000);
            TEST_HAS_NOT_AURA(rogue140m, ClassSpells::Priest::MIND_VISION_RNK_2);
            TEST_HAS_AURA(priest, ClassSpells::Priest::MIND_VISION_RNK_2);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<MindVisionTestImpt>();
    }
};

class PrayerOfShadowProtectionTest : public TestCaseScript
{
public:
    PrayerOfShadowProtectionTest() : TestCaseScript("spells priest prayer_of_shadow_protection") { }

    class PrayerOfShadowProtectionTestImpt : public TestCase
    {
    public:
        PrayerOfShadowProtectionTestImpt() : TestCase(STATUS_PASSING, true) { }

        void TestPrayerOfShadowProtection(TestPlayer* priest, Unit* warrior, uint32 spellId, uint32 reagentId, uint32 manaCost, uint32 shadowResistanceBonus, uint32 priestStartShadowResistance, uint32 warriorStartShadowResistance)
        {
            uint32 expectedPriestSR = priestStartShadowResistance + shadowResistanceBonus;
            uint32 expectedWarriorSR = warriorStartShadowResistance + shadowResistanceBonus;

            // Mana cost
            priest->AddItem(reagentId, 1); // Reagent
            TEST_POWER_COST(priest, warrior, spellId, POWER_MANA, manaCost);
            TEST_ASSERT(priest->GetItemCount(reagentId, false) == 0);

            // Aura
            TEST_AURA_MAX_DURATION(warrior, spellId, 20 * MINUTE * IN_MILLISECONDS);
            TEST_AURA_MAX_DURATION(priest, spellId, 20 * MINUTE* IN_MILLISECONDS);

            // Shadow Resistance
            TEST_ASSERT(priest->GetResistance(SPELL_SCHOOL_SHADOW) == expectedPriestSR);
            TEST_ASSERT(warrior->GetResistance(SPELL_SCHOOL_SHADOW) == expectedWarriorSR);
        }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* warrior = SpawnPlayer(CLASS_WARRIOR, RACE_ORC);

            GroupPlayer(priest, warrior);

            uint32 const startPriestSR = priest->GetResistance(SPELL_SCHOOL_SHADOW);
            uint32 const startWarriorSR = warrior->GetResistance(SPELL_SCHOOL_SHADOW);

            uint32 const SACRED_CANDLE = 17029;

            TestPrayerOfShadowProtection(priest, warrior, ClassSpells::Priest::PRAYER_OF_SHADOW_PROTECTION_RNK_1, SACRED_CANDLE, 1300, 60, startPriestSR, startWarriorSR);
            TestPrayerOfShadowProtection(priest, warrior, ClassSpells::Priest::PRAYER_OF_SHADOW_PROTECTION_RNK_2, SACRED_CANDLE, 1620, 70, startPriestSR, startWarriorSR);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<PrayerOfShadowProtectionTestImpt>();
    }
};

class PsychicScreamTest : public TestCaseScript
{
public:
    PsychicScreamTest() : TestCaseScript("spells priest psychic_scream") { }

    class PsychicScreamTestImpt : public TestCase
    {
    public:
        PsychicScreamTestImpt() : TestCase(STATUS_PASSING, true) { }

        bool isFeared(Unit* victim)
        {
            return victim->HasAura(ClassSpells::Priest::PSYCHIC_SCREAM_RNK_4);
        }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* enemy1 = SpawnPlayer(CLASS_WARRIOR, RACE_HUMAN);
            TestPlayer* enemy2 = SpawnPlayer(CLASS_WARRIOR, RACE_HUMAN);
            TestPlayer* enemy3 = SpawnPlayer(CLASS_WARRIOR, RACE_HUMAN);
            TestPlayer* enemy4 = SpawnPlayer(CLASS_WARRIOR, RACE_HUMAN);
            Position spawn(_location);
            spawn.MoveInFront(spawn, 10.0f);
            TestPlayer* enemyFurther = SpawnPlayer(CLASS_WARRIOR, RACE_HUMAN, 70, spawn);

            // Mana
            uint32 const expectedPsychicScreamMana = 210;
            TEST_POWER_COST(priest, priest, ClassSpells::Priest::PSYCHIC_SCREAM_RNK_4, POWER_MANA, expectedPsychicScreamMana);

            // Cooldown
            TEST_HAS_COOLDOWN(priest, ClassSpells::Priest::PSYCHIC_SCREAM_RNK_4, 30 * SECOND);

            // Auras & range
            TEST_AURA_MAX_DURATION(enemy1, ClassSpells::Priest::PSYCHIC_SCREAM_RNK_4, 8 * SECOND * IN_MILLISECONDS);
            TEST_AURA_MAX_DURATION(enemy2, ClassSpells::Priest::PSYCHIC_SCREAM_RNK_4, 8 * SECOND * IN_MILLISECONDS);
            TEST_AURA_MAX_DURATION(enemy3, ClassSpells::Priest::PSYCHIC_SCREAM_RNK_4, 8 * SECOND * IN_MILLISECONDS);
            TEST_AURA_MAX_DURATION(enemy4, ClassSpells::Priest::PSYCHIC_SCREAM_RNK_4, 8 * SECOND * IN_MILLISECONDS);
            TEST_HAS_NOT_AURA(enemyFurther, ClassSpells::Priest::PSYCHIC_SCREAM_RNK_4);

            // Reset auras & cd
            priest->GetSpellHistory()->ResetAllCooldowns();
            enemy1->RemoveAurasDueToSpell(ClassSpells::Priest::PSYCHIC_SCREAM_RNK_4);
            enemy2->RemoveAurasDueToSpell(ClassSpells::Priest::PSYCHIC_SCREAM_RNK_4);
            enemy3->RemoveAurasDueToSpell(ClassSpells::Priest::PSYCHIC_SCREAM_RNK_4);
            enemy4->RemoveAurasDueToSpell(ClassSpells::Priest::PSYCHIC_SCREAM_RNK_4);
            enemyFurther->KillSelf(); // not needed anymore

            // Max 5 enemies touched
            TestPlayer* enemy5 = SpawnPlayer(CLASS_WARRIOR, RACE_HUMAN);
            TestPlayer* enemy6 = SpawnPlayer(CLASS_WARRIOR, RACE_HUMAN);
            priest->SetPower(POWER_MANA, expectedPsychicScreamMana);
            priest->ForceSpellHitResult(SPELL_MISS_NONE);
            FORCE_CAST(priest, priest, ClassSpells::Priest::PSYCHIC_SCREAM_RNK_4);
            priest->ResetForceSpellHitResult();

            uint32 count = 0;
            count += uint32(isFeared(enemy1));
            count += uint32(isFeared(enemy2));
            count += uint32(isFeared(enemy3));
            count += uint32(isFeared(enemy4));
            count += uint32(isFeared(enemy5));
            count += uint32(isFeared(enemy6));
            TEST_ASSERT(count == 5);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<PsychicScreamTestImpt>();
    }
};

class ShadowProtectionTest : public TestCaseScript
{
public:
    ShadowProtectionTest() : TestCaseScript("spells priest shadow_protection") { }

    class ShadowProtectionTestImpt : public TestCase
    {
    public:
        ShadowProtectionTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            TestPlayer* warrior = SpawnPlayer(CLASS_WARRIOR, RACE_ORC);

            uint32 shadowResistanceBonus = 70;
            uint32 expectedShadowResistance = warrior->GetResistance(SPELL_SCHOOL_SHADOW) + shadowResistanceBonus;

            // Mana cost
            uint32 const expectedShadowProtectionMana = 810;
            TEST_POWER_COST(priest, warrior, ClassSpells::Priest::SHADOW_PROTECTION_RNK_4, POWER_MANA, expectedShadowProtectionMana);

            // Health
            ASSERT_INFO("Shadow resistance: %u, expected: %u", warrior->GetResistance(SPELL_SCHOOL_SHADOW), expectedShadowResistance);
            TEST_ASSERT(warrior->GetResistance(SPELL_SCHOOL_SHADOW) == expectedShadowResistance);

            // Aura
            TEST_AURA_MAX_DURATION(warrior, ClassSpells::Priest::SHADOW_PROTECTION_RNK_4, 10 * MINUTE * IN_MILLISECONDS);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<ShadowProtectionTestImpt>();
    }
};

class ShadowWordDeathTest : public TestCaseScript
{
public:
    ShadowWordDeathTest() : TestCaseScript("spells priest shadow_word_death") { }

    class ShadowWordDeathTestImpt : public TestCase
    {
    public:
        ShadowWordDeathTestImpt() : TestCase(STATUS_KNOWN_BUG, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_TROLL);
            TestPlayer* warrior = SpawnPlayer(CLASS_WARRIOR, RACE_HUMAN);
            Creature* dummy = SpawnCreature();

            EQUIP_ITEM(priest, 34336); // Sunflare -- 292 SP

            float const shadowWordDeathCastTime = 1.5f;
            float const shadowWordDeathCoeff = shadowWordDeathCastTime / 3.5f;
            uint32 const bonusSpell = 292 * shadowWordDeathCoeff;
            uint32 const shadowWordDeathMin = ClassSpellsDamage::Priest::SHADOW_WORD_DEATH_RNK_2_MIN + bonusSpell;
            uint32 const shadowWordDeathMax = ClassSpellsDamage::Priest::SHADOW_WORD_DEATH_RNK_2_MAX + bonusSpell;

            warrior->DisableRegeneration(true);
            priest->DisableRegeneration(true);
            EnableCriticals(priest, false);

            // Both have same starting health
            uint32 const priestStartHealth = 4 * shadowWordDeathMin;
            priest->SetMaxHealth(priestStartHealth);
            priest->SetHealth(priestStartHealth);

            uint32 const warriorStartHealth = 3 * shadowWordDeathMin;
            warrior->SetMaxHealth(warriorStartHealth);
            warrior->SetHealth(warriorStartHealth);

            // Get damage on SWD & Mana cost
            priest->GetSpellHistory()->ResetAllCooldowns();
            priest->ForceSpellHitResult(SPELL_MISS_NONE);
            uint32 const expectedShadowWordDeathMana = 309;
            TEST_POWER_COST(priest, warrior, ClassSpells::Priest::SHADOW_WORD_DEATH_RNK_2, POWER_MANA, expectedShadowWordDeathMana);
            priest->SetPower(POWER_MANA, 2000);

            // Cooldown
            TEST_HAS_COOLDOWN(priest, ClassSpells::Priest::SHADOW_WORD_DEATH_RNK_2, 12 * SECOND);

            // Damage = backlash
            Wait(1000);
            uint32 damage = warriorStartHealth - warrior->GetHealth();
            uint32 backlash = priestStartHealth - priest->GetHealth();
            TEST_ASSERT(damage == backlash);

            // Backlash absorbed by shield
            priest->GetSpellHistory()->ResetAllCooldowns();
            uint32 priestHealth = priest->GetHealth();
            FORCE_CAST(priest, priest, ClassSpells::Priest::POWER_WORD_SHIELD_RNK_12, SPELL_MISS_NONE, TRIGGERED_CAST_DIRECTLY);
            FORCE_CAST(priest, warrior, ClassSpells::Priest::SHADOW_WORD_DEATH_RNK_2);
            TEST_ASSERT(priest->GetHealth() == priestHealth);
            priest->RemoveAurasDueToSpell(ClassSpells::Priest::POWER_WORD_SHIELD_RNK_12);

            // On spell reflect: priest takes damage and backlash <- BUG HERE
            priest->GetSpellHistory()->ResetAllCooldowns();
            uint32 warriorHealth = warrior->GetHealth();
            FORCE_CAST(warrior, warrior, ClassSpells::Warrior::DEFENSIVE_STANCE_RNK_1);
            EQUIP_ITEM(warrior, 34185); // Shield
            warrior->SetPower(POWER_RAGE, 100 * 10);
            FORCE_CAST(warrior, warrior, ClassSpells::Warrior::SPELL_REFLECTION_RNK_1);
            Wait(1000);
            FORCE_CAST(priest, warrior, ClassSpells::Priest::SHADOW_WORD_DEATH_RNK_2);
            TEST_ASSERT(warrior->GetHealth() == warriorHealth);

            // No backlash on kill
            priest->GetSpellHistory()->ResetAllCooldowns();
            priestHealth = priest->GetHealth();
            FORCE_CAST(priest, warrior, ClassSpells::Priest::SHADOW_WORD_DEATH_RNK_2);
            TEST_ASSERT(warrior->IsDead());
            TEST_ASSERT(priest->GetHealth() == priestHealth);

            // No durability damage on suicide
            priest->GetSpellHistory()->ResetAllCooldowns();
            FORCE_CAST(priest, dummy, ClassSpells::Priest::SHADOW_WORD_DEATH_RNK_2);
            Wait(1000);
            TEST_ASSERT(priest->IsDead());
            Item* sunflare = priest->GetItemByPos(INVTYPE_WEAPONMAINHAND);
            uint32 sunflareDurability = sunflare->GetUInt32Value(ITEM_FIELD_DURABILITY);
            uint32 sunflareMaxDurability = sunflare->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
            TEST_ASSERT(sunflareDurability == sunflareMaxDurability);
            priest->ResetForceSpellHitResult();

            // Damage
            // Enough health to survive
            uint32 health = 10000000;
            priest->SetMaxHealth(health);
            priest->SetHealth(health);
            TEST_DIRECT_SPELL_DAMAGE(priest, dummy, ClassSpells::Priest::SHADOW_WORD_DEATH_RNK_2, shadowWordDeathMin, shadowWordDeathMax, false);
            TEST_DIRECT_SPELL_DAMAGE(priest, dummy, ClassSpells::Priest::SHADOW_WORD_DEATH_RNK_2, shadowWordDeathMin * 1.5f, shadowWordDeathMax * 1.5f, true);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<ShadowWordDeathTestImpt>();
    }
};

class ShadowWordPainTest : public TestCaseScript
{
public:
    ShadowWordPainTest() : TestCaseScript("spells priest shadow_word_pain") { }

    class ShadowWordPainTestImpt : public TestCase
    {
    public:
        ShadowWordPainTestImpt() : TestCase(STATUS_PASSING, true) { }

        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);
            Creature* dummy = SpawnCreature();

            EQUIP_ITEM(priest, 34336); // Sunflare -- 292 SP

            // Mana cost
            uint32 const expectedShadowWordPainMana = 575;
            TEST_POWER_COST(priest, dummy, ClassSpells::Priest::SHADOW_WORD_PAIN_RNK_10, POWER_MANA, expectedShadowWordPainMana);

            // Damage
            float const shadowWordPainCoeff = ClassSpellsCoeff::Priest::SHADOW_WORD_PAIN;
            uint32 const spellBonus = 292 * shadowWordPainCoeff;
            uint32 const shadowWordPainTotal = ClassSpellsDamage::Priest::SHADOW_WORD_PAIN_RNK_10_TOTAL + spellBonus;
            TEST_DOT_DAMAGE(priest, dummy, ClassSpells::Priest::SHADOW_WORD_PAIN_RNK_10, shadowWordPainTotal, true);
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<ShadowWordPainTestImpt>();
    }
};

class ShadowfiendTest : public TestCaseScript
{
public:
    ShadowfiendTest() : TestCaseScript("spells priest shadowfiend") { }

    class ShadowfiendTestImpt : public TestCase
    {
    public:
        ShadowfiendTestImpt() : TestCase(STATUS_PASSING, true) { }

        /*
        Data:
        - http://wowwiki.wikia.com/wiki/Shadowfiend?oldid=1581560
        - https://github.com/FeenixServerProject/Archangel_2.4.3_Bugtracker/issues/2590
        - Corecraft: 65% shadow spell coeff / Adding 90% melee and ranged attacker's miss chance auras
        */
        void Test() override
        {
            TestPlayer* priest = SpawnPlayer(CLASS_PRIEST, RACE_BLOODELF);

            Position spawn(_location);
            spawn.MoveInFront(spawn, 20.0f);
            TestPlayer* warrior = SpawnPlayer(CLASS_WARRIOR, RACE_HUMAN, 70, spawn);
            warrior->SetMaxHealth(std::numeric_limits<int>::max());
            warrior->SetHealth(warrior->GetMaxHealth());

            EQUIP_ITEM(priest, 34336); // Sunflare -- 292 SP

            // Mana cost
            uint32 const expectedShadowFiendMana = 157;
            TEST_POWER_COST(priest, warrior, ClassSpells::Priest::SHADOWFIEND_RNK_1, POWER_MANA, expectedShadowFiendMana);
            priest->SetMaxPower(POWER_MANA, std::numeric_limits<int>::max());

            // Cooldown
            TEST_HAS_COOLDOWN(priest, ClassSpells::Priest::SHADOWFIEND_RNK_1, 5 * MINUTE);

            Guardian* shadowfiend = priest->GetGuardianPet();
            TEST_ASSERT(shadowfiend != nullptr);
            shadowfiend->ForceMeleeHitResult(MELEE_HIT_MISS);

            // Summon in melee range of target
            TEST_ASSERT(warrior->GetDistance(shadowfiend) < 5);

            // Attacking target
            Wait(10);
            TEST_ASSERT(shadowfiend->GetVictim() == warrior);

            // Damage
            uint32 const spellBonus = 292 * 0.65f;
            uint32 expectedShadowfiendMin = 100 + spellBonus;
            uint32 expectedShadowfiendMax = 121 + spellBonus;
            uint32 sampleSize;
            uint32 absoluteAllowedError;
            _GetApproximationParams(sampleSize, absoluteAllowedError, expectedShadowfiendMin, expectedShadowfiendMax);

            auto AI = priest->GetTestingPlayerbotAI();
            shadowfiend->ForceMeleeHitResult(MELEE_HIT_NORMAL);
            for (uint32 i = 0; i < sampleSize; i++) {
                shadowfiend->AttackerStateUpdate(warrior, BASE_ATTACK);
                TC_LOG_DEBUG("test.unit_test", "hit: %u", i);
            }
            shadowfiend->ForceMeleeHitResult(MELEE_HIT_MISS);

            auto damageToTarget = AI->GetWhiteDamageDoneInfo(warrior);
            if (!damageToTarget || damageToTarget->empty())
                TEST_ASSERT(false);

            uint32 totalMana = 0;
            uint32 total = 0;
            uint32 count = 0;
            for (auto itr : *damageToTarget)
            {
                if (itr.damageInfo.HitOutCome != MELEE_HIT_NORMAL)
                    TEST_ASSERT(false);

                TEST_ASSERT(itr.damageInfo.Damages[0].DamageSchoolMask == SPELL_SCHOOL_MASK_SHADOW);
                uint32 damage = itr.damageInfo.Damages[0].Damage;
                uint32 mana = itr.damageInfo.Damages[0].Damage * 2.5f;
                TC_LOG_DEBUG("test.unit_test", "damage: %u, mana gained: %u", damage, mana);
                total += damage;
                totalMana += damage * 2.5f;
                count++;
            }

            ASSERT_INFO("count: %u, sample: %u", count, sampleSize);
            TEST_ASSERT(count == sampleSize);

            TC_LOG_DEBUG("test.unit_test", "total: %u, mana: %u", total, totalMana);

            // Mana regen: 250% of damage dealt
            uint32 expectedMana = total * 2.5f;
            ASSERT_INFO("Mana: %u, expected: %u, sampleSize: %u", priest->GetPower(POWER_MANA), expectedMana, sampleSize);
            TEST_ASSERT(priest->GetPower(POWER_MANA) == expectedMana);

            // Damage take 65% of owner's shadow spell power

            // Damage

            // Has Shadow Armor
            TEST_HAS_AURA(shadowfiend, 34424);

            // Resistance: 150 but 250 for shadow
            TEST_ASSERT(shadowfiend->GetResistance(SPELL_SCHOOL_ARCANE) >= 150);
            TEST_ASSERT(shadowfiend->GetResistance(SPELL_SCHOOL_FIRE) >= 150);
            TEST_ASSERT(shadowfiend->GetResistance(SPELL_SCHOOL_FROST) >= 150);
            TEST_ASSERT(shadowfiend->GetResistance(SPELL_SCHOOL_NATURE) >= 150);
            TEST_ASSERT(shadowfiend->GetResistance(SPELL_SCHOOL_SHADOW) >= 250);

            // Attack speed 1.5
            TEST_ASSERT(shadowfiend->GetAttackTime(BASE_ATTACK) == 1500);
            shadowfiend->ResetForceMeleeHitResult();
        }
    };

    std::shared_ptr<TestCase> GetTest() const override
    {
        return std::make_shared<ShadowfiendTestImpt>();
    }
};

void AddSC_test_spells_priest()
{
    // Discipline: 10/10
    new DispelMagicTest();
    new FearWardTest();
    new InnerFireTest();
    new LevitateTest();
    new ManaBurnTest();
    new MassDispelTest();
    new PowerWordFortitudeTest();
    new PowerWordShieldTest();
    new PrayerOfFortitudeTest();
    new ShackleUndeadTest();
    // Holy: 13/13
    new AbolishDiseaseTest();
    new BindingHealTest();
    new CureDiseaseTest();
    new FlashHealTest();
    new GreaterHealTest();
    new HealTest();
    new HolyFireTest();
    new LesserHealTest();
    new PrayerOfHealingTest();
    new PrayerOfMendingTest();
    new RenewTest();
    new ResurrectionTest();
    new SmiteTest();
    // Shadow: /
    new FadeTest();
    new MindBlastTest();
    new MindControlTest();
    new MindVisionTest();
    new PrayerOfShadowProtectionTest();
    new PsychicScreamTest();
    new ShadowProtectionTest();
    new ShadowWordDeathTest();
    new ShadowWordPainTest();
    new ShadowfiendTest();
}