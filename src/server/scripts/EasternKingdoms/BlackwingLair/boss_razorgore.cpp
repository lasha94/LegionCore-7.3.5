/*
 * Copyright (C) 2008-2013 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2006-2009 ScriptDev2 <https://scriptdev2.svn.sourceforge.net/>
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

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "blackwing_lair.h"
#include "Player.h"

enum Say
{
    SAY_EGGS_BROKEN1        = 0,
    SAY_EGGS_BROKEN2        = 1,
    SAY_EGGS_BROKEN3        = 2,
    SAY_DEATH               = 3,
};

enum Spells
{
    SPELL_MINDCONTROL       = 19832,
    SPELL_CHANNEL           = 45537,
    SPELL_EGG_DESTROY       = 19873,

    SPELL_CLEAVE            = 22540,
    SPELL_WARSTOMP          = 24375,
    SPELL_FIREBALLVOLLEY    = 22425,
    SPELL_CONFLAGRATION     = 23023
};

enum Summons
{
    NPC_ELITE_DRACHKIN      = 12422,
    NPC_ELITE_WARRIOR       = 12458,
    NPC_WARRIOR             = 12416,
    NPC_MAGE                = 12420,
    NPC_WARLOCK             = 12459,

    GO_EGG                  = 177807
};

enum EVENTS
{
    EVENT_CLEAVE            = 1,
    EVENT_STOMP             = 2,
    EVENT_FIREBALL          = 3,
    EVENT_CONFLAGRATION     = 4
};

class boss_razorgore : public CreatureScript
{
public:
    boss_razorgore() : CreatureScript("boss_razorgore") { }

    struct boss_razorgoreAI : public BossAI
    {
        boss_razorgoreAI(Creature* creature) : BossAI(creature, BOSS_RAZORGORE) { }

        void Reset()
        {
            _Reset();

            ownerGUID.Clear();

            secondPhase = false;
            instance->SetData(DATA_EGG_EVENT, NOT_STARTED);
        }

        void EnterCombat(Unit* who)
        {
            DoZoneInCombat();
            std::list<GameObject*> eggs;
            me->GetGameObjectListWithEntryInGrid(eggs, GO_EGG, 100);
            for (std::list<GameObject*>::iterator itr = eggs.begin(); itr != eggs.end(); ++itr)
            {
                (*itr)->SetGoState(GO_STATE_READY);
                (*itr)->SetPhaseMask(1, true);
                (*itr)->Respawn();
            }
        }

        void JustDied(Unit* /*killer*/)
        {
            _JustDied();
            Talk(SAY_DEATH);

            instance->SetData(DATA_EGG_EVENT, NOT_STARTED);
        }

        void DoChangePhase()
        {
            events.RescheduleEvent(EVENT_CLEAVE, 15000);
            events.RescheduleEvent(EVENT_STOMP, 35000);
            events.RescheduleEvent(EVENT_FIREBALL, 7000);
            events.RescheduleEvent(EVENT_CONFLAGRATION, 12000);

            DoZoneInCombat();

            secondPhase = true;
            me->RemoveAllAuras();
            me->SetHealth(me->GetMaxHealth());
        }

        void DoAction(int32 const action)
        {
            if (action == ACTION_PHASE_TWO)
                DoChangePhase();
        }

        void DamageTaken(Unit* /*who*/, uint32& damage, DamageEffectType dmgType)
        {
            //if (!secondPhase)
                //damage = 0;
        }

        void UpdateAI(uint32 diff)
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            if (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case EVENT_CLEAVE:
                        DoCastVictim(SPELL_CLEAVE);
                        events.RescheduleEvent(EVENT_CLEAVE, urand(7000, 10000));
                        break;
                    case EVENT_STOMP:
                        DoCastVictim(SPELL_WARSTOMP);
                        events.RescheduleEvent(EVENT_STOMP, urand(15000, 25000));
                        break;
                    case EVENT_FIREBALL:
                        DoCastVictim(SPELL_FIREBALLVOLLEY);
                        events.RescheduleEvent(EVENT_FIREBALL, urand(12000, 15000));
                        break;
                    case EVENT_CONFLAGRATION:
                        DoCastVictim(SPELL_CONFLAGRATION);
                        if (me->getVictim() && me->getVictim()->HasAura(SPELL_CONFLAGRATION))
                            if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 1, 100, true))
                                me->TauntApply(target);
                        events.RescheduleEvent(EVENT_CONFLAGRATION, 30000);
                        break;
                }
            }
            DoMeleeAttackIfReady();
        }

    private:
        bool secondPhase;
        ObjectGuid ownerGUID;
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return GetInstanceAI<boss_razorgoreAI>(creature);
    }
};

class go_orb_of_domination : public GameObjectScript
{
public:
    go_orb_of_domination() : GameObjectScript("go_orb_of_domination") { }

    bool OnGossipHello(Player* player, GameObject* go)
    {
        if (InstanceScript* instance = go->GetInstanceScript())
            if (instance->GetData(DATA_EGG_EVENT) != DONE)
                if (Creature* razor = Unit::GetCreature(*go, instance->GetGuidData(DATA_RAZORGORE_THE_UNTAMED)))
                {
                    razor->SetInCombatWithZone();
                    razor->Attack(player, true);
                    player->CastSpell(razor, SPELL_MINDCONTROL);
                    player->SetInCombatWith(razor);
                    razor->SetInCombatWithZone();
                    razor->GetMotionMaster()->Clear();
                    Position pos = razor->GetPosition();
                    razor->GetMotionMaster()->MovePoint(1, pos);
                }
        return true;
    }
};

class spell_egg_event : public SpellScriptLoader
{
    public:
        spell_egg_event() : SpellScriptLoader("spell_egg_event") { }

        class spell_egg_eventSpellScript : public SpellScript
        {
            PrepareSpellScript(spell_egg_eventSpellScript);

            void HandleOnHit()
            {
                if (InstanceScript* instance = GetCaster()->GetInstanceScript())
                    instance->SetData(DATA_EGG_EVENT, SPECIAL);
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_egg_eventSpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_egg_eventSpellScript();
        }
};

void AddSC_boss_razorgore()
{
    new boss_razorgore();
    new go_orb_of_domination();
    new spell_egg_event();
}
