#include <chrono>

using namespace SKSE;

// Force is the basic data struct for speed control. Only contains 3 floats
class Force {
public:
    float x;
    float y;
    float z;
    Force(float x, float y, float z) : x(x), y(y), z(z) {}
    Force() : x(0), y(0), z(0) {}

    Force operator+(const Force& v1) {
        Force result;
        result.x = this->x + v1.x;
        result.y = this->y + v1.y;
        result.z = this->z + v1.z;
        return result;
    };

    Force operator-(const Force& v1) {
        Force result;
        result.x = this->x - v1.x;
        result.y = this->y - v1.y;
        result.z = this->z - v1.z;
        return result;
    };

    Force operator*(const float v1) {
        Force result;
        result.x = this->x * v1;
        result.y = this->y * v1;
        result.z = this->z * v1;
        return result;
    };

    Force operator/(const float v1) {
        Force result;
        result.x = this->x / v1;
        result.y = this->y / v1;
        result.z = this->z / v1;
        return result;
    };

    // Deprecated: probably not necessary, and also this doesn't work if the new stable has different direction
    // CapForce makes sure self doesn't exceed cap. Requires cap to be at the same direction as self
    void CapForce(Force cap) {
        if (x > 0 && cap.x < x) x = cap.x;
        if (x < 0 && cap.x > x) x = cap.x;
        if (y > 0 && cap.y < y) y = cap.y;
        if (y < 0 && cap.y > y) y = cap.y;
        if (z > 0 && cap.z < z) z = cap.z;
        if (z < 0 && cap.z > z) z = cap.z;
    }

    // PreventReverse makes sure each of self's x,y,z doesn't reverse direction
    void PreventReverse(Force direction) {
        if (x > 0 && direction.x < 0) x = 0;
        if (x < 0 && direction.x > 0) x = 0;
        if (y > 0 && direction.y < 0) y = 0;
        if (y < 0 && direction.y > 0) y = 0;
        if (z > 0 && direction.z < 0) z = 0;
        if (z < 0 && direction.z > 0) z = 0;
    }
};

// TrapezoidForce simulates a natural force. After created, it starts from zero and linearly increase to stable
// If its stable force is modified, it will modify its current force gradually and linearly to reach stable
// We expect that Decrease stage only happens once, and Mod stage or Stable stage can happen multiple times. See Update()
class TrapezoidForce {
public:

    enum class ForceStage {
        kIncrease = 0
    };

    // parameters
    Force stable;
    float durDecrease;

    // states
    Force current;
    float remainingModDur; // only one of remainingModDur and remainingDecreaseDur can be positive
    float remainingDecreaseDur;
    bool startedDecrease;
    std::chrono::steady_clock::time_point lastUpdate;

    // The default constructor starts from zero and linearly increase to stable
    TrapezoidForce(float x, float y, float z, float durIncrease, float durDecrease = 0)
        : current(Force()), stable(Force(x, y, z)), durDecrease(durDecrease) {
        remainingModDur = durIncrease;
        lastUpdate = std::chrono::high_resolution_clock::now();
    }

    TrapezoidForce(float x, float y, float z) : current(Force(x, y, z)), stable(Force(x, y, z)), durDecrease(0) {
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
            current = Force();
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
            Force speedIncrease = (stable - current) / remainingModDur;
            if (passedTime < remainingModDur) {
                remainingModDur -= passedTime;
            } else {
                passedTime = remainingModDur;
                remainingModDur = 0;
            }
            current = current + speedIncrease * passedTime;
            //current.CapForce(stable);
        }

        if (remainingDecreaseDur > 0 && startedDecrease) {
            // Decrease stage
            Force speedDecrease = (current) / remainingDecreaseDur;
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
        stable = Force(x, y, z);
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

    // IsDecreaseComplete returns true when this Force was commanded to decrease and is now almost zero, and not interrupted by ModStable
    bool IsDecreaseComplete() {
        if (startedDecrease && abs(current.x) < 0.3f && abs(current.y) < 0.3f && abs(current.z) < 0.3f) {
            return true;
        } else {
            return false;
        }
    }
};

class AllForce {
public:
    //TODO: replace with a vector
    TrapezoidForce* allCurrent;


    AllForce() : allCurrent(nullptr) {}

    void UpdateAll() { 
        auto now = std::chrono::high_resolution_clock::now();

        allCurrent->Update(now);
    }

    Force SumAll() { return allCurrent->current;
    }

};