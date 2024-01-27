#include <chrono>
#include "Player.h"

using namespace SKSE;

class Force : public RE::NiPoint3 {
public:
    Force(const RE::NiPoint3& point) : RE::NiPoint3(point) {}
    Force(float x, float y, float z) : RE::NiPoint3(x, y, z) {}
    Force() {}
};

// Velo is the basic data struct for speed control. Only contains 3 floats
class Velo {
public:
    float x;
    float y;
    float z;
    Velo(float x, float y, float z) : x(x), y(y), z(z) {}
    Velo() : x(0), y(0), z(0) {}

    Velo operator+(const Velo& v1) {
        Velo result;
        result.x = this->x + v1.x;
        result.y = this->y + v1.y;
        result.z = this->z + v1.z;
        return result;
    };

    Velo operator+(const RE::NiPoint3& v1) {
        Velo result;
        result.x = this->x + v1.x;
        result.y = this->y + v1.y;
        result.z = this->z + v1.z;
        return result;
    };

    Velo operator-(const Velo& v1) {
        Velo result;
        result.x = this->x - v1.x;
        result.y = this->y - v1.y;
        result.z = this->z - v1.z;
        return result;
    };

    Velo operator*(const float v1) {
        Velo result;
        result.x = this->x * v1;
        result.y = this->y * v1;
        result.z = this->z * v1;
        return result;
    };

    Velo operator/(const float v1) {
        Velo result;
        result.x = this->x / v1;
        result.y = this->y / v1;
        result.z = this->z / v1;
        return result;
    };

    // Deprecated: probably not necessary, and also this doesn't work if the new stable has different direction
    // CapVelo makes sure self doesn't exceed cap. Requires cap to be at the same direction as self
    void CapVelo(Velo cap) {
        if (x > 0 && cap.x < x) x = cap.x;
        if (x < 0 && cap.x > x) x = cap.x;
        if (y > 0 && cap.y < y) y = cap.y;
        if (y < 0 && cap.y > y) y = cap.y;
        if (z > 0 && cap.z < z) z = cap.z;
        if (z < 0 && cap.z > z) z = cap.z;
    }

    // PreventReverse makes sure each of self's x,y,z doesn't reverse direction
    void PreventReverse(Velo direction) {
        if (x > 0 && direction.x < 0) x = 0;
        if (x < 0 && direction.x > 0) x = 0;
        if (y > 0 && direction.y < 0) y = 0;
        if (y < 0 && direction.y > 0) y = 0;
        if (z > 0 && direction.z < 0) z = 0;
        if (z < 0 && direction.z > 0) z = 0;
    }

    float Length() { return sqrt(x * x + y * y + z * z);
    }

    RE::NiPoint3 AsNiPoint3() { return RE::NiPoint3(x, y, z);
    }
};

// TrapezoidVelo simulates natural change of velocity that doesn't rely on Force. After created, it starts from zero and linearly increase to stable
// If its stable velo is modified, it will modify its current velo gradually and linearly to reach stable
// We expect that Decrease stage only happens once, and Mod stage or Stable stage can happen multiple times. See Update()
class TrapezoidVelo {
public:

    enum class VeloStage {
        kIncrease = 0
    };

    // parameters
    Velo stable;
    float durDecrease;

    // states
    Velo current;
    float remainingModDur; // only one of remainingModDur and remainingDecreaseDur can be positive
    float remainingDecreaseDur;
    bool startedDecrease;
    std::chrono::steady_clock::time_point lastUpdate;

    // The default constructor starts from zero and linearly increase to stable
    TrapezoidVelo(float x, float y, float z, float durIncrease, float durDecrease = 0)
        : current(Velo()), stable(Velo(x, y, z)), durDecrease(durDecrease) {
        remainingModDur = durIncrease;
        lastUpdate = std::chrono::high_resolution_clock::now();
    }

    TrapezoidVelo(float x, float y, float z) : current(Velo(x, y, z)), stable(Velo(x, y, z)), durDecrease(0) {
        lastUpdate = std::chrono::high_resolution_clock::now();
    }

    // Update that only considers the internal linear modification/decrease stage
    void Update(std::chrono::steady_clock::time_point now) {
        if (remainingModDur < 0.001f && startedDecrease == false) {
            // Stable stage
            current = stable;
            return;
        }
        if (remainingDecreaseDur < 0.001f && startedDecrease) {
            // Decrease stage reaches its end
            current = Velo();
            return;
        }
        float passedTime =
            ((float) std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count()) / 1000.0f;
        if (passedTime > 0.1f) {
            // player's frame is not stable (below 10fps), or player has stopped the game
            // Thus, we need to make the passedTime very small
            passedTime = 0.01f;
        }
        log::trace("PassedTime:{}, remainingMoDur:{}, remainingDecreaseDur:{}", passedTime, remainingModDur,
                   remainingDecreaseDur);
        if (remainingModDur > 0) {
            // Mod stage
            Velo speedIncrease = (stable - current) / remainingModDur;
            if (passedTime < remainingModDur) {
                remainingModDur -= passedTime;
            } else {
                passedTime = remainingModDur;
                remainingModDur = 0;
            }
            current = current + speedIncrease * passedTime;
            //current.CapVelo(stable);
        }

        if (remainingDecreaseDur > 0 && startedDecrease) {
            // Decrease stage
            Velo speedDecrease = (current) / remainingDecreaseDur;
            if (passedTime < remainingDecreaseDur) {
                remainingDecreaseDur -= passedTime;
            } else {
                passedTime = remainingDecreaseDur;
                remainingDecreaseDur = 0;
            }
            current = current - speedDecrease * passedTime;
            current.PreventReverse(stable);
        }

        lastUpdate = now;
    }

    // ModStable modifies stable and makes current gradually reach stable. It also stops decreasing stage.
    void ModStable(float x, float y, float z, float modDur) { 
        stable = Velo(x, y, z);
        remainingModDur = modDur;
        remainingDecreaseDur = 0.0f;
        startedDecrease = false;
    }

    // StartDecrease starts the linear decrease stage. Can optionaly change the duration
    void StartDecrease(bool useDefault = true, float newDecreaseDur = 0.0f) {
        if (useDefault == false) {
            remainingDecreaseDur = newDecreaseDur;
        } else {
            remainingDecreaseDur = durDecrease;
        }
        startedDecrease = true;
    }

    // IsDecreaseComplete returns true when this Velo was commanded to decrease and is now almost zero, and not interrupted by ModStable
    bool IsDecreaseComplete() {
        if (startedDecrease && abs(current.x) < 0.3f && abs(current.y) < 0.3f && abs(current.z) < 0.3f) {
            return true;
        } else {
            return false;
        }
    }
};

void VeloEffectMain(Velo& vel);


class WindObj {
public:
    enum WindType { 
        mid,
        lg,
        ex
    };
    RE::NiPointer<RE::TESObjectREFR> obj;
    WindType type;
    int64_t frameCreate;
    bool setAngle = false;

    WindObj(RE::NiPointer<RE::TESObjectREFR> obj, WindType type, int64_t frame) : obj(obj), type(type), frameCreate(frame) {}
    WindObj() : obj(nullptr), frameCreate(0) {}

    void Delete() { 
        if (obj) {
            auto ref = obj.get();
            if (ref) {
                ref->Disable(); // Our papyrus script will delete it after disable
            }
        }
    }
};

class WindManager {
public:
    std::vector<WindObj> buffer;
    std::size_t capacity;
    std::size_t index;

    // Wind Manager also creates the rock explosion shortly after landing
    int64_t frameLastRockExplo;

    WindManager(std::size_t cap) : buffer(cap), capacity(cap), index(0) {}

    
    static WindManager& GetSingleton() {
        static WindManager singleton(20);
        return singleton;
    }

    void Push(WindObj& obj) {
        if (buffer[index].obj) {
            log::trace("Deleting a wind");
            buffer[index].Delete();
        }
        buffer[index] = obj;
        index = (index + 1) % capacity;
    }

    void Clear() {
        auto& playerSt = PlayerState::GetSingleton();
        auto shockWaveSpell = GetShockWaveSpell();
        for (std::size_t i = 0; i < capacity; i++) {
            if (buffer[i].obj) {
                log::trace("Deleting a wind");
                buffer[i].Delete();
                buffer[i].obj = nullptr;
                buffer[i].frameCreate = 0;
            }
        }
        index = 0;

        if (shockWaveSpell) {
            if (playerSt.player->HasSpell(shockWaveSpell)) playerSt.player->RemoveSpell(shockWaveSpell);
        }
        frameLastRockExplo = 0;
    }

    // Update should be called every frame, to delete old wind
    void Update() {
        auto& playerSt = PlayerState::GetSingleton();
        auto shockWaveSpell = GetShockWaveSpell();

        // Update rock explosion
        RE::NiPointer<RE::TESObjectREFR> rock;
        if (iFrameCount - frameLastRockExplo == 15 && frameLastRockExplo != 0) {
            auto rockExplo = GetExploRock();
            rock = playerSt.player->PlaceObjectAtMe(rockExplo, false);
        }

        // Remove player's cloak spell
        if (iFrameCount - frameLastRockExplo >= 150 && frameLastRockExplo != 0) {
            frameLastRockExplo = 0;
            if (playerSt.player->HasSpell(shockWaveSpell)) playerSt.player->RemoveSpell(shockWaveSpell);
        }



        // Update Wind
        for (std::size_t i = 0; i < capacity; i++) {
            if (buffer[i].obj) {
                auto windRef = buffer[i].obj.get();
                if (!windRef) continue;

                // If a wind lives long enough, or player on ground, or player lose sight of the wind and far enough
                // we should delete the wind
                // This doesn't need to run every frame
                if ((iFrameCount + i) % 5 == 0) {
                    if (iFrameCount - buffer[i].frameCreate > 800 || playerSt.isInMidAir == false) {
                        log::trace("Deleting a wind");
                        buffer[i].Delete();
                        buffer[i].obj = nullptr;
                        buffer[i].frameCreate = 0;
                        continue;
                    } 
                }
                
                
                if (windRef->Is3DLoaded() && buffer[i].setAngle == false) {
                    log::trace("Moving a just loaded wind. Ori pos: {}, {}, {}", windRef->GetPositionX(),
                                windRef->GetPositionY(), windRef->GetPositionZ());
                    float windAdvancePlayerTime;
                    windRef->data.angle = playerSt.player->GetAngle() * 1.0f;
                    auto newPos = playerSt.player->GetPosition();
                    switch (buffer[i].type) { 
                    case WindObj::mid: {
                            windAdvancePlayerTime = 60.0f + 30.0f * (float)(rand()) / (float)(RAND_MAX);
                            break;
                    }
                    case WindObj::lg:
                    case WindObj::ex: {
                            windAdvancePlayerTime = 20.0f + 5.0f * (float)(rand()) / (float)(RAND_MAX);
                            newPos.z += 100.0f;
                            //log::trace("lg or ex wind angle: {}, {}, {}", windRef->data.angle.x,
                            //           windRef->data.angle.y, windRef->data.angle.z);
                            break;
                    }
                                    
                    }
                    newPos += Quad2Velo(playerSt.velocity) * windAdvancePlayerTime;
                    windRef->SetPosition(newPos);
                    log::trace("Setting it to new pos: {}, {}, {}", newPos.x, newPos.y, newPos.z);
                    buffer[i].setAngle = true;
                                
                }
                
                
            }
        }
    }
};
