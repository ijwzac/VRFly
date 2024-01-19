#include "Force.h"

using namespace SKSE;

// XYZMoveSpell can give player velo based on player's hand position
class XYZMoveSpell {
public:
    bool valid;
    RE::NiPoint3 oriPos;  // the original relative position of player's hand
    bool considerVertical;
    bool considerHorizontal;

    // parameters
    float modTime = 0.0f;
    float maxZ = 2.7f;
    float maxLength = 8.0f;

    XYZMoveSpell(RE::NiPoint3 oriPos, bool considerVertical, bool considerHorizontal)
        : oriPos(oriPos),
          considerVertical(considerVertical), considerHorizontal(considerHorizontal), valid(true) {}

    XYZMoveSpell()
        : oriPos(RE::NiPoint3()),
          considerVertical(false),
          considerHorizontal(false),
          valid(false) {}

    Velo CalculateNewStable(RE::NiPoint3 newPos) {
        float conf_verticalMult = 0.25f;
        float conf_horizontalMult = 0.25f;
        auto diff = newPos - oriPos;
        log::trace("newPos: {}, {}, {}", newPos.x, newPos.y, newPos.z);
        log::trace("oriPos: {}, {}, {}", oriPos.x, oriPos.y, oriPos.z);
        Velo result = Velo();
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

    // parameters
    float modTime = 0.0f;
    float maxZ = 1.7f;
    float length = 3.3f;

    EmitSpell(bool isValid, bool isLeft) // TODO: finish EmitSpell
        : valid(isValid), isLeft(isLeft) {}

    EmitSpell() : valid(false) {}

    Velo CalculateNewStable() {
        Velo result = Velo();

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

// EmitForceSpell can give player force based on player's hand angle
class EmitForceSpell {
public:
    bool valid;
    bool isLeft;

    // parameters
    float maxZ = 0.6f;
    float length = 1.2f;

    EmitForceSpell(bool isValid, bool isLeft) 
        : valid(isValid), isLeft(isLeft) {}

    EmitForceSpell() : valid(false) {}

    Force CalculateNewForce() {
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
            // rotation = adjustNodeRotation(baseNode, rotation, RE::NiPoint3(fMagicNum1, fMagicNum2, fMagicNum3),
            // false);
            rotation = adjustNodeRotation(baseNode, rotation, RE::NiPoint3(.0f, 1.0f, .0f), false);

            auto verticleVector = RE::NiPoint3{rotation.entry[0][1], rotation.entry[1][1], rotation.entry[2][1]};

            auto pointOutHand = verticleVector * 30.5f + handPos;

            if (bShowEmitSpellDirection) debug_show_weapon_range(player, handPos, pointOutHand, weaponNode);

            result = verticleVector * length;
            if (result.z > maxZ) result.z = maxZ;

        } else {
            log::warn("Fail to get player hand node ");
        }

        return result;
    }
};

// WingSpell can give player force based on player's hand movement. It also changes the behavior of drag and lift
class WingSpell {
public:
    bool valid;

    // parameters
    float conf_slapStrength = 4.0f; // TODO: this is a little high, while gravity is too low

    WingSpell(bool isValid) : valid(isValid) {}

    WingSpell() : valid(false) {}

    Force CalculateSlapForce(RE::NiPoint3& speedL, RE::NiPoint3& speedR) {
        auto& playerSt = PlayerState::GetSingleton();
        Force result = Force();

        result = result - speedL * conf_slapStrength;
        result = result - speedR * conf_slapStrength;
        
        return result;
    }
};

enum class Slot {
    kLeft = 0,
    kRight = 1 << 0,
    kHead = 1 << 1,
};

class ForceEffect {
public:

    Slot slot;
    Force force;
    std::chrono::steady_clock::time_point lastUpdate;

    RE::SpellItem* spell;

    // Only one below is valid
    EmitForceSpell emitForceSpell;
    WingSpell wingSpell;

    ForceEffect(Slot slot, Force force, RE::SpellItem* spell) : slot(slot), force(force), spell(spell) { 
        lastUpdate = std::chrono::high_resolution_clock::now();

    }

    void Update() {
        lastUpdate = std::chrono::high_resolution_clock::now();
    }
};

// VeloEffect is created by a spell or weapon, and it adds a Velo on player. 
// It's responsible for the update of the Velo, but whoever created VeloEffect should be responsible for the deletion of Velo
class VeloEffect {
public:

    Slot slot;
    TrapezoidVelo* velo;
    std::chrono::steady_clock::time_point lastUpdate;

    // Only one is not null among the following 2 fields
    RE::SpellItem* spell;
    RE::TESObjectWEAP* weapon;

    // If spell is not null, only one of the following 2 fields below is valid
    XYZMoveSpell xyzSpell;
    EmitSpell emitSpell;

    VeloEffect(Slot slot, TrapezoidVelo* velo, RE::SpellItem* spell, RE::TESObjectWEAP* weapon)
        : slot(slot), velo(velo), spell(spell), weapon(weapon), xyzSpell(XYZMoveSpell()), emitSpell(EmitSpell()) {
        lastUpdate = std::chrono::high_resolution_clock::now();
    }

    void Update() { 
        lastUpdate = std::chrono::high_resolution_clock::now();
        velo->Update(lastUpdate);
    }
};

// CalculateDragSimple assumes that Drag is always in the opposite direction of velocity, 
Force CalculateDragSimple(RE::NiPoint3& currentVelo);

class AllFlyEffects {
public:
    std::vector<VeloEffect*> bufferV;
    std::size_t capacityV;
    std::size_t indexV;

    std::vector<ForceEffect*> bufferF;
    std::size_t capacityF;
    std::size_t indexF;
    RE::NiPoint3 accumVelocity;

    std::chrono::steady_clock::time_point lastUpdate;

    Velo gravityV = Velo(0.0f, 0.0f, -1.9f);

    Force gravityF = Force(0.0f, 0.0f, -0.7f);

    AllFlyEffects(std::size_t cap) : bufferV(cap), capacityV(cap), indexV(0), bufferF(cap), capacityF(cap), indexF(0) {
        lastUpdate = std::chrono::high_resolution_clock::now();
    }

    ~AllFlyEffects() { 
        Clear();
    }

    void Clear() { 
        for (auto e : bufferV) {
            if (e) delete e;
        }
        bufferV.clear();
        indexV = 0;

        
        for (auto e : bufferF) {
            if (e) delete e;
        }
        bufferF.clear();
        indexF = 0;

        accumVelocity = RE::NiPoint3();
    }

    void Push(VeloEffect* e) {
        bufferV[indexV] = e;
        indexV = (indexV + 1) % capacityV;
    }

    
    void Push(ForceEffect* e) {
        bufferF[indexF] = e;
        indexF = (indexF + 1) % capacityF;
    }

    VeloEffect* FindVeloEffect(Slot slot, RE::SpellItem* spell, RE::TESObjectWEAP* weapon) {
        for (auto e : bufferV) {
            if (e) {
                if (e->slot == slot && e->spell == spell && e->weapon == weapon) {
                    return e;
                }
            }
        }
        return nullptr;
    }

    ForceEffect* FindForceEffect(Slot slot, RE::SpellItem* spell) {
        for (auto e : bufferF) {
            if (e) {
                if (e->spell == spell && e->slot == slot) {
                    return e;
                }
            }
        }
        return nullptr;
    }

    void DeleteEffect(VeloEffect* e) { 
        for (std::size_t i = 0; i < capacityV; i++) {
            if (bufferV[i] == e) {
                delete e;
                bufferV[i] = nullptr;
                break;
            }
        }
    }

    void DeleteEffect(ForceEffect* e) {
        for (std::size_t i = 0; i < capacityF; i++) {
            if (bufferF[i] == e) {
                delete e;
                bufferF[i] = nullptr;
                break;
            }
        }
    }

    // Note that we don't update all effects in bufferV, so we can know which effect hasn't been updated and should be deleted

    Velo SumCurrentVelo() {
        Velo result = Velo();

        // Calculate all VeloEffect
        bool noVeloEffect = true;
        for (auto e : bufferV) {
            if (e) {
                noVeloEffect = false;
                auto f = e->velo->current;
                log::trace("Found a valid VeloEffect with current Velo: {}, {}, {}", f.x, f.y, f.z );
                result = result + e->velo->current;
            }
        }
        if (!noVeloEffect && IsForceEffectEmpty()) {
            result = result + gravityV;
            log::trace("Applying gravityVelo");
        }

        // Calculate all ForceEffect and add the accumulated velocity over the duration since last update
        auto now = std::chrono::high_resolution_clock::now();
        float passedTime =
            ((float)std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count()) / 1000.0f;
        bool noForceEffect = true;
        for (auto e : bufferF) {
            if (e) {
                noForceEffect = false;
                auto f = e->force;
                log::trace("Found a valid ForceEffect with Force: {}, {}, {}", f.x, f.y, f.z);
                accumVelocity = accumVelocity + f * passedTime;
            }
        }


        
        auto playerSt = PlayerState::GetSingleton();
        if (playerSt.player) {
            // If not on ground, and there is at least one Force, apply gravity and drag to player as force
            if (!noForceEffect && playerSt.player->IsInMidair()) {
                accumVelocity = accumVelocity + gravityF * passedTime;

                auto currentVelo = result.AsNiPoint3() + accumVelocity;
                auto dragSimple = CalculateDragSimple(currentVelo);
                log::trace("Current velo:{}, {}, {}", currentVelo.x, currentVelo.y, currentVelo.z);
                accumVelocity = accumVelocity + dragSimple * passedTime;
                log::trace("Applying gravityForce, Drag:{}, {}, {}", dragSimple.x, dragSimple.y, dragSimple.z);
            }

            // If on ground, reduce accumVelocity greatly, so player don't slipper on ground
            if (!playerSt.player->IsInMidair()) {
                if (accumVelocity.Length() < 0.05f && accumVelocity.z <= 0.0f) {
                    // static friction
                    accumVelocity = RE::NiPoint3(0.0f, 0.0f, 0.0f);
                } else if (accumVelocity.Length() < 0.5f && accumVelocity.z <= 0.0f) {
                    // static friction
                    accumVelocity = accumVelocity * (1.0f - passedTime / 0.05f);
                } else {
                    // friction
                    accumVelocity = accumVelocity * (1.0f - passedTime / 0.5f);
                }
            }
        }

        log::trace("Accumulated velocity itself: {}, {}, {}", accumVelocity.x, accumVelocity.y,
                   accumVelocity.z);
        result = result + accumVelocity;
        lastUpdate = now;
        return result;
    }

    bool IsVeloEffectEmpty() {
        for (auto e : bufferV) {
            if (e) {
                return false;
            }
        }
        return true;
    }

    bool IsForceEffectEmpty() {
        for (auto e : bufferF) {
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
        for (auto e : bufferV) {
            if (e) {
                float passedTime =
                    ((float)std::chrono::duration_cast<std::chrono::milliseconds>(now - e->lastUpdate).count()) / 1000.0f;
                if (passedTime > conf_IdleDeadline) {
                    delete e->velo;
                    DeleteEffect(e);
                }
            }
        }
        for (auto e : bufferF) {
            if (e) {
                float passedTime =
                    ((float)std::chrono::duration_cast<std::chrono::milliseconds>(now - e->lastUpdate).count()) /
                    1000.0f;
                if (passedTime > conf_IdleDeadline) {
                    DeleteEffect(e);
                }
            }
        }
    }
};

extern AllFlyEffects allEffects;

void SpellCheckMain();

