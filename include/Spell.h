#include "Force.h"

using namespace SKSE;

// XYZMoveSpell can give player force based on player's hand position
class XYZMoveSpell {
public:
    bool valid;
    RE::NiPoint3 oriPos;  // the original relative position of player's hand
    bool considerVertical;
    bool considerHorizontal;

    float maxZ = 1.8f;
    float maxLength = 8.0f;

    XYZMoveSpell(RE::NiPoint3 oriPos, bool considerVertical, bool considerHorizontal)
        : oriPos(oriPos),
          considerVertical(considerVertical), considerHorizontal(considerHorizontal), valid(true) {}

    XYZMoveSpell()
        : oriPos(RE::NiPoint3()),
          considerVertical(false),
          considerHorizontal(false),
          valid(false) {}

    Force CalculateNewStable(RE::NiPoint3 newPos) {
        float conf_verticalMult = 0.25f;
        float conf_horizontalMult = 0.25f;
        auto diff = newPos - oriPos;
        log::trace("newPos: {}, {}, {}", newPos.x, newPos.y, newPos.z);
        log::trace("oriPos: {}, {}, {}", oriPos.x, oriPos.y, oriPos.z);
        Force result = Force();
        if (considerVertical) {
            result.z = diff.z * conf_verticalMult;
            if (result.z > maxZ) result.z = maxZ;
        }
        if (considerHorizontal) {
            result.x = diff.x * conf_horizontalMult;
            result.y = diff.y * conf_horizontalMult;
        }

        float length = result.Length();
        if (length > maxLength) {
            result.x *= maxLength / length;
            result.y *= maxLength / length;
            result.z *= maxLength / length;
        }

        return result;
    }
};

// EmitSpell can give player velocity based on player's hand angle
class EmitSpell {
public:
    bool valid;
    bool isLeft;
    float maxZ = 1.7f;
    float length = 3.3f;

    EmitSpell(bool isValid, bool isLeft) // TODO: finish EmitSpell
        : valid(isValid), isLeft(isLeft) {}

    EmitSpell() : valid(false) {}

    Force CalculateNewStable() {
        Force result = Force();

        auto& playerSt = PlayerState::GetSingleton();
        auto player = playerSt.player;

        const auto actorRoot = netimmerse_cast<RE::BSFadeNode*>(player->Get3D());
        if (!actorRoot) {
            log::warn("Fail to get actorRoot:{}", player->GetBaseObject()->GetName());
            return result;
        }

        const auto nodeName = isLeft ? "NPC L Hand [LHnd]"sv : "NPC R Hand [RHnd]"sv;
        auto weaponNode = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeName));

        const auto nodeBaseStr = "NPC Pelvis [Pelv]"sv;  // base of player
        const auto baseNode = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeBaseStr));

        if (weaponNode) {
            auto handPos = weaponNode->world.translate;

            auto rotation = weaponNode->world.rotate;
            //rotation = adjustNodeRotation(baseNode, rotation, RE::NiPoint3(fMagicNum1, fMagicNum2, fMagicNum3), false);
            rotation = adjustNodeRotation(baseNode, rotation, RE::NiPoint3(.0f, 1.0f, .0f), false);

            auto verticleVector = RE::NiPoint3{rotation.entry[0][1], rotation.entry[1][1], rotation.entry[2][1]};

            auto pointOutHand = verticleVector * 30.5f + handPos;

            if (bShowEmitSpellDirection) debug_show_weapon_range(player, handPos, pointOutHand, weaponNode);

            result.x = verticleVector.x * length;
            result.y = verticleVector.y * length;
            result.z = verticleVector.z * length;
            if (result.z > maxZ) result.z = maxZ;

        } else {
            log::warn("Fail to get player hand node ");
        }



        return result;
    }
};

class ActiveFlyEffect {
public:
    enum class Slot {
        kLeft = 0,
        kRight = 1<< 0,
        kGrip = 1<< 1,
    };

    Slot slot;
    TrapezoidForce* force;
    std::chrono::steady_clock::time_point lastUpdate;

    // Only one is filled among the following 2 fields
    RE::SpellItem* spell;
    XYZMoveSpell xyzSpell;
    EmitSpell emitSpell;
    RE::TESObjectWEAP* weapon;

    ActiveFlyEffect(Slot slot, TrapezoidForce* force, RE::SpellItem* spell, RE::TESObjectWEAP* weapon)
        : slot(slot), force(force), spell(spell), weapon(weapon), xyzSpell(XYZMoveSpell()), emitSpell(EmitSpell()) {
        lastUpdate = std::chrono::high_resolution_clock::now();
    }

    void Update() { 
        lastUpdate = std::chrono::high_resolution_clock::now();
        force->Update(lastUpdate);
    }
};

class AllActiveFlyEffects {
public:
    std::vector<ActiveFlyEffect*> buffer;
    std::size_t capacity;
    std::size_t index;

    Force gravity = Force(0.0f, 0.0f, -1.9f);

    AllActiveFlyEffects(std::size_t cap) : buffer(cap), capacity(cap), index(0) {}

    ~AllActiveFlyEffects() { 
        Clear();
    }

    void Clear() { 
        for (auto e : buffer) {
            if (e) delete e;
        }
        buffer.clear();
        index = 0;
    }

    void Push(ActiveFlyEffect* e) {
        buffer[index] = e;
        index = (index + 1) % capacity;
    }

    ActiveFlyEffect* FindEffect(ActiveFlyEffect::Slot slot, RE::SpellItem* spell, RE::TESObjectWEAP* weapon) {
        for (auto e : buffer) {
            if (e) {
                if (e->slot == slot && e->spell == spell && e->weapon == weapon) {
                    return e;
                }
            }
        }
        return nullptr;
    }

    void DeleteEffect(ActiveFlyEffect* e) { 
        for (std::size_t i = 0; i < capacity; i++) {
            if (buffer[i] == e) {
                delete e;
                buffer[i] = nullptr;
                break;
            }
        }
    }

    // Note that we don't update all effects in buffer, so we can know which effect hasn't been updated and should be deleted

    Force SumCurrentForce() { 
        Force result = Force();
        log::trace("In SumCurrentForce");
        for (auto e : buffer) {
            if (e) {
                auto f = e->force->current;
                log::trace("Found a valid e with current: {}, {}, {}", f.x, f.y, f.z );
                result = result + e->force->current;
            }
        }
        result = result + gravity;
        return result;
    }

    bool IsEmpty() {
        for (auto e : buffer) {
            if (e) {
                return false;
            }
        }
        return true;
    }

    // TODO: finish this
    void DeleteIdleEffects() {
        auto now = std::chrono::high_resolution_clock::now();
        float conf_IdleDeadline = 0.5f;
        for (auto e : buffer) {
            if (e) {
                float passedTime =
                    ((float)std::chrono::duration_cast<std::chrono::milliseconds>(now - e->lastUpdate).count()) / 1000.0f;
                if (passedTime > conf_IdleDeadline) {
                    delete e->force;
                    DeleteEffect(e);
                }
            }
        }
    }
};

extern AllActiveFlyEffects allEffects;

void SpellCheckMain();

