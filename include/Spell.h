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

    float GetIdleLifetime() { return GetMyConf(fSPVeloIdleLifetime);
    }

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

    float GetIdleLifetime() { return GetMyConf(fSPVeloIdleLifetime); }

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
    float maxZ = 6.3f;
    float maxZDouble = 3.0f; // When there is another EmitForceSpell, maxZ of this one is set to this
    float length = 9.5f;
    float maxXY = 2.0f;

    EmitForceSpell(bool isValid, bool isLeft) 
        : valid(isValid), isLeft(isLeft) {}

    EmitForceSpell() : valid(false) {}

    float GetIdleLifetime() { return GetMyConf(fSPForceIdleLifetime); }

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
            auto xy = sqrt(result.x * result.x + result.y * result.y);
            if (xy > maxXY) {
                result.x = result.x / xy * maxXY;
                result.y = result.y / xy * maxXY;
            }

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

    RE::NiPoint3 handPosL; // This is the hand pos used only by wings, not by others
    RE::NiPoint3 handPosR;

    
    std::chrono::steady_clock::time_point lastUpdate;

    WingSpell(bool isValid) : valid(isValid) {
        lastUpdate = lastUpdate = std::chrono::high_resolution_clock::now();
    }

    WingSpell() : valid(false) { lastUpdate = std::chrono::high_resolution_clock::now(); }

    float GetIdleLifetime() {
        return 1.0f; // as long as not 0, doesn't matter, because it's never idle unless player dispel our shout 
    }  

    Force CalculateflapForce(RE::NiPoint3& speedL, RE::NiPoint3& speedR) {
        auto& playerSt = PlayerState::GetSingleton();
        Force result = Force();
        float conf_flapStrength = GetMyConf(fFlapStrength);
        result = result - speedL * conf_flapStrength;
        result = result - speedR * conf_flapStrength;
        
        return result;
    }

    void UpdateHandPos() {
        auto& playerSt = PlayerState::GetSingleton();
        handPosL = GetPlayerHandPos(true, playerSt.player);
        handPosR = GetPlayerHandPos(false, playerSt.player);
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

// CalculateDragSimple assumes that Drag is always in the opposite direction of velocity
Force CalculateDragSimple(RE::NiPoint3& currentVelo);

// CalculateDragComplex calculates drag by the velocity and player's wings direction
Force CalculateDragComplex(RE::NiPoint3& currentVelo);


// CalculateLift calculates lift by the velocity and player's wings direction
Force CalculateLift(RE::NiPoint3& currentVelo);

Force CalculateVerticalHelper(RE::NiPoint3& currentVelo);

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

    Force gravityF = Force(0.0f, 0.0f, -8.0f);

    AllFlyEffects(std::size_t cap) : bufferV(cap), capacityV(cap), indexV(0), bufferF(cap), capacityF(cap), indexF(0) {
        lastUpdate = std::chrono::high_resolution_clock::now();
    }

    ~AllFlyEffects() { 
        Clear();
    }

    void Clear() { 
        for (std::size_t i = 0; i < capacityV; i++) {
            if (bufferV[i]) {
                delete bufferV[i];
                bufferV[i] = nullptr;
            }
        }
        indexV = 0;

        
        for (std::size_t i = 0; i < capacityF; i++) {
            if (bufferF[i]) {
                delete bufferF[i];
                bufferF[i] = nullptr;
            }
        }
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

    inline void ZeroAccumulated() {
        accumVelocity.x = 0.0f;
        accumVelocity.y = 0.0f;
        accumVelocity.z = 0.0f;
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
        auto& playerSt = PlayerState::GetSingleton();
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
        float conf_maxPassedTime = 0.05f; // 20fps
        if (passedTime > conf_maxPassedTime) passedTime = conf_maxPassedTime;

        float timeSlowSWRatio = CurrentSpellWheelSlowRatio(playerSt.player);
        // If slowing time, passed time should be reduced
        if (timeSlowSWRatio < 0.9f) passedTime *= timeSlowSWRatio;

        float timeSlowRatioMine = CurrentMyTimeSlowRatio(playerSt.player);
        // If slowing time, passed time should be reduced
        if (timeSlowRatioMine < 0.9f && timeSlowSWRatio > 0.9f) passedTime *= timeSlowRatioMine;

        bool noForceEffect = true;
        bool seenSpell = false;
        for (auto e : bufferF) {
            if (e) {
                noForceEffect = false;
                auto f = e->force;
                if (seenSpell && f.z > e->emitForceSpell.maxZDouble) f.z = e->emitForceSpell.maxZDouble;
                log::trace("Found a valid ForceEffect with Force: {}, {}, {}", f.x, f.y, f.z);
                accumVelocity = accumVelocity + f * passedTime;
                seenSpell = true;
            }
        }


        
        if (playerSt.player) {
            // If not on ground, and there is at least one Force, apply gravity and drag to player as force
            if (!noForceEffect && playerSt.player->IsInMidair()) {
                accumVelocity = accumVelocity + gravityF * passedTime;

                auto currentVelo = result.AsNiPoint3() + accumVelocity;
                log::trace("Current velo:{}, {}, {}", currentVelo.x, currentVelo.y, currentVelo.z);

                RE::NiPoint3 drag, lift;
                if (!playerSt.hasWings) {
                    // if player doesn't have wings, simply calculate drag as opposite to currentVelo
                    drag = CalculateDragSimple(currentVelo);
                } else {
                    // if player has wings, use wings direction to calculate drag
                    drag = CalculateDragComplex(currentVelo);
                    lift = CalculateLift(currentVelo);
                }
                if (playerSt.isSkyDiving == false || playerSt.hasWings == false) {
                    accumVelocity = accumVelocity + drag * passedTime + lift * passedTime;
                    log::trace("Applying gravityForce and Drag. Drag is simple:{}", playerSt.hasWings == false);
                    log::trace("Drag {}:{}, {}, {}", drag.Length(), drag.x, drag.y, drag.z);
                    log::trace("Lift {}:{}, {}, {}", lift.Length(), lift.x, lift.y, lift.z);
                } else {
                    accumVelocity = accumVelocity + drag * passedTime * 0.7f + lift * passedTime * 0.7f;
                    log::trace("Player is skyDiving. Drag before reduced {}:{}, {}, {}", drag.Length(), drag.x, drag.y, drag.z);

                    log::trace("Lift before reduced {}:{}, {}, {}", lift.Length(), lift.x, lift.y, lift.z);
                }

                // if player hasn't ever set wingDir since this flight, don't apply help force
                if (playerSt.everSetWingDirSinceThisFlight && playerSt.isSkyDiving == false && playerSt.hasWings) {
                    RE::NiPoint3 help = CalculateVerticalHelper(currentVelo);
                    accumVelocity = accumVelocity + help * passedTime;
                    log::trace("Vertical help force {}:{}, {}, {}", help.Length(), help.x, help.y, help.z);
                }

                // if player is in fire lift (upon fire), add strong lift
                if (playerSt.isInFireLift && currentVelo.z > -10.0f) {
                    float fireStr = 10.0f;
                    if (currentVelo.Length() > 10.0f) fireStr *= 1 + (currentVelo.Length() - 10.0f) / 20.0f;
                    accumVelocity = accumVelocity + RE::NiPoint3(0.0f, 0.0f, fireStr) * passedTime;
                    log::trace("Player is in fire lift");
                }

                // if player in spiritual lift (upon every living creature), add lift
                if (playerSt.isInSpiritualLift && currentVelo.z > -10.0f) {
                    float spiritualStr = 6.0f;
                    if (currentVelo.Length() > 10.0f) spiritualStr *= 1 + (currentVelo.Length() - 10.0f) / 20.0f;
                    accumVelocity = accumVelocity + RE::NiPoint3(0.0f, 0.0f, spiritualStr) * passedTime;
                    log::trace("Player is in spiritual lift");
                }
            }

            // If on ground, reduce accumVelocity greatly, so player don't slipper on ground
            if (!playerSt.player->IsInMidair()) {
                if (accumVelocity.Length() < 0.05f && accumVelocity.z <= 0.0f) {
                    // static friction
                    accumVelocity = accumVelocity * (0.85f);
                    log::trace("After static friction. accumVelocity:{}, {}, {}", accumVelocity.x, accumVelocity.y,
                               accumVelocity.z);
                } else if (accumVelocity.Length() < 0.5f && accumVelocity.z <= 0.0f) {
                    // strong dynamic friction
                    accumVelocity = accumVelocity * (1.0f - passedTime / 0.1f);
                    log::trace("After strong dynamic friction. accumVelocity:{}, {}, {}", accumVelocity.x, accumVelocity.y,
                               accumVelocity.z);
                } else {
                    // weak dynamic friction
                    accumVelocity = accumVelocity * (1.0f - passedTime / 0.38f);
                    log::trace("After dynamic friction. accumVelocity:{}, {}, {}", accumVelocity.x, accumVelocity.y,
                               accumVelocity.z);
                }
            }
        }

        log::trace("Accumulated velocity itself: {}, {}, {}", accumVelocity.x, accumVelocity.y,
                   accumVelocity.z);
        result = result + accumVelocity;
        static float maxVeloLength = 0.0f;
        if (result.Length() > maxVeloLength) {
            maxVeloLength = result.Length();
            log::trace("New highest Velo:{}", maxVeloLength);
        }
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

    bool IsEffectOnlyWings() {
        bool hasWings = false;
        bool hasOtherEffect = false;
        for (auto e : bufferF) {
            if (e) {
                if (e->emitForceSpell.valid) {
                    hasOtherEffect = true;
                    break;
                }
                if (e->wingSpell.valid) {
                    hasWings = true;
                }
            }
        }
        for (auto e : bufferV) {
            if (e) {
                hasOtherEffect = true;
                break;
            }
        }
        return hasWings && !hasOtherEffect;
    }

    ForceEffect* GetWingEff() {
        for (auto e : bufferF) {
            if (e) {
                if (e->wingSpell.valid) {
                    return e;
                }
            }
        }
        return nullptr;
    }

    // TODO: finish this
    void DeleteIdleEffects() {
        auto now = std::chrono::high_resolution_clock::now();
        for (auto e : bufferV) {
            if (e) {
                float passedTime =
                    ((float)std::chrono::duration_cast<std::chrono::milliseconds>(now - e->lastUpdate).count()) / 1000.0f;
                float conf_IdleDeadline(1.0f);
                if (e->emitSpell.valid) {
                    conf_IdleDeadline = e->emitSpell.GetIdleLifetime();
                } else if (e->xyzSpell.valid) {
                    conf_IdleDeadline = e->xyzSpell.GetIdleLifetime();
                }
                if (passedTime > conf_IdleDeadline) {
                    delete e->velo;
                    DeleteEffect(e);
                }
            }
        }
        for (auto e : bufferF) {
            if (e) {
                float conf_IdleDeadline(1.0f);
                if (e->emitForceSpell.valid) {
                    conf_IdleDeadline = e->emitForceSpell.GetIdleLifetime();
                } else if (e->wingSpell.valid) {
                    conf_IdleDeadline = e->wingSpell.GetIdleLifetime();
                }
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


void WingsCheckMain();

void PlayWingAnimation(RE::Actor* player, RE::NiPoint3 speedL, RE::NiPoint3 speedR);