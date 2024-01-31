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
    // Whole mod toggle
    if (GetMyBoolConf(bEnableWholeMod) == false) {
        _SetVelocity(controller, a_velocity);
        return;
    }

    auto& playerSt = PlayerState::GetSingleton();
    if (!playerSt.player || !playerSt.player->Is3DLoaded()) {
        allEffects.ZeroAccumulated();
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
    //if (controller->jumpHeight != 0 && playerSt.isDragonNearby) {
    if (controller->jumpHeight != 0) {
        log::trace("jump height:{}. Not player. We still don't know why this is working", controller->jumpHeight);
        _SetVelocity(controller, a_velocity);
        return;
    }

    if (playerSt.player->IsInWater()) {
        allEffects.ZeroAccumulated();
        _SetVelocity(controller, a_velocity);
        return;
    }

    if (auto charController = playerSt.player->GetCharController(); charController) {
        if (charController->flags.any(RE::CHARACTER_FLAGS::kJumping)) {
            log::trace("Jumping");
            playerSt.lastJumpFrame = iFrameCount;
            allEffects.ZeroAccumulated();
            _SetVelocity(controller, a_velocity);
            return;
        }
    }

    // TODO: if we want to cast wings shout when double jump, we need to modify the following
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

    float timeSlowSWRatio = CurrentSpellWheelSlowRatio(playerSt.player);
    auto ourVelo = playerSt.velocity;
    if (timeSlowSWRatio < 0.9f) {
        ourVelo = ourVelo * timeSlowSWRatio;
        //log::trace("Detected slow time when setting velo. Ratio:{}", timeSlowRatio);
    }

    float timeSlowRatioMine = CurrentMyTimeSlowRatio(playerSt.player);
    // If slowing time, passed time should be reduced
    if (timeSlowRatioMine < 0.9f && timeSlowSWRatio > 0.9f) ourVelo = ourVelo * timeSlowRatioMine;

    if (playerSt.setVelocity) {
        // If the only effect is wings and player is not flapping wings and player on ground, combine the velocity
        // TODO: problem with jump
        if (playerSt.isEffectOnlyWings && !playerSt.isflappingWings && !isInMidAir) {
            log::trace("Combine velocity");
            auto combined = a_velocity + ourVelo;
            _SetVelocity(controller, combined);
            return;
        } else if (playerSt.isEffectOnlyWings && !playerSt.isflappingWings && isInMidAir) {
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

    log::trace("Origin set velocity");
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
    // Whole mod toggle
    if (GetMyBoolConf(bEnableWholeMod) == false) {
        ZacOnFrame::_OnFrame();  
        return;
    }

    if (isOurFnRunning) {
        log::warn("Our functions are running in parallel!!!"); // Not seen even when stress testing. Keep observing
    }
    isOurFnRunning = true;
    // Get the current time point
    auto now = std::chrono::high_resolution_clock::now();
    bool isPaused = true;
    if (const auto ui{RE::UI::GetSingleton()}) {
        // Check if it has been a while since the last_time, and if so, don't do anything for a few seconds
        // This can disable our mod after loading screen, to avoid bugs
        auto dur_last = std::chrono::duration_cast<std::chrono::microseconds>(now - last_time);
        last_time = now;
        if (dur_last.count() > 1000 * 1000) {  // 1 second
            count_after_pause = 60;
            CleanBeforeLoad();
            log::info(
                "Detected long period since last frame. Player is probably loading. Clean and pause our functions for "
                "3 seconds");
        }
        if (count_after_pause > 0) count_after_pause--;

        if (!ui->GameIsPaused() && count_after_pause <= 0) {  // Not in menu, not in console, but can't detect in load

            FlyMain();

            iFrameCount++;
            isPaused = false;
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
    // Check if need to cancel flyup animation
    playerSt.UpdateAnimation();
    // Check if player is suddenly turning
    playerSt.DetectSuddenTurn();
    // Update ExtraLiftManager
    ExtraLiftManager::GetSingleton().Update();
    // See if we need to spawn something
    playerSt.SpawnCheck();

    // Every 10 frames, scan for dragons, living creatures, firespots
    if (iFrameCount % 10 == 7) {
        playerSt.SparseScan(1500.0f, false);
    }

    // Every 30 frames, give player stamina cost and check if remove stamina regen punishment
    if (iFrameCount % 30 == 11 ) {
        if (playerSt.accumulatedStamCost > 1.0f) playerSt.CommitStaminaCost();

        if (auto spStaRegen = GetFlapStaRegenEffSpell(); spStaRegen) {
            if (player->HasSpell(spStaRegen)) {
                auto now = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - playerSt.lastFlap);
                auto conf_staminaRegenReduceTime = GetMyConf(fFlapStaminaReduceTime);
                if (duration.count() > conf_staminaRegenReduceTime || !playerSt.isInMidAir) {
                    player->RemoveSpell(spStaRegen);
                    log::debug("Removing damage regen spell from player. Dur count:{}. ReduceTime:{}. IsinMidair:{}", duration.count(),
                               conf_staminaRegenReduceTime, playerSt.isInMidAir);
                }
            }
        }
    }

    
    // PART I ======= Spell checking
    SpellCheckMain();

    // PART II ====== Dragon wings checking
    WingsCheckMain();


    // PART III ======= Change player velocity
    Velo sumAll = allEffects.SumCurrentVelo();
    float conf_maxVelo = GetMyConf(fMaxSpeed) * GetMyConf(fMultiSpeed);
    if (sumAll.Length() > conf_maxVelo) {
        if (iFrameCount - playerSt.lastNotification > 360 && GetMyBoolConf(bEnableNotification)) {
            playerSt.lastNotification = 0;
            RE::DebugNotification("Reached max velocity");
        }
        sumAll = sumAll / sumAll.Length() * conf_maxVelo;
    }

    float conf_maxVeloZ = GetMyConf(fMaxSpeedZ);
    if (sumAll.z < -conf_maxVeloZ) sumAll.z = -conf_maxVeloZ;
    if (sumAll.z > conf_maxVeloZ) sumAll.z = conf_maxVeloZ;
    if (!allEffects.IsForceEffectEmpty() || !allEffects.IsVeloEffectEmpty()) {
        playerSt.SetVelocity(sumAll.x, sumAll.y, sumAll.z);
        playerSt.CancelFallNumber();

        playerSt.setVelocity = true;
        playerSt.isEffectOnlyWings = allEffects.IsEffectOnlyWings();
    } else {
        playerSt.isEffectOnlyWings = false;
        playerSt.setVelocity = false;
    }

    
    // PART IV ====== Effects due to velocity
    VeloEffectMain(sumAll);
    WindManager::GetSingleton().Update();
    if (playerSt.frameShouldSlowTime != 0 && playerSt.frameShouldSlowTime == iFrameCount) {
        log::trace("Apply slow motion effect");
        playerSt.frameShouldSlowTime = 0;
        TimeSlowEffect(playerSt.player, 90, 0.3f);
    }
    auto& slowTimeData = SlowTimeEffect::GetSingleton();
    if (slowTimeData.shouldRemove()) {
        StopTimeSlowEffect(playerSt.player);
    }

    // Test: see if we can determine if a dragon is present
}

void ZacOnFrame::TimeSlowEffect(RE::Actor* playerActor, int64_t slowFrame, float slowRatio) { 
    if (slowFrame <= 0 || slowRatio >= 1.0f || slowRatio <= 0.0f) {
        log::trace("Due to settings, no time slow effect");
        return;
    }
    auto& slowTimeData = SlowTimeEffect::GetSingleton();


    if (iFrameCount - slowTimeData.frameLastSlowTime < 50 && iFrameCount - slowTimeData.frameLastSlowTime > 0) {
        log::trace("The last slow time effect is within 50 frames. Don't slow time now");
        return;
    }

    if (slowTimeData.timeSlowSpell == nullptr) {
        RE::SpellItem* timeSlowSpell = GetTimeSlowSpell_Mine();
        if (!timeSlowSpell) {
            log::trace("TimeSlowEffect: failed to get timeslow spell");
            return;
        }
        slowTimeData.timeSlowSpell = timeSlowSpell;
    }

    if (playerActor->HasSpell(slowTimeData.timeSlowSpell)) {
        log::trace("TimeSlowEffect: Spell wheel or us are already slowing time, don't do anything");
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
        effect->effectItem.magnitude = fTimeSlowRatio; // set our magnitude
    }

    // Apply spell to player. Now time slow will take effect
    playerActor->AddSpell(slowTimeData.timeSlowSpell);
    log::trace("time slow takes effect", fTimeSlowRatio);
}

void ZacOnFrame::StopTimeSlowEffect(RE::Actor* playerActor) {
    auto& slowTimeData = SlowTimeEffect::GetSingleton();
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

    slowTimeData.clear();
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
    SlowTimeEffect::GetSingleton().clear();
    iFrameCount = 0;
    iLastPressGrip = 0;
    PlayerState::GetSingleton().Clear();
    WindManager::GetSingleton().Clear();
    ExtraLiftManager::GetSingleton().Clear();
}
