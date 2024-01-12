// We hook into the Main Update() function, which fires every frame
//
// On each frame, get the position and rotation of player's weapon, and nearby enemy actors' weapons
// Then detect if player's weapon collides with enemy's weapon and check some other conditions,
//		call collision functions, and then update some data struct
//
// The data here will be used by OnMeleeHit.cpp, to change or even nullify hit events that happen shortly after the
// collision
#pragma once

#include <RE/Skyrim.h>
#include "Settings.h"
#include "OnMeleeHit.h"

using namespace SKSE;
using namespace SKSE::log;

extern bool bDisableSpark;

namespace ZacOnFrame {

    void InstallFrameHook();
    void OnFrameUpdate();
    bool FrameGetWeaponPos(RE::Actor*, RE::NiPoint3&, RE::NiPoint3&, RE::NiPoint3&, RE::NiPoint3&, bool);
    bool FrameGetWeaponFixedPos(RE::Actor*, RE::NiPoint3&, RE::NiPoint3&, RE::NiPoint3&, RE::NiPoint3&);
    bool IsNiPointZero(const RE::NiPoint3&);
    RE::hkVector4 CalculatePushVector(RE::NiPoint3 sourcePos, RE::NiPoint3 targetPos, bool isEnemy, float speed);
    void TimeSlowEffect(RE::Actor*, int64_t);
    void StopTimeSlowEffect(RE::Actor*);
    void CleanBeforeLoad();
    

    // Stores the original OnFrame function, and call it no matter what later, so we don't break game's functionality
    static REL::Relocation<decltype(OnFrameUpdate)> _OnFrame;

        //// For several frames, change the angle of enemy, creating hit juice
        //void ChangeAngle() { 
        //    if (!enemy) return;
        //    if (angleSetCount < enemyRotFrame) {
        //        if (enemyIsRotClockwise) {
        //            enemy->SetRotationZ(enemyOriAngleZ + angleSetCount * fEnemyRotStep);
        //        } else {
        //            enemy->SetRotationZ(enemyOriAngleZ - angleSetCount * fEnemyRotStep);
        //        }
        //        angleSetCount++;
        //    }
        //}

        //// For several frames, change the velocity of enemy, to push them away
        //void ChangeVelocity() {
        //    if (!enemy) return;
        //    float x = hkv.quad.m128_f32[0];
        //    float y = hkv.quad.m128_f32[1];
        //    log::trace("Entering ChangeVelocity of enemy, hkv.x:{}, hkv.y:{}", x,
        //               y);
        //    /*if (sqrt(x * x + y * y) < 10.0f) {
        //        return;
        //    }*/

        //    // If enemy is already far enough or this function is about to get no more call, set hkv to zero so they won't be pushed farther
        //    auto dist = enemy->GetPosition().GetDistance(
        //        enemyOriPos); 
        //    if (dist > fEnemyPushMaxDist || dist > pushEnemyMaxDist ||
        //        (iFrameCount - iFrameCollision == collisionIgnoreDur - 3)) { 
        //        hkv = hkv * 0.0f;
        //        if (enemy && enemy->GetCharController()) {
        //            RE::hkVector4 tmp;
        //            enemy->GetCharController()->GetLinearVelocityImpl(tmp);
        //            tmp = tmp * 0.0f;  // reset speed
        //            enemy->GetCharController()->SetLinearVelocityImpl(tmp);
        //        }
        //        return;
        //    }

        //    if (enemy && enemy->GetCharController()) {
        //        log::trace("About to change enemy speed, hkv length:{}", hkv.SqrLength3());
        //        RE::hkVector4 tmp;
        //        enemy->GetCharController()->GetLinearVelocityImpl(tmp);
        //        tmp = tmp + hkv; // new speed considers old speed
        //        //tmp = hkv;  // new speed doesn't consider old speed
        //        enemy->GetCharController()->SetLinearVelocityImpl(tmp);
        //    }

        //    
        //    return;
        //}


    class WeaponPos { // this class is just used to store player's weapon pos and calculate speed
    public:
        RE::NiPoint3 bottom;
        RE::NiPoint3 top;
        WeaponPos() = default;
        WeaponPos(RE::NiPoint3 b, RE::NiPoint3 t) : bottom(b), top(t) {}
    };

    class SpeedRing {
    public:
        std::vector<WeaponPos> bufferL;
        std::vector<WeaponPos> bufferR;
        std::size_t capacity; // how many latest frames are stored
        std::size_t indexCurrentL;
        std::size_t indexCurrentR;
        SpeedRing(std::size_t cap) : bufferL(cap), bufferR(cap), capacity(cap), indexCurrentR(0), indexCurrentL(0) {}
        
        void Clear() {

        }

        void Push(WeaponPos p, bool isLeft) { 
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
            const std::vector<WeaponPos>& buffer = isLeft ? bufferL : bufferR;

            // Get the start and end positions
            RE::NiPoint3 startPosBottom = buffer[(currentIdx - N + capacity) % capacity].bottom;
            RE::NiPoint3 endPosBottom = buffer[(currentIdx - 1 + capacity) % capacity].bottom;
            RE::NiPoint3 startPosTop = buffer[(currentIdx - N + capacity) % capacity].top;
            RE::NiPoint3 endPosTop = buffer[(currentIdx - 1 + capacity) % capacity].top;

            // Calculate velocities
            RE::NiPoint3 velocityBottom = (endPosBottom - startPosBottom) / static_cast<float>(N);
            RE::NiPoint3 velocityTop = (endPosTop - startPosTop) / static_cast<float>(N);

            // Return the larger velocity based on magnitude
            return (velocityBottom.Length() > velocityTop.Length()) ? velocityBottom : velocityTop;
        }

    };
    extern SpeedRing speedBuf; // positions of player's weapons, but the handle length is 0 and weapon length is a fixed number
    extern SpeedRing weapPosBuf;  // positions of player's weapons

    // Record information for timeslow spell
    class SlowTimeEffect {
    public: 
        int64_t frameShouldRemove;
        std::vector<float> oldMagnitude; 
        RE::SpellItem* timeSlowSpell;
        int64_t frameLastSlowTime; // don't allow too frequent slow time 
        SlowTimeEffect(std::size_t cap)
            : frameShouldRemove(-1), frameLastSlowTime(-1), oldMagnitude(cap), timeSlowSpell(nullptr) {}

        bool shouldRemove(int64_t currentFrame) {
            if (frameShouldRemove == -1) {
                return false;
            }
            return frameShouldRemove <= currentFrame;
        }

        void clear() { 
            frameShouldRemove = -1;
            oldMagnitude.clear();
        }
    };

    extern SlowTimeEffect slowTimeData;


    void FillShieldCenterNormal(RE::Actor* actor, RE::NiPoint3& shieldCenter, RE::NiPoint3& shieldNormal);
}

