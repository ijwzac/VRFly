#include "Spell.h"


using namespace SKSE;

void SpellCheckMain() {
    auto& playerSt = PlayerState::GetSingleton();
    auto player = playerSt.player;
    auto spellLift = GetLiftSpell();
    auto spellXY = GetXYSpell();
    auto spellXYZ = GetXYZSpell();
    auto spellEmit = GetEmitSpell();
    auto spellEmitFire = GetEmitFireSpell();
    if (!spellLift || !spellXY || !spellXYZ || !spellEmit) {
        log::error("Didn't find one of the spells! Return from FlyMain");
    }

    std::vector<RE::SpellItem*> spellList;
    spellList.push_back(spellLift);
    spellList.push_back(spellXY);
    spellList.push_back(spellXYZ);
    spellList.push_back(spellEmit);
    spellList.push_back(spellEmitFire);

    for (bool isLeft : {false, true}) {
        RE::MagicItem* equip = isLeft ? playerSt.leftSpell : playerSt.rightSpell;
        if (!equip) continue;
        for (RE::SpellItem* spell : spellList) {
            if (player->IsCasting(spell) && spell == equip) {
                log::trace("Player is casting spell: {:x}. Now checking trigger. IsLeft:{}", spell->GetFormID(), isLeft);

                bool isCastingHandMatch = false;
                RE::TESGlobal* g = isLeft ? GetTriggerL() : GetTriggerR();
                if (!g) {
                    log::error("Didn't find global");
                    continue;
                }
                short globalV = g->value;
                isCastingHandMatch = globalV == 1;

                log::trace("Is trigger pressed:{}", isCastingHandMatch);

                if (isCastingHandMatch) {
                    // Now check if there exists an active effect matching this spell and its slot
                    auto slot = isLeft ? ActiveFlyEffect::Slot::kLeft : ActiveFlyEffect::Slot::kRight;
                    auto eff = allEffects.FindEffect(slot, spell, nullptr);

                    if (!eff) {
                        // Case 1: this is the first time player casts this spell. Create a new active effect
                        ActiveFlyEffect* effect =
                            new ActiveFlyEffect(slot, new TrapezoidForce(0.0f, 0.0f, 0.0f), spell, nullptr);

                        if (spell == spellLift) {
                            effect->xyzSpell = XYZMoveSpell(GetPlayerHandPos(isLeft, player), true, false);
                        } else if (spell == spellXY) {
                            effect->xyzSpell = XYZMoveSpell(GetPlayerHandPos(isLeft, player), false, true);
                        } else if (spell == spellXYZ) {
                            effect->xyzSpell = XYZMoveSpell(GetPlayerHandPos(isLeft, player), true, true);
                        } else if (spell == spellEmit || spell == spellEmitFire) {
                            effect->emitSpell = EmitSpell(true, isLeft);
                        }
                        allEffects.Push(effect);
                    } else {
                        // Case 2: player is continuing casting the spell. Update the effect
                        float conf_LiftModTime = 0.3f;
                        Force newStable;
                        if (spell == spellLift || spell == spellXY || spell == spellXYZ) {
                            newStable = eff->xyzSpell.CalculateNewStable(GetPlayerHandPos(isLeft, player));
                        } else if (spell == spellEmit || spell == spellEmitFire) {
                            conf_LiftModTime = 1.3f;
                            newStable = eff->emitSpell.CalculateNewStable();
                        }
                        log::trace("About to Mod force to: {}, {}, {}", newStable.x, newStable.y, newStable.z);
                        eff->force->ModStable(newStable.x, newStable.y, newStable.z, conf_LiftModTime);

                        eff->Update();
                    }
                }
            }
        }

        
    }

    // TODO: consider case 3: player stopped casting the spell. Make the force decrease, delete effect, and later delete
    // force
    allEffects.DeleteIdleEffects();
}