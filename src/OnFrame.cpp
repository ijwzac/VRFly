#include <RE/Skyrim.h>
#include "OnFrame.h"
#include <chrono>

using namespace SKSE;
using namespace SKSE::log;

using namespace ZacOnFrame;

// The hook below is taught by ThirdEyeSqueegee and Flip on Discord. Related code:
// https://github.com/ThirdEyeSqueegee/DynamicGamma/blob/main/include/GammaController.h#L16 and
// https://github.com/doodlum/MaxsuWeaponSwingParry-ng/blob/main/src/WeaponParry_Hooks.h
// TODO: Some numbers are magic numbers from their code. Figure them out
void ZacOnFrame::InstallFrameHook() {
    SKSE::AllocTrampoline(1 << 4);
    auto& trampoline = SKSE::GetTrampoline();

    REL::Relocation<std::uintptr_t> OnFrameBase{REL::RelocationID(35565, 36564)}; // 35565 is 1405bab10
    _OnFrame =
        trampoline.write_call<5>(OnFrameBase.address() + REL::Relocate(0x748, 0xc26, 0x7ee), ZacOnFrame::OnFrameUpdate);

    REL::Relocation<std::uintptr_t> ProxyVTable{RE::VTABLE_bhkCharProxyController[1]};
    // Now we need to overwrite function 0x07 in RE/B/bhkCharProxyController.h, which is:
    // "void SetLinearVelocityImpl(const hkVector4& a_velocity) override;  // 07"
    // Thanks to the author of PLANCK, this is discovered to be where proxy controller that controls the movement of player and dragons
    // sets the velocity
    _SetVelocity = ProxyVTable.write_vfunc(0x07, ZacOnFrame::HookSetVelocity);
}

void ZacOnFrame::HookSetVelocity(RE::bhkCharProxyController* controller, RE::hkVector4& a_velocity) {
    auto& playerSt = PlayerState::GetSingleton();
    if (!playerSt.player || !playerSt.player->Is3DLoaded()) {
        _SetVelocity(controller, a_velocity);
        return;
    }

    // Check if this is dragon or player
    //RE::hkVector4 pos1;
    //controller->GetPositionImpl(pos1, false);
    //log::trace("D");
    //log::trace("D");
    //RE::NiPoint3 pos2 = Quad2Velo(pos1);
    //log::trace("D");
    //log::trace("D");
    //RE::NiPoint3 diffPos = playerSt.player->GetPosition() - pos2;
    //log::trace("D");
    //log::trace("D");
    //log::trace("diffPos:{}, {}, {}", diffPos.x, diffPos.y, diffPos.z);
    //if (diffPos.Length() > 100.0f) {
    //    log::trace("Not player");
    //    _SetVelocity(controller, a_velocity);
    //    return;
    //}
    log::trace("jump height:{}", controller->jumpHeight);
    if (controller->jumpHeight != 0) {
        log::trace("Not player or player is jumping");
        _SetVelocity(controller, a_velocity);
        return;
    }

    if (playerSt.player->IsInWater()) {
        _SetVelocity(controller, a_velocity);
        return;
    }

    if (auto charController = playerSt.player->GetCharController(); charController) {
        if (charController->flags.any(RE::CHARACTER_FLAGS::kJumping)) {
            log::trace("Jumping");
            playerSt.lastJumpFrame = iFrameCount;
            _SetVelocity(controller, a_velocity);
            return;
        }
    }

    int64_t conf_jumpExpireDur = 180;
    if (iFrameCount - playerSt.lastJumpFrame < conf_jumpExpireDur && iFrameCount - playerSt.lastJumpFrame > 0) {
        log::trace("Recently jumped");
        _SetVelocity(controller, a_velocity);
        return;
    }

    bool isInMidAir = playerSt.player->IsInMidair();
    bool isFirstUpdateMidAir = false;
    if (playerSt.isInMidAir == false && isInMidAir) isFirstUpdateMidAir = true;
    if (playerSt.isInMidAir == true && isInMidAir == false && iFrameCount - playerSt.lastOngroundFrame > 40) {
        playerSt.shouldCheckKnock = true;
    }
    playerSt.isInMidAir = isInMidAir;
    if (!isInMidAir) playerSt.lastOngroundFrame = iFrameCount;

    float timeSlowRatio = CurrentSpellWheelSlowRatio(playerSt.player);
    auto ourVelo = playerSt.velocity;
    if (timeSlowRatio < 0.9f) {
        ourVelo = ourVelo * timeSlowRatio;
        //log::trace("Detected slow time when setting velo. Ratio:{}", timeSlowRatio);
    }

    if (playerSt.setVelocity) {
        // If the only effect is wings and player is not slapping wings and player on ground, combine the velocity
        // TODO: problem with jump
        if (playerSt.isEffectOnlyWings && !playerSt.isSlappingWings && !isInMidAir) {
            log::trace("Combine velocity");
            auto combined = a_velocity + ourVelo;
            _SetVelocity(controller, combined);
            return;
        } else if (playerSt.isEffectOnlyWings && !playerSt.isSlappingWings && isInMidAir) {
            if (isFirstUpdateMidAir) {
                // This is the first update player steps into midair (from edge of cliff), we put the current velocity
                // into accumulated velocity
                allEffects.accumVelocity = Quad2Velo(a_velocity);
                log::trace("Origin set velocity, but also modify our accumVelocity to this");
                _SetVelocity(controller, a_velocity);
                return;
            }
        }
        // Otherwise, overwrite
        log::trace("Overwrite velocity");
        _SetVelocity(controller, ourVelo);
        return;
    }

    log::trace("Origin set velocity velocity");
    _SetVelocity(controller, a_velocity);
    return;
    
    // A strange thing: reading controller->GetCharacterProxy()->velocity crashes the game
}

bool isOurFnRunning = false;
long long highest_run_time = 0;
long long total_run_time = 0;
long long run_time_count = 1;

int64_t count_after_pause;

void ZacOnFrame::OnFrameUpdate() {
    if (isOurFnRunning) {
        log::warn("Our functions are running in parallel!!!"); // Not seen even when stress testing. Keep observing
    }
    isOurFnRunning = true;
    // Get the current time point
    auto now = std::chrono::high_resolution_clock::now();
    bool isPaused = true;
    if (bEnableWholeMod) {
        if (const auto ui{RE::UI::GetSingleton()}) {
            // Check if it has been a while since the last_time, and if so, don't do anything for a few seconds
            // This can disable our mod after loading screen, to avoid bugs
            auto dur_last = std::chrono::duration_cast<std::chrono::microseconds>(now - last_time);
            last_time = now;
            if (dur_last.count() > 1000 * 1000) { // 1 second
                count_after_pause = 180;
                CleanBeforeLoad();
                log::info("Detected long period since last frame. Player is probably loading. Clean and pause our functions for 3 seconds");
            }
            if (count_after_pause > 0) count_after_pause--;

            if (!ui->GameIsPaused() && count_after_pause <= 0) {  // Not in menu, not in console, but can't detect in load

                FlyMain();

                iFrameCount++;
                isPaused = false;
            }
        }
    }
    isOurFnRunning = false;
    if (!isPaused) {
        // Convert time point to since epoch (duration)
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - now);

        // Display the microseconds since epoch
        if (highest_run_time < duration.count()) highest_run_time = duration.count();
        total_run_time += duration.count();
        auto average_run_time = total_run_time / run_time_count++;
        if (run_time_count % 300 == 0) {
            log::info("Exe time of our fn:{} us. Highest:{}. Average:{}. Total:{}. Count:{}", duration.count(), highest_run_time,
                      average_run_time, total_run_time, run_time_count);
        }

        // Tested on 0.9.0 version. Normally takes around  when in combat. 0us when not. Highest: 
        //                  Turning trace on slows down average speed by around 
        //                  Debug version is like  slower, but highest doesn't change much
    }

    ZacOnFrame::_OnFrame();  
}

void ZacOnFrame::FlyMain() {
    auto& playerSt = PlayerState::GetSingleton();
    auto player = playerSt.player;
    if (!player || !player->Is3DLoaded()) {
        log::warn("Player null or Player not 3D loaded. Shouldn't happen");
        return;
    }

    // Update player's equipments
    playerSt.UpdateEquip();
    // Update player's hand position to speedBuf
    playerSt.UpdateSpeedBuf();
    // Update recent velocity
    playerSt.UpdateRecentVel();

    
    // PART I ======= Spell checking
    SpellCheckMain();

    // PART II ====== Dragon wings checking
    WingsCheckMain();


    // PART III ======= Change player velocity
    Velo sumAll = allEffects.SumCurrentVelo();
    if (!allEffects.IsForceEffectEmpty() || !allEffects.IsVeloEffectEmpty()) {
        playerSt.SetVelocity(sumAll.x, sumAll.y, sumAll.z);
        playerSt.CancelFall();

        playerSt.setVelocity = true;
        playerSt.isEffectOnlyWings = allEffects.IsEffectOnlyWings();
    } else {
        playerSt.isEffectOnlyWings = false;
        playerSt.setVelocity = false;
    }

    
    // PART IV ====== Effects due to velocity
    VeloEffectMain(sumAll);
    WindManager::GetSingleton().Update();


}

void ZacOnFrame::TimeSlowEffect(RE::Actor* playerActor, int64_t slowFrame) { 
    if (slowFrame <= 0 || fTimeSlowRatio >= 1.0f || fTimeSlowRatio <= 0.0f) {
        log::trace("Due to settings, no time slow effect");
        return;
    }

    // TODO: have a different frame counter for projectile
    if (iFrameCount - slowTimeData.frameLastSlowTime < 50 && iFrameCount - slowTimeData.frameLastSlowTime > 0) {
        log::trace("The last slow time effect is within 50 frames. Don't slow time now");
        return;
    }

    if (slowTimeData.timeSlowSpell == nullptr) {
        RE::SpellItem* timeSlowSpell = GetTimeSlowSpell_SpeelWheel();
        if (!timeSlowSpell) {
            timeSlowSpell = GetTimeSlowSpell_Mine();
            if (!timeSlowSpell) {
                log::trace("TimeSlowEffect: failed to get timeslow spell");
                return;
            }
        }
        slowTimeData.timeSlowSpell = timeSlowSpell;
    }

    if (playerActor->HasSpell(slowTimeData.timeSlowSpell)) {
        log::trace("TimeSlowEffect: Spell wheel or us are already slowing time, don't do anything");
        return;
    }
    
    if (slowTimeData.oldMagnitude.size() > 0) {
        log::error("TimeSlowEffect: slowTimeData.oldMagnitude.size() is not zero:{}", slowTimeData.oldMagnitude.size());
        return;
    }

    // Record spell magnitude
    if (slowTimeData.timeSlowSpell->effects.size() == 0) {
        log::error("TimeSlowEffect: timeslow spell has 0 effect");
        return;
    }

    slowTimeData.frameShouldRemove = iFrameCount + slowFrame;
    slowTimeData.frameLastSlowTime = iFrameCount;

    for (RE::BSTArrayBase::size_type i = 0; i < slowTimeData.timeSlowSpell->effects.size(); i++) {
        auto effect = slowTimeData.timeSlowSpell->effects.operator[](i);
        if (!effect) {
            log::trace("TimeSlowEffect: effect[{}] is null", i);
            continue;
        }
        float oldmag = effect->effectItem.magnitude;
        slowTimeData.oldMagnitude.push_back(oldmag); // store the existing magnitude, which are set by spell wheel
        effect->effectItem.magnitude = fTimeSlowRatio; // set our magnitude
    }

    // Apply spell to player. Now time slow will take effect
    playerActor->AddSpell(slowTimeData.timeSlowSpell);
    log::trace("time slow takes effect", fTimeSlowRatio);
}

void ZacOnFrame::StopTimeSlowEffect(RE::Actor* playerActor) {
    if (slowTimeData.timeSlowSpell == nullptr) {
        log::error("StopTimeSlowEffect: timeslow spell is null. Doesn't make sense");
        return;
    }

    if (!playerActor->HasSpell(slowTimeData.timeSlowSpell)) {
        log::warn("StopTimeSlowEffect: player already doesn't have timeslow spell. Probably removed by spellwheel");
        slowTimeData.clear();
        return;
    }

    // Remove spell from player. Now time slow should stop
    playerActor->RemoveSpell(slowTimeData.timeSlowSpell);
    log::trace("time slow stops", fTimeSlowRatio);

    // Restore the effects to the magnitude that spellwheel set
    if (slowTimeData.timeSlowSpell->effects.size() == 0) {
        log::error("StopTimeSlowEffect: timeslow spell has 0 effect");
        return;
    }
    for (RE::BSTArrayBase::size_type i = 0; i < slowTimeData.timeSlowSpell->effects.size(); i++) {
        auto effect = slowTimeData.timeSlowSpell->effects.operator[](i);
        if (!effect) {
            log::trace("StopTimeSlowEffect: effect[{}] is null", i);
            continue;
        }
        if (i < slowTimeData.oldMagnitude.size() && i >= 0) {
            effect->effectItem.magnitude = slowTimeData.oldMagnitude[i];
        } else {
            log::error("StopTimeSlowEffect: out of boundary. Index: {}. size:{}", i, slowTimeData.oldMagnitude.size());
        }
    }

    slowTimeData.clear();
}




// This function modifies the 4 NiPoint3 passed in. If failed to get the 3D of actor, it return false.
// Otherwise, it returns true, even if some NiPoint3 are not modified. 
// Caller needs to detect if they are modified, by calling IsNiPointZero()
bool ZacOnFrame::FrameGetWeaponPos(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                       RE::NiPoint3& posWeaponTopL,
                       RE::NiPoint3& posWeaponTopR, bool isPlayer) {
    bool bDisplayEnemySegmentCheck = posWeaponBottomL.x == 1234.0f; // a magic number to prevent showing effect twice

    const auto actorRoot = netimmerse_cast<RE::BSFadeNode*>(actor->Get3D());
    if (!actorRoot) {
        log::warn("Fail to find actor:{}", actor->GetBaseObject()->GetName());
        return false;
    }

    const auto nodeNameL =
        "SHIELD"sv;  // This is the node name of left hand weapon, no matter if it's shield, sword, unarmed
    const auto nodeNameR = "WEAPON"sv;

    const auto weaponL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeNameL));
    const auto weaponR = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeNameR));


    // check whether the actor reall y equips a weapon on that hand
    // one-hand weapon: correctly on each hand
    // fist: equipL/R is null
    // bow: both hand are same
    // two-hand weapon: both hands are the same
    // shield: on left hand
    bool isEquipL(false), isEquipR(false), isBow(false), isTwoHandedAxe(false), isWarHammer(false),
        isGreatSword(false), hasShield(false), isSwordL(false), isSwordR(false), isAxeL(false), isAxeR(false),
        isMaceL(false), isMaceR(false), isStaffL(false), isStaffR(false), isFistL(false), isFistR(false),
        isDaggerL(false), isDaggerR(false), hasGauntlet(false);

    if (auto equipR = actor->GetEquippedObject(false); equipR) {
        isEquipR = true;
        if (auto equipWeapR = equipR->As<RE::TESObjectWEAP>(); equipWeapR) {
            log::trace("Player equipWeapR form:{}. Is hand:{}", equipWeapR->formID, equipWeapR->IsHandToHandMelee());
            if (equipWeapR->HasKeywordString("WeapTypeDagger")) {
                isDaggerR = true;
            } else if (equipWeapR->HasKeywordString("WeapTypeMace")) {
                isMaceR = true;
            } else if (equipWeapR->HasKeywordString("WeapTypeSword")) {
                isSwordR = true;
            } else if (equipWeapR->HasKeywordString("WeapTypeWarAxe")) {
                isAxeR = true;
            } else if (equipWeapR->IsStaff()) {
                isStaffR = true;
            } else if (equipWeapR->IsHandToHandMelee()) {
                if (isPlayer) {
                    // if player is wearing any gauntlet, set fist to true
                    isFistR = hasGauntlet;
                } 
            }
        }
    } else {
        if (isPlayer) {
            // if player is wearing any gauntlet, set fist to true
            isFistR = hasGauntlet;
        } else {
            // We have more restrict ways to check if enemy is using fist
            RE::AIProcess* const attackerAI = actor->GetActorRuntimeData().currentProcess;
            if (attackerAI) {
                const RE::TESForm* equipped = attackerAI->GetEquippedRightHand();
                if (equipped) {
                    auto weapHand = equipped->As<RE::TESObjectWEAP>();
                    if (weapHand && weapHand->IsHandToHandMelee()) {
                        isFistR = true;
                    }
                }
            }
        }
    }
    if (auto equipL = actor->GetEquippedObject(true); equipL) {
        isEquipL = true;
        if (auto equipWeapL = equipL->As<RE::TESObjectWEAP>(); equipWeapL) {
            log::trace("Player equipWeaLR form:{}. Is hand:{}", equipWeapL->formID, equipWeapL->IsHandToHandMelee());
            if (equipWeapL->IsBow()) {
                isBow = true;
                isEquipL = true;
                isEquipR = false;
            } else if (equipWeapL->IsTwoHandedSword()) {
                isGreatSword = true;
                isEquipL = false;
                isEquipR = true;
            } else if (equipWeapL->IsTwoHandedAxe() ) {
                isTwoHandedAxe = true;
                isEquipL = false;
                isEquipR = true;
            } else if (equipWeapL->HasKeywordString("WeapTypeWarhammer")) {
                isWarHammer = true;
                isEquipL = false;
                isEquipR = true;
            } else if (equipWeapL->HasKeywordString("WeapTypeDagger")) {
                isDaggerL = true;
            } else if (equipWeapL->HasKeywordString("WeapTypeMace")) {
                isMaceL = true;
            } else if (equipWeapL->HasKeywordString("WeapTypeSword")) {
                isSwordL = true;
            } else if (equipWeapL->HasKeywordString("WeapTypeWarAxe")) {
                isAxeL = true;
            } else if (equipWeapL->IsStaff()) {
                isStaffL = true;
            } else if (equipWeapL->IsHandToHandMelee()) {
                if (isPlayer) {
                    // if player is wearing any gauntlet, set fist to true
                    isFistL = hasGauntlet;
                }
            }
        }
        if (equipL->IsArmor()) {
            hasShield = true;
            
        }
    } else {
        if (isPlayer) {
            // if player is wearing any gauntlet, set fist to true
            isFistL = hasGauntlet;
        } else {
            // We have more restrict ways to check if enemy is using fist
            RE::AIProcess* const attackerAI = actor->GetActorRuntimeData().currentProcess;
            if (attackerAI) {
                const RE::TESForm* equipped = attackerAI->GetEquippedLeftHand();
                if (equipped) {
                    auto weapHand = equipped->As<RE::TESObjectWEAP>();
                    if (weapHand && weapHand->IsHandToHandMelee()) {
                        isFistL = true;
                    }
                }
            }
        }
    }

    log::trace("isFistL:{}, isFistR:{}", isFistL, isFistR);

    // Handling some special actors
    // 3. Races that use claws as weapon Werewolf or werebear or troll or bear or spriggan or hagraven
    twoNodes claws = HandleClawRaces(actor, posWeaponBottomL, posWeaponBottomR, posWeaponTopL, posWeaponTopR);
    if (!claws.isEmpty()) {
        if ((!isPlayer && bDisplayEnemySegmentCheck) || (isPlayer && bShowEmitSpellDirection)) {
            debug_show_weapon_range(actor, posWeaponBottomR, posWeaponTopR, claws.nodeR);
            debug_show_weapon_range(actor, posWeaponBottomL, posWeaponTopL, claws.nodeL);
        }
    }

    // Handling normal humanoid actors
    if (claws.isEmpty() && weaponL 
        &&
        (isEquipL || (isFistL && isFistR) || (hasShield)) &&
        !(!isPlayer && isBow) && !(!isPlayer && isStaffL)  // ignore enemy's bow and staff
        ) { // only enable fist collision when both hands are fist
        float reachL(0.0f), handleL(0.0f);
        if (isFistL) { 
            reachL = 10.0f;
            handleL = 10.0f;
            log::trace("Left: fist. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isBow) { // OK
            reachL = 70.0f;
            handleL = 70.0f;
            log::trace("Left: bow. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isDaggerL) { // OK
            reachL = 20.0f;
            handleL = 10.0f;
            log::trace("Left: dagger. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isSwordL) { // OK
            reachL = 90.0f;
            handleL = 12.0f;
            log::trace("Left: sword. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isAxeL) { // OK
            reachL = 60.0f;
            handleL = 12.0f;
            log::trace("Left: axe. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isMaceL) { // OK
            reachL = 60.0f;
            handleL = 12.0f;
            log::trace("Left: mace. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isStaffL) { // OK
            reachL = 70.0f;
            handleL = 70.0f;
            log::trace("Left: staff. actor:{}", actor->GetBaseObject()->GetName());
        } else if (hasShield) {
            log::trace("Left: Shield. actor:{}", actor->GetBaseObject()->GetName());

            
        } else {
            log::trace("Left: unknown. actor:{}", actor->GetBaseObject()->GetName());
        }
        if (reachL > 0.0f) {
            posWeaponBottomL = weaponL->world.translate;
            const auto weaponDirectionL =
                RE::NiPoint3{weaponL->world.rotate.entry[0][1], weaponL->world.rotate.entry[1][1],
                             weaponL->world.rotate.entry[2][1]};
            posWeaponTopL = posWeaponBottomL + weaponDirectionL * reachL;
            posWeaponBottomL = posWeaponBottomL - weaponDirectionL * handleL;

            if ((bShowEmitSpellDirection && isPlayer) ||
                (!isPlayer && bDisplayEnemySegmentCheck))
                // 1234.0f is a magic number to prevent showing effects twice
                debug_show_weapon_range(actor, posWeaponBottomL, posWeaponTopL, weaponL);
        }

    } else {
        // Animals, dragons will reach here
        log::trace("Doesn't get left weapon. actor:{}", actor->GetBaseObject()->GetName());
        
    }
    
    if (claws.isEmpty() && weaponR && 
        (isEquipR || (isFistL && isFistR)) && 
        !(!isPlayer && isBow) && !(!isPlayer && isStaffR)
        ) {
        float reachR(0.0f), handleR(0.0f);
        if (isFistR) {
            reachR = 10.0f;
            handleR = 10.0f;
            log::trace("Right: fist. actor:{}", actor->GetBaseObject()->GetName());
        }else if (isTwoHandedAxe || isWarHammer) { // OK
            reachR = 70.0f;
            handleR = 45.0f;
            log::trace("Right: two-handed axe/hammer. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isGreatSword) { // OK
            reachR = 100.0f;
            handleR = 20.0f;
            log::trace("Right: two-handed greatsword. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isDaggerR) { // OK
            reachR = 20.0f;
            handleR = 10.0f;
            log::trace("Right: dagger. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isSwordR) {  // OK
            reachR = 90.0f;
            handleR = 12.0f;
            log::trace("Right: sword. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isAxeR) {  // OK
            reachR = 60.0f;
            handleR = 12.0f;
            log::trace("Right: axe. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isMaceR) {  // OK
            reachR = 60.0f;
            handleR = 12.0f;
            log::trace("Right: mace. actor:{}", actor->GetBaseObject()->GetName());
        } else if (isStaffR) {  // OK
            reachR = 70.0f;
            handleR = 70.0f;
            log::trace("Right: staff. actor:{}", actor->GetBaseObject()->GetName());
        } else {
            log::trace("Right: unknown. actor:{}", actor->GetBaseObject()->GetName());
        }
        if (reachR > 0.0f) {
            posWeaponBottomR = weaponR->world.translate;
            const auto weaponDirectionR =
                RE::NiPoint3{weaponR->world.rotate.entry[0][1], weaponR->world.rotate.entry[1][1],
                             weaponR->world.rotate.entry[2][1]};
            posWeaponTopR = posWeaponBottomR + weaponDirectionR * reachR;
            posWeaponBottomR = posWeaponBottomR - weaponDirectionR * handleR;

            if ((bShowEmitSpellDirection && isPlayer) ||
                (!isPlayer && bDisplayEnemySegmentCheck)) {
                debug_show_weapon_range(actor, posWeaponBottomR, posWeaponTopR, weaponR);
            }
        }
    } else {
        log::trace("Doesn't get right weapon. actor:{}", actor->GetBaseObject()->GetName());
    }

    
    return true;
}
RE::hkVector4 ZacOnFrame::CalculatePushVector(RE::NiPoint3 sourcePos, RE::NiPoint3 targetPos, bool isEnemy, float speed) {
    RE::NiPoint3 pushDirection = targetPos - sourcePos;
    // Normalize the direction
    float length =
        sqrt(pushDirection.x * pushDirection.x + pushDirection.y * pushDirection.y);
    if (length > 0.0f) {  // avoid division by zero
        pushDirection.x /= length;
        pushDirection.y /= length; 
    }
    float pushMulti = 70.0f;

    // modify pushMulti by the speed of player's weapon at collision
    float speedFactor;
    if (speed < 20.0f) {
        speedFactor = 0.1f;
    } else if (speed < 40.0f) {
        speedFactor = 0.3f;
    } else if (speed < 60.0f) {
        speedFactor = 0.5f;
    } else if (speed < 80.0f) {
        speedFactor = 0.7f;
    } else if (speed < 100.0f) {
        speedFactor = 0.9f;
    } else {
        speedFactor = 1.0f;
    }
    if (!isEnemy) speedFactor = 1.0f - speedFactor;
    pushMulti = pushMulti * speedFactor;
    log::trace("pushMulti:{}. IsEnemy:{}. Before x:{}, y:{}", pushMulti, isEnemy, pushDirection.x, pushDirection.y);

    return RE::hkVector4(pushDirection.x * pushMulti, pushDirection.y * pushMulti, 0.0f, 0.0f);
}

// Clear every buffer before player load a save
void ZacOnFrame::CleanBeforeLoad() { 
    slowTimeData.clear();
    iFrameCount = 0;
    iLastPressGrip = 0;
    PlayerState::GetSingleton().Clear();
    WindManager::GetSingleton().Clear();
}


void ZacOnFrame::FillShieldCenterNormal(RE::Actor* actor, RE::NiPoint3& shieldCenter, RE::NiPoint3& shieldNormal) {
    const auto actorRoot = netimmerse_cast<RE::BSFadeNode*>(actor->Get3D());
    if (!actorRoot) {
        log::warn("Fail to get actorRoot:{}", actor->GetBaseObject()->GetName());
        return;
    }

    const auto nodeNameL = "SHIELD"sv;
    auto weaponNodeL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeNameL));

    const auto nodeBaseStr = "NPC Pelvis [Pelv]"sv;  // base of player
    const auto baseNode = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeBaseStr));

    if (weaponNodeL) {
        shieldCenter = weaponNodeL->world.translate;

        auto rotationL = weaponNodeL->world.rotate;
        rotationL = adjustNodeRotation(baseNode, rotationL, RE::NiPoint3(1.50f, 0.0f, 0.0f), false);

        shieldNormal = RE::NiPoint3{rotationL.entry[0][1], rotationL.entry[1][1], rotationL.entry[2][1]};

        // slightly move shield center towards shield normal a little, since previously center is hand
        shieldCenter = shieldCenter + shieldNormal * 7.0f;

        auto pointOutShield = shieldNormal * 30.5f + shieldCenter;

        if (bShowEmitSpellDirection) debug_show_weapon_range(actor, shieldCenter, pointOutShield, weaponNodeL);
    }
}