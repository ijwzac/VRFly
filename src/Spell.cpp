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
    auto spellEmitForce = GetEmitForceSpell();
    if (!spellLift || !spellXY || !spellXYZ || !spellEmit || !spellEmitFire || !spellEmitForce) {
        log::error("Didn't find one of the spells! Return from FlyMain");
        return;
    }

    std::vector<RE::SpellItem*> spellList;
    spellList.push_back(spellLift);
    spellList.push_back(spellXY);
    spellList.push_back(spellXYZ);
    spellList.push_back(spellEmit);
    spellList.push_back(spellEmitFire);
    spellList.push_back(spellEmitForce);

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
                isCastingHandMatch = g->value > 0.9f;

                log::trace("Is trigger pressed:{}", isCastingHandMatch);

                if (isCastingHandMatch) {
                    // Now check if there exists an active effect matching this spell and its slot
                    auto slot = isLeft ? Slot::kLeft : Slot::kRight;

                    // Check for Velo spells
                    if (spell == spellLift || spell == spellXY || spell == spellXYZ || spell == spellEmit ||
                        spell == spellEmitFire) {
                        auto eff = allEffects.FindVeloEffect(slot, spell, nullptr);

                        if (!eff) {
                            // Case 1: this is the first time player casts this spell. Create a new active effect
                            VeloEffect* effect =
                                new VeloEffect(slot, new TrapezoidVelo(0.0f, 0.0f, 0.0f), spell, nullptr);

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
                            Velo newStable;
                            if (spell == spellLift || spell == spellXY || spell == spellXYZ) {
                                conf_LiftModTime = eff->xyzSpell.modTime;
                                newStable = eff->xyzSpell.CalculateNewStable(GetPlayerHandPos(isLeft, player));
                            } else if (spell == spellEmit || spell == spellEmitFire) {
                                conf_LiftModTime = eff->emitSpell.modTime;
                                newStable = eff->emitSpell.CalculateNewStable();
                            }
                            log::trace("About to Mod velo to: {}, {}, {}", newStable.x, newStable.y, newStable.z);
                            eff->velo->ModStable(newStable.x, newStable.y, newStable.z, conf_LiftModTime);

                            eff->Update();
                        }
                    }

                    // Check for Force spells
                    if (spell == spellEmitForce) {
                        auto eff = allEffects.FindForceEffect(slot, spell);

                        if (!eff) {
                            // Case 1: this is the first time player casts this spell. Create a new active effect
                            ForceEffect* effect = new ForceEffect(slot, Force(), spell);

                            if (spell == spellEmitForce) {
                                effect->emitForceSpell = EmitForceSpell(true, isLeft);
                            }
                            allEffects.Push(effect);
                        } else {
                            // Case 2: player is continuing casting the spell. Update the effect
                            Force newForce;
                            if (spell == spellEmitForce) {
                                newForce = eff->emitForceSpell.CalculateNewForce();
                                eff->force = newForce;
                            }
                            log::trace("Mod force to: {}, {}, {}", newForce.x, newForce.y, newForce.z);

                            eff->Update();
                        }
                    }
                }
            }
        }

        
    }

    // Case 3: player stopped casting the spell. Delete the velo and delete the effect
    allEffects.DeleteIdleEffects();
}

// CalculateDragSimple assumes that Drag is always in the opposite direction of velocity,
Force CalculateDragSimple(RE::NiPoint3& currentVelo) {
    float conf_dragcoefficient = 0.03f;
    auto v = currentVelo * currentVelo.Length() * conf_dragcoefficient * -1.0f;
    return Force(v.x, v.y, v.z);
}

// WingsCheckMain updates player's wings status
void WingsCheckMain() {
    auto& playerSt = PlayerState::GetSingleton();
    auto player = playerSt.player;
    auto spellWingsFlag = GetWingsFlagSpell();
    if (!spellWingsFlag) {
        log::error("Didn't find spellWingsFlag! Return from WingsCheckMain");
        return;
    }

    // If player doesn't have this spell, meaning they are not flying
    if (!player->HasSpell(spellWingsFlag)) {
        log::trace("Player doesn't have the spellWingsFlag");
        return;
    }

    auto slot = Slot::kHead;
    auto eff = allEffects.FindForceEffect(slot, spellWingsFlag);

    if (!eff) {
        // It's the first frame for player to cast this shout
        eff = new ForceEffect(slot, Force(), spellWingsFlag);

        eff->wingSpell = WingSpell(true);

        allEffects.Push(eff);
    }

    // No matter what, update the effect to prevent it from being deleted
    eff->Update();
      

    // If player isn't holding both grips, don't do anything
    // TODO: switch back to grips
    auto gripL = GetTriggerL();
    auto gripR = GetTriggerR();
    if (!gripL || !gripR) {
        log::error("Didn't find gripL or gripR! Return from WingsCheckMain");
        return;
    }
    if (gripL->value < 0.1f || gripR->value < 0.1f) {
        log::trace("Player not holding both grips");
        return;
    }

    
    auto speedL = playerSt.speedBuf.GetVelocity(5, true);
    auto speedR = playerSt.speedBuf.GetVelocity(5, false);
    float conf_slapThres = 0.0f;
    if (speedL.Length() > conf_slapThres || speedR.Length() > conf_slapThres) {
        // Case 1: player is moving controllers fast. Set force based on controller speed
        Force newForce;
        newForce = eff->wingSpell.CalculateSlapForce(speedL, speedR);
        eff->force = newForce;
        log::trace("Mod force from Wings to: {}, {}, {}", newForce.x, newForce.y, newForce.z);
    }
    
}