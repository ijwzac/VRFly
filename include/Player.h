#include "Utils.h"
#include "Settings.h"

using namespace SKSE;


class ExtraLiftManager {
public:
    struct SpiritualData {
        RE::Actor* fromWho;
        float passByHour;
        int64_t lastSteamSpawnPrivate;
    };
    int64_t lastSteamSpawnFrame;
    std::vector<SpiritualData> vSpiritual;
    ExtraLiftManager() : lastSteamSpawnFrame(0) {}

    // Pretty typical singleton setup
    // *Private* constructor/destructor
    // And we *delete* the copy constructors and move constructors.
    ~ExtraLiftManager() = default;
    ExtraLiftManager(const ExtraLiftManager&) = delete;
    ExtraLiftManager(ExtraLiftManager&&) = delete;
    ExtraLiftManager& operator=(const ExtraLiftManager&) = delete;
    ExtraLiftManager& operator=(ExtraLiftManager&&) = delete;

    static ExtraLiftManager& GetSingleton() {
        static ExtraLiftManager singleton;
        return singleton;
    }

    void Update() {
        // Delete spiritual data that is 12 hours ago
        if (iFrameCount % 100 == 17) {
            auto calendar = RE::Calendar::GetSingleton();
            if (!calendar) {
                log::error("Fail to get RE::Calendar");
                return;
            }
            float currentHours = calendar->GetHoursPassed();
            for (int i = 0; i < vSpiritual.size(); i++) {
                if (currentHours - vSpiritual[i].passByHour > 12) {
                    vSpiritual.erase(vSpiritual.begin() + i);
                }
            }
        }

        if (iFrameCount % 1000 == 23) log::trace("In the past 12 hours, passed {} actors", NumPassedByActorRecent());
    }

    int NumPassedByActorRecent() { return vSpiritual.size();
    }

    void AddSpiritual(RE::Actor* who, RE::Actor* player) {
       
        //bool shouldSpawnSteam = false;

        auto calendar = RE::Calendar::GetSingleton();
        if (!calendar) {
            log::error("Fail to get RE::Calendar");
            return;
        }
        float currentHours = calendar->GetHoursPassed();

        int indexExist = -1;
        for (int i = 0; i < vSpiritual.size(); i++) {
            if (vSpiritual[i].fromWho == who) {
                indexExist = i;
                break;
            }
        }

        if (indexExist >= 0) {
            //shouldSpawnSteam = (iFrameCount - vSpiritual[indexExist].lastSteamSpawnPrivate) > 20 * 120;
            vSpiritual[indexExist].lastSteamSpawnPrivate = iFrameCount;
            vSpiritual[indexExist].passByHour = currentHours;
        } else {
            //shouldSpawnSteam = true;
            vSpiritual.push_back(SpiritualData(who, currentHours, iFrameCount));
        }

        //shouldSpawnSteam = shouldSpawnSteam && (iFrameCount - lastSteamSpawnFrame > 300);
        ////shouldSpawnSteam = (iFrameCount - lastSteamSpawnFrame) > 600;
        //if (auto smSteam = GetSteamSm(); smSteam && shouldSpawnSteam) {
        //    lastSteamSpawnFrame = iFrameCount;
        //    player->PlaceObjectAtMe(
        //        smSteam, false);  // smSteam will delete itself after 20 seconds, by our script
        //} else {
        //    log::error("Failed to get steam small");
        //}
    }

    void Clear() {
        lastSteamSpawnFrame = 0;
    }
};

class SpeedRing {
public:
    const RE::NiPoint3 emptyPoint = RE::NiPoint3(123.0f, 0.0f, 0.0f);

    std::vector<RE::NiPoint3> bufferL;
    std::vector<RE::NiPoint3> bufferR;
    std::size_t capacity;  // how many latest frames are stored
    std::size_t indexCurrentL;
    std::size_t indexCurrentR;
    SpeedRing(std::size_t cap) : bufferL(cap), bufferR(cap), capacity(cap), indexCurrentR(0), indexCurrentL(0) {}

    void Clear() {
        for (std::size_t i = 0; i < capacity; i++) {
            bufferL[i] = emptyPoint;
            bufferR[i] = emptyPoint;
        }
    }

    void Push(RE::NiPoint3 p, bool isLeft) {
        if (isLeft) {
            bufferL[indexCurrentL] = p;
            indexCurrentL = (indexCurrentL + 1) % capacity;
        } else {
            bufferR[indexCurrentR] = p;
            indexCurrentR = (indexCurrentR + 1) % capacity;
        }
    }

    // Thanks ChatGPT for generating this function
    RE::NiPoint3 GetVelocity(std::size_t N, bool isLeft) const {
        if (N == 0 || N > capacity) {
            log::error("N is 0 or larger than capacity");
            return RE::NiPoint3(0.0f, 0.0f, 0.0f);
        }

        std::size_t currentIdx = isLeft ? indexCurrentL : indexCurrentR;
        const std::vector<RE::NiPoint3>& buffer = isLeft ? bufferL : bufferR;

        // Get the start and end positions
        RE::NiPoint3 startPos = buffer[(currentIdx - N + capacity) % capacity];
        RE::NiPoint3 endPos = buffer[(currentIdx - 1 + capacity) % capacity];

        auto diff1 = startPos - emptyPoint;
        auto diff2 = endPos - emptyPoint;
        if (diff1.Length() < 0.01f || diff2.Length() < 0.01f) {
            log::error("startPos or endPos is empty");
            return RE::NiPoint3(0.0f, 0.0f, 0.0f);
        }


        // Calculate velocities
        RE::NiPoint3 velocityBottom = (endPos - startPos) / static_cast<float>(N);

        // Return the larger velocity based on magnitude
        return velocityBottom;
    }
};

class PlayerState {
public:
    RE::Actor* player;
    SpeedRing speedBuf;

    RE::TESObjectWEAP* leftWeap;
    RE::MagicItem* leftSpell;
    RE::TESObjectWEAP* rightWeap;
    RE::MagicItem* rightSpell;

    bool setVelocity;
    RE::hkVector4 velocity;
    std::vector<float> recentVelo;

    float accumulatedStamCost = 0.0f;

    bool isInMidAir = false; // Updated by HookSetVelocity, not OnFrame
    bool shouldCheckKnock = false;  // Set true in HookSetVelocity, set false in OnFrame
    bool isflappingWings = false;
    bool isEffectOnlyWings = false; // if wings is the one and only effect in allEffects
    bool hasWings = false;
    bool everSetWingDirSinceThisFlight = false; // false when player just started flying and hasn't holded both grips
    bool isSkyDiving = false;
    bool isDragonNearby = false;
    bool isInSpiritualLift = false;
    bool isInFireLift = false;

    int64_t lastJumpFrame = 0;
    int64_t frameLastflap = 0;
    std::chrono::steady_clock::time_point lastFlap;
    int64_t lastSoundFrame = 0;
    int64_t reenableVibrateFrame = 0;
    int64_t lastOngroundFrame = 0;
    int64_t frameShouldSlowTime = 0;   // Set in VeloEffectMain, reset in OnFrame
    int64_t lastFlyUpFrame = 0;
    int64_t lastNotification = 0;
    int64_t lastSuddenTurnFrame = 0;
    int64_t lastSpawnSteamFrame = 0;
    int64_t lastVisualEffect = 0;
    RE::NiPoint3 lastAngle;

    std::vector<RE::TESObjectREFR*> vNearbyFirespots;
    std::vector<RE::Actor*> vNearbyLiving;

    struct DelaySpawn {
        int64_t shouldSpawnFrame;
        RE::TESObjectREFR* atWhat;
        RE::TESObjectACTI* spawned;
    };
    std::vector<DelaySpawn> vDelaySpawn;

    RE::NiPoint3 dirWings = RE::NiPoint3(0.0f, 0.0f, 1.0f);  // A normalized pointer that is vertical to wings. Initialized to be pointing to the sky


    PlayerState()
        : player(nullptr),
          setVelocity(false),
          speedBuf(SpeedRing(100)),
          recentVelo(),
          vNearbyFirespots(),
          vNearbyLiving(),
          vDelaySpawn() {}

    void Clear() { 
        setVelocity = false;
        velocity = RE::NiPoint3();
        isInMidAir = false;
        isflappingWings = false;
        isEffectOnlyWings = false;
        shouldCheckKnock = false;
        hasWings = false;
        everSetWingDirSinceThisFlight = false;
        isSkyDiving = false;
        isDragonNearby = false;
        isInSpiritualLift = false;
        isInFireLift = false;

        lastJumpFrame = 0;
        frameLastflap = 0;
        lastSoundFrame = 0;
        lastOngroundFrame = 0;
        frameShouldSlowTime = 0;
        reenableVibrateFrame = 0;
        lastFlyUpFrame = 0;
        lastNotification = 0;
        lastSuddenTurnFrame = 0;
        lastSpawnSteamFrame = 0;
        lastVisualEffect = 0;

        speedBuf.Clear();
        recentVelo.clear();
        vNearbyFirespots.clear();
        vNearbyLiving.clear();
    }

    static PlayerState& GetSingleton() {
        static PlayerState singleton;
        if (singleton.player == nullptr) {
            // Get player
            auto playerCh = RE::PlayerCharacter::GetSingleton();
            if (!playerCh) {
                log::error("Can't get player!");
            }

            auto playerActor = static_cast<RE::Actor*>(playerCh);
            auto playerRef = static_cast<RE::TESObjectREFR*>(playerCh);
            if (!playerActor || !playerRef) {
                log::error("Fail to cast player to Actor or ObjRef");
            }
            singleton.player = playerActor;

        }
        

        return singleton;
    }

    void AddStaminaCost(int msElapsed) { 
        auto costPerSec = GetMyConf(fFlapStaminaCost) * GetMyConf(fMultiFlapStaminaCost);
        if (msElapsed > 50) {
            // fps too low, just add once
            accumulatedStamCost += costPerSec * 50.0f / 1000.0f;
        } else {
            accumulatedStamCost += costPerSec * msElapsed / 1000.0f;
        }
    }

    void CommitStaminaCost() {

        if (!player->AsActorValueOwner()) {
            log::error("Player can't be cast to actor value owner");
            return;
        }
        player->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina,
                                                       -accumulatedStamCost);
        accumulatedStamCost = 0.0f;
    }

    // A very heavy function. Scan every few frames, search for dragons, living creatures, firespots
    void SparseScan(float radius, bool onlyActor) {

        if (hasWings == false) return;

        const auto TES = RE::TES::GetSingleton();
        auto heatSourceFire = GetHeatSourceFire();
        if (!TES || !heatSourceFire) {
            log::error("Can't get TES or heatSourceFire");
            return;
        }
        vNearbyLiving.clear();
        if (!onlyActor) vNearbyFirespots.clear();
        bool findDragonThisScan = false;
        // Note: verified that the ForEachReferenceInRange is not concurrent
        //log::trace("Starting a scan at frame:{}", iFrameCount); 
        TES->ForEachReferenceInRange(player, radius, [&](RE::TESObjectREFR& b_ref) {
            if (b_ref.Is(RE::FormType::ActorCharacter)) {
                auto actor = static_cast<RE::Actor*>(&b_ref);
                auto race = actor->GetRace();
                if (actor != player && !actor->IsDead() && !actor->IsDisabled() && actor->Is3DLoaded() &&
                    !actor->IsDeleted()) {
                    if (race->HasKeywordString("ActorTypeDragon"sv)) {
                        findDragonThisScan = true;
                    } else {
                        if (!race->HasKeywordString("ActorTypeUndead") &&
                            !race->HasKeywordString("ActorTypeFamiliar") && !race->HasKeywordString("ActorTypeGhost") &&
                            !race->HasKeywordString("ActorTypeDaedra")) {
                            //log::trace("Find a living creature at frame:{}", iFrameCount);
                            vNearbyLiving.push_back(actor);
                        }
                    }
                }
            } else if (onlyActor == false) {
                // Seems that STAT are not object reference
                auto baseObj = b_ref.GetBaseObject();
                if (baseObj) {
                    if (baseObj->Is(RE::FormType::MovableStatic)) {
                        
                        // Should include forms with "Brazier", "fire" but not "firewood". In Static and movable static
                        if (heatSourceFire->HasForm(baseObj->formID)) {
                            vNearbyFirespots.push_back(&b_ref);
                        }
                         
                        // Why deprecated: Form names are not loaded into the game unless using po3's tweaks
                        // auto editorID = baseObj->GetFormEditorID(); 
                        // auto name = baseObj->GetFormEditorID();
                        // if (auto length = strlen(name); length > 0) {
                        //     char* lowerCaseName = new char[strlen(name) + 1];  // +1 for the null terminator
                        //     for (size_t i = 0; name[i] != '\0'; ++i) {
                        //         lowerCaseName[i] = std::tolower(static_cast<unsigned char>(name[i]));
                        //     }
                        //     lowerCaseName[strlen(name)] = '\0';
                        //     log::debug("Name:{}", lowerCaseName);
                        //     if (strstr(lowerCaseName, "house") || strstr(lowerCaseName, "smithforge") ||
                        //         (strstr(lowerCaseName, "fire") && !strstr(lowerCaseName, "firewood"))) {
                        //         vNearbyFirespots.push_back(&b_ref);
                        //     }
                        // }
                    }

                }
            }



            return RE::BSContainer::ForEachResult::kContinue;
            });

        log::trace("Num of vNearbyLiving:{}", vNearbyLiving.size());
        if (!onlyActor) log::debug("Num of vNearbyFirespots:{}", vNearbyFirespots.size());
        isDragonNearby = findDragonThisScan;
    }

    void SpawnSteamNearby(float range) {
        auto pos = player->GetPosition();
        auto smSteam = GetSteamSm();
        auto lgSteam = GetSteamLg();
        if (!smSteam || !lgSteam) {
            log::error("Failed to get steams");
            return;
        }

        log::debug("SpawnSteam for vNearbyLiving:{}, vNearbyFirespots:{}", vNearbyLiving.size(), vNearbyFirespots.size());


        for (auto actor : vNearbyLiving) {
            if (actor->Is3DLoaded()) {
                auto posOther = actor->GetPosition();
                auto diff = posOther - pos;
                log::trace("Distance player and living:{}", diff.Length());
                if (diff.Length() < range) {
                    auto randomI = rand() % 100;
                    vDelaySpawn.push_back(DelaySpawn(iFrameCount + randomI, actor, smSteam));
                }
            }
        }

        for (auto fireSpot : vNearbyFirespots) {
            if (fireSpot->Is3DLoaded()) {
                auto posOther = fireSpot->GetPosition();
                auto diff = posOther - pos;
                log::debug("Distance player and firespot:{}", diff.Length());
                if (diff.Length() < range) {
                    auto randomI = rand() % 100;
                    vDelaySpawn.push_back(DelaySpawn(iFrameCount + randomI, fireSpot, lgSteam));
                }
            }
        }
    }

    void SpawnCheck() {
        for (int i = 0; i < vDelaySpawn.size(); i++) {
            if (iFrameCount > vDelaySpawn[i].shouldSpawnFrame) {
                vDelaySpawn[i].atWhat->PlaceObjectAtMe(vDelaySpawn[i].spawned, false);
                auto id = vDelaySpawn[i].spawned->formID;
                log::debug("vDelaySpawn size:{}. Spawned {:x}", vDelaySpawn.size(), id);
                vDelaySpawn.erase(vDelaySpawn.begin() + i);
                break; // only spawn 1 thing per frame
            }
        }
    }

    RE::Actor* FindActorAtFoot() { 
        auto posX = player->GetPositionX();
        auto posY = player->GetPositionY();
        for (RE::Actor* actor : vNearbyLiving) {
            auto actorPos = actor->GetPosition();
            auto diffX = actorPos.x - posX;
            auto diffY = actorPos.y - posY;
            int conf_radius = GetMyIntConf(lSpiritualLiftRadius);
            if (diffX * diffX + diffY * diffY < conf_radius * conf_radius) {  // about 5.6 meters
                log::trace("Find actor at foot");
                return actor;
            }
        }

        return nullptr;
    }

    RE::TESObjectREFR* FindFirespotAtFoot() {
        auto posX = player->GetPositionX();
        auto posY = player->GetPositionY();
        int conf_radius = GetMyIntConf(lFireLiftRadius);
        for (RE::TESObjectREFR* fire : vNearbyFirespots) {
            auto firePos = fire->GetPosition();
            auto diffX = firePos.x - posX;
            auto diffY = firePos.y - posY;
            if (diffX * diffX + diffY * diffY < conf_radius * conf_radius) {  // about 7 meters
                log::trace("Find fire at foot");
                return fire;
            }
        }

        return nullptr;
    }

    void UpdateAnimation() {
        // If flyingUp animation was fired, we will replace it with another more reasonable one later
        if (iFrameCount - lastFlyUpFrame > 500 && lastFlyUpFrame != 0) {
            lastFlyUpFrame = 0;
            log::trace("Play animation FootLeft");
            player->NotifyAnimationGraph("FootLeft"sv);
        }
    }

    void UpdateEquip() {
        auto equipR = player->GetEquippedObject(false);
        rightSpell = nullptr;
        rightWeap = nullptr;
        if (equipR) {
            if (auto equipWeapR = equipR->As<RE::TESObjectWEAP>(); equipWeapR) {
                rightWeap = equipWeapR;
            } else if (auto equipSpellR = equipR->As<RE::MagicItem>(); equipSpellR && equipR->IsMagicItem()) {
                rightSpell = equipSpellR;
            }
        }

        auto equipL = player->GetEquippedObject(true);
        leftSpell = nullptr;
        leftWeap = nullptr;
        if (equipL) {
            if (auto equipWeapL = equipL->As<RE::TESObjectWEAP>(); equipWeapL) {
                leftWeap = equipWeapL;
            } else if (auto equipSpellL = equipL->As<RE::MagicItem>(); equipSpellL && equipL->IsMagicItem()) {
                leftSpell = equipSpellL;
            }
        }
    }

    void UpdateSpeedBuf() {
        const auto actorRoot = netimmerse_cast<RE::BSFadeNode*>(player->Get3D());
        if (!actorRoot) {
            log::warn("Fail to get actorRoot:{}", player->GetBaseObject()->GetName());
            return;
        }

        const auto nodeNameL = "NPC L Hand [LHnd]"sv;
        const auto nodeNameR = "NPC R Hand [RHnd]"sv;

        auto weaponNodeL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeNameL));
        auto weaponNodeR = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeNameR));


        if (weaponNodeL && weaponNodeR) {
            auto playerPos = player->GetPosition();
            auto handPosL = weaponNodeL->world.translate - playerPos;
            auto handPosR = weaponNodeR->world.translate - playerPos;

            speedBuf.Push(handPosL, true);
            speedBuf.Push(handPosR, false);
        }
    }

    void UpdateRecentVel() {
        if (recentVelo.size() >= 20) {
            recentVelo.erase(recentVelo.begin());
        }
        recentVelo.push_back(Quad2Velo(velocity).Length());
    }

    void DetectSuddenTurn() { 
        if (player->Is3DLoaded()) {
            auto currentAngle = player->GetAngle();
            static float maxZ = 0.0f;
            static float minZ = 3.0f;
            maxZ = currentAngle.z > maxZ ? currentAngle.z : maxZ;
            minZ = currentAngle.z < minZ ? currentAngle.z : minZ;
            log::trace("current angle z:{}. MaxZ:{}, MinZ:{}", currentAngle.z, maxZ, minZ);
            const float pi = 3.14159f;
            
            // Note that angle 2pi and angle 0 is the same
            float diff1 = abs(currentAngle.z - lastAngle.z);
            float diff2 = currentAngle.z - 0.0f + (2 * pi - lastAngle.z);
            float diff3 = lastAngle.z - 0.0f + (2 * pi - currentAngle.z);
            float diff = fminf(fminf(diff1, diff2), diff3);

            //log::trace("diff1:{}, diff2:{}, diff3:{}, diff:{}", diff1, diff2, diff3, diff);

            if (diff > 0.02f) {
                lastSuddenTurnFrame = iFrameCount;
                /*if (iFrameCount - lastNotification > 240) {
                    lastNotification = iFrameCount;
                    log::trace("Player is turning");
                }*/
            }

            lastAngle = currentAngle;
        }
    }

    void SetVelocity(float x, float y, float z) {
        velocity = RE::hkVector4(x, y, z, 0.0f);
    }

    void CancelFallNumber() {
         if (player->GetCharController()) {
            player->GetCharController()->fallStartHeight = 0.0f;
            player->GetCharController()->fallTime = 0.0f;
        }
    }

    // Deprecated: not working
    //void CancelFallFlag() {
    //    if (player->GetCharController()) {
    //        auto flag = player->GetCharController()->flags;

    //        log::trace("Reset kCastingDisabled");
    //        player->GetActorRuntimeData().boolFlags.reset(RE::Actor::BOOL_FLAGS::kCastingDisabled);
    //    }
    //}

    float UpdateWingDir(RE::NiPoint3 leftPos, RE::NiPoint3 rightPos) {
        auto& playerSt = PlayerState::GetSingleton();
        float conf_shoulderHeight = GetMyConf(fShoulderHeight) / 1.4;

        RE::NiPoint3 middlePos = RE::NiPoint3(0.0f, 0.0f, conf_shoulderHeight);
        log::trace("left hand pos: {}, {}, {}", leftPos.x, leftPos.y, leftPos.z);
        log::trace("right hand pos: {}, {}, {}", rightPos.x, rightPos.y, rightPos.z);

        // calculate the cross product
        RE::NiPoint3 vectorA = leftPos - middlePos;
        RE::NiPoint3 vectorB = rightPos - middlePos;

        RE::NiPoint3 newDir = vectorB.Cross(vectorA);  // this points to the sky
        if (newDir.Length() != 0.0f) {
            newDir = newDir / newDir.Length();
            if (newDir.z <= 0.2f) {
                // We don't allow wings to be facing downwards or to be close to vertical
                return newDir.z;
            } else {
                dirWings = newDir;
                return newDir.z;
            }
        } else {
            // Something is wrong, don't update wing dir
            return 0.0f;
        }
    }
};

