#include "Utils.h"
#include "Settings.h"

using namespace SKSE;

class SpeedRing {
public:
    std::vector<RE::NiPoint3> bufferL;
    std::vector<RE::NiPoint3> bufferR;
    std::size_t capacity;  // how many latest frames are stored
    std::size_t indexCurrentL;
    std::size_t indexCurrentR;
    SpeedRing(std::size_t cap) : bufferL(cap), bufferR(cap), capacity(cap), indexCurrentR(0), indexCurrentL(0) {}

    void Clear() {}

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
            // Return zero velocity or handle error
            return RE::NiPoint3(0.0f, 0.0f, 0.0f);
        }

        std::size_t currentIdx = isLeft ? indexCurrentL : indexCurrentR;
        const std::vector<RE::NiPoint3>& buffer = isLeft ? bufferL : bufferR;

        // Get the start and end positions
        RE::NiPoint3 startPos = buffer[(currentIdx - N + capacity) % capacity];
        RE::NiPoint3 endPos = buffer[(currentIdx - 1 + capacity) % capacity];

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

    bool isInMidAir = false;
    

    PlayerState() : player(nullptr), setVelocity(false), speedBuf(SpeedRing(100)) {}

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

    void SetVelocity(float x, float y, float z) {
        velocity = RE::hkVector4(x, y, z, 0.0f);
        setVelocity = true;

        //if (player->GetCharController()) {
        //    log::trace("About to set player velocity to: {}, {}, {}", x, y, z);
        //    player->GetCharController()->SetLinearVelocityImpl(velocity);
        //}
    }

    void StopSetVelocity() { setVelocity = false;
    }

    void CancelFall() {
         if (player->GetCharController()) {
            player->GetCharController()->fallStartHeight = 0.0f;
            player->GetCharController()->fallTime = 0.0f;
        }
    }
};

void WingsCheckMain();