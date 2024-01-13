#include "Force.h"

using namespace SKSE;

// XYZMoveSpell can give player force based on player's hand position
class XYZMoveSpell {
public:
    bool valid;
    RE::NiPoint3 oriPos;  // the original relative position of player's hand
    bool considerVertical;
    bool considerHorizontal;

    XYZMoveSpell(RE::NiPoint3 oriPos, bool considerVertical, bool considerHorizontal)
        : oriPos(oriPos),
          considerVertical(considerVertical), considerHorizontal(considerHorizontal), valid(true) {}

    XYZMoveSpell() : oriPos(RE::NiPoint3()), valid(false) {}

    Force CalculateNewStable(RE::NiPoint3 newPos) {
        float conf_verticalMult = 0.4f;
        float conf_horizontalMult = 0.4f;
        auto diff = newPos - oriPos;
        log::trace("newPos: {}, {}, {}", newPos.x, newPos.y, newPos.z);
        log::trace("oriPos: {}, {}, {}", oriPos.x, oriPos.y, oriPos.z);
        Force result = Force();
        if (considerVertical) {
            result.z = diff.z * conf_verticalMult;
        }
        if (considerHorizontal) {
            result.x = diff.x * conf_horizontalMult;
            result.y = diff.y * conf_horizontalMult;
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
    RE::TESObjectWEAP* weapon;

    ActiveFlyEffect(Slot slot, TrapezoidForce* force, RE::SpellItem* spell, RE::TESObjectWEAP* weapon)
        : slot(slot), force(force), spell(spell), weapon(weapon) {
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

