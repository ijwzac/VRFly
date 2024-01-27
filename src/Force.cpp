#include "Force.h"

void VeloEffectMain(Velo& vel) {
    auto& playerSt = PlayerState::GetSingleton();

	// When velocity is high, occasionally create wind objects
    auto midWind = GetWindMid();
    auto lgWind = GetWindLg();
    auto exWind = GetWindEx();
    if (!midWind || !lgWind || !exWind) {
        log::error("Failed to get winds from my mod");
        return;
    }

    auto random = rand();
    if (iFrameCount % 150 == 0 ) {
        log::trace("About to check gen wind. Velo: {}", vel.Length());
        RE::NiPointer<RE::TESObjectREFR> newWind;
        WindObj::WindType type;
        if (vel.Length() > 27.0f) {  //  
            type = WindObj::ex;
            newWind = playerSt.player->PlaceObjectAtMe(exWind, false);
        } else if (vel.Length() > 20.0f) {
            if (iFrameCount % 300 == 0) {
                type = WindObj::lg;
                newWind = playerSt.player->PlaceObjectAtMe(lgWind, false);
            }
        } else if (vel.Length() > 5.0f) {
            // We stop using smallwind
            type = WindObj::mid;
            newWind = playerSt.player->PlaceObjectAtMe(midWind, false);
        }
        if (newWind) {
            auto windObj = WindObj(newWind, type, iFrameCount);
            WindManager::GetSingleton().Push(windObj);
        }
    }

    // If this is the first frame on ground, see if we create a shockwave
    auto smExplo = GetExploSm();
    auto midExplo = GetExploMid();
    auto lgExplo = GetExploLg();
    auto rockExplo = GetExploRock();
    auto shockWaveSpell = GetShockWaveSpell();
    if (!smExplo || !midExplo || !lgExplo || !rockExplo || !shockWaveSpell) {
        log::error("Failed to get explosions from my mod");
        return;
    }
    if (playerSt.shouldCheckKnock) {
        playerSt.shouldCheckKnock = false;
        float highestVel = 0.0f;
        for (auto recentVel : playerSt.recentVelo) {
            if (highestVel < recentVel) highestVel = recentVel;
        }
        float conf_StrongKnockThres(27.0f), conf_MediumKnockThres(17.0f), conf_WeakKnockThres(10.0f);

        log::trace("About to check gen knock. highestVel: {}", highestVel);
        if (highestVel > conf_WeakKnockThres) {
            if (highestVel > conf_StrongKnockThres) {
                log::trace("Creating large landing explosion");
                playerSt.player->PlaceObjectAtMe(lgExplo, false);

                // The spell shockWaveSpell will be removed by WindManager

                if (!playerSt.player->HasSpell(shockWaveSpell)) playerSt.player->AddSpell(shockWaveSpell);
                WindManager::GetSingleton().frameLastRockExplo = iFrameCount;

                // If player is sneaking or hands position low, play slow motion effect. Superhero landing!
                auto leftPos = GetPlayerHandPos(true, playerSt.player);
                auto rightPos = GetPlayerHandPos(false, playerSt.player);
                if (playerSt.player->IsSneaking() || leftPos.z < 80.0f || rightPos.z < 80.0f) {
                    playerSt.frameShouldSlowTime = iFrameCount + 10;
                }
            } else if (highestVel > conf_MediumKnockThres) {
                log::trace("Creating medium landing explosion");
                playerSt.player->PlaceObjectAtMe(midExplo, false);
            } else {
                log::trace("Creating small landing explosion");
                playerSt.player->PlaceObjectAtMe(smExplo, false);
            }
            // Note: don't use playerSt.player->GetActorRuntimeData().currentProcess->KnockExplosion
            // It only knocks down player, and this knock ignores my no stagger perk

            // Sound: like thunder NPCDragonDeathSequenceExplosionSD. Wing attack: NPCDragonAttackWingSD
            // Hit: NPCDragonLandSD > NPCDragonAttackTailSD > NPCDragonFootWalkRSD
            // Dragon flyby: NPCDragonFlybySD. Takeoff: NPCDragonTakeoffFXSD. Wing flap: NPCDragonWingFlapSD
            // Wing flap 2: NPCDragonWingFlapTakeoffSD
            // Land with dust: NPCDragonLandCrashLongSD
        }
    }
}