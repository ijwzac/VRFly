#include "Utils.h"
#include "Settings.h"

using namespace SKSE;

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

    bool isInMidAir = false; // Updated by HookSetVelocity, not OnFrame
    bool shouldCheckKnock = false;  // Set true in HookSetVelocity, set false in OnFrame
    bool isSlappingWings = false;
    bool isEffectOnlyWings = false; // if wings is the one and only effect in allEffects
    bool hasWings = false;

    int64_t lastJumpFrame = 0;
    int64_t frameLastSlap = 0;
    int64_t lastSoundFrame = 0;
    int64_t lastOngroundFrame = 0;

    RE::NiPoint3 dirWings = RE::NiPoint3(0.0f, 0.0f, 1.0f);  // A normalized pointer that is vertical to wings. Initialized to be pointing to the sky


    PlayerState() : player(nullptr), setVelocity(false), speedBuf(SpeedRing(100)), recentVelo() {}

    void Clear() { 
        setVelocity = false;
        velocity = RE::NiPoint3();
        isInMidAir = false;
        isSlappingWings = false;
        isEffectOnlyWings = false;
        hasWings = false;
        lastJumpFrame = 0;
        frameLastSlap = 0;
        lastSoundFrame = 0;
        lastOngroundFrame = 0;

        speedBuf.Clear();
        recentVelo.clear();
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

    void SetVelocity(float x, float y, float z) {
        velocity = RE::hkVector4(x, y, z, 0.0f);
    }

    void CancelFall() {
         if (player->GetCharController()) {
            player->GetCharController()->fallStartHeight = 0.0f;
            player->GetCharController()->fallTime = 0.0f;
        }
    }

    void UpdateWingDir(RE::NiPoint3 leftPos, RE::NiPoint3 rightPos) {
        auto& playerSt = PlayerState::GetSingleton();
        float conf_shoulderHeight = 100.0f;

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
                return;
            } else {
                dirWings = newDir;
                return;
            }
        } else {
            // Something is wrong, don't update wing dir
            return;
        }
    }
};