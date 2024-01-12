#pragma once

#include <RE/Skyrim.h>
#include "Settings.h"

using namespace SKSE;
using namespace SKSE::log;

namespace OnMeleeHit {
#pragma warning(push)
#pragma warning(disable : 4251)

    class __declspec(dllexport) OnMeleeHitHook {
    public:
        [[nodiscard]] static OnMeleeHitHook& GetSingleton() noexcept;

        static void InstallHook();


    private:
        OnMeleeHitHook() = default;
        OnMeleeHitHook(const OnMeleeHitHook&) = delete;
        OnMeleeHitHook(OnMeleeHitHook&&) = delete;
        ~OnMeleeHitHook() = default;

        OnMeleeHitHook& operator=(const OnMeleeHitHook&) = delete;
        OnMeleeHitHook& operator=(OnMeleeHitHook&&) = delete;

        /** Our new function */
        static void OnMeleeHit(RE::Actor* attacker, RE::Actor* target, std::int64_t a_int1, bool a_bool,
                               void* a_unkptr);

        /** The original function */
        static inline REL::Relocation<decltype(OnMeleeHit)> _OnMeleeHit;

    public:
        static void FireOriMeleeHit(RE::Actor* attacker, RE::Actor* target, std::int64_t a_int1, bool a_bool,
                                    void* a_unkptr) {
            _OnMeleeHit(attacker, target, a_int1, a_bool, a_unkptr);
        }
    };

/*Impacts:
(Default one) 0x0004BB52 WPNBlade1HandVsMetaImpact
 0x0004BB53 WPNBluntVsMetalImpact
 0x0004BB54 WPNArrowVsMetalImpact ; maybe try this out to have Dplay_Block_SparksN.esp increase the effect?
 0x0004BB55 WPNAxeVsMetalImpact
 0x0005CEF7 PHYGenericMetalSmallImpact
 0x0005CEF8 PHYGenericMetalMediumImpact
 0x0005CEFB PHYGenericMetalHeavyImpact
*/
    bool play_impact_1(RE::Actor* actor, const RE::BSFixedString& nodeName);
    bool debug_show_weapon_collide(RE::Actor* actor, const RE::BSFixedString& nodeName);
    bool play_impact_2(RE::TESObjectREFR* a, RE::BGSImpactData* impact, RE::NiPoint3* P_V, RE::NiPoint3* P_from,
                     RE::NiNode* bone);
    bool play_impact_3(RE::TESObjectCELL* cell, float a_lifetime, const char* model, RE::NiPoint3* a_rotation,
                       RE::NiPoint3* a_position, float a_scale, uint32_t a_flags, RE::NiNode* a_target);

    // From: https://github.com/ersh1/Precision/blob/567978e940dc36dc2f68d704fc2ccec4a77a7f10/src/Offsets.h#L334
    typedef float (*tActor_GetReach)(RE::Actor* a_this);
    static REL::Relocation<tActor_GetReach> Actor_GetReach{RELOCATION_ID(37588, 38538)};  // 623F10, 649520

    // From: https://github.com/D7ry/valhallaCombat/blob/2d686d2bddca3b3448af3af3c6b43e2fb3f5ced9/src/include/offsets.h#L48
    inline int soundHelper_a(void* manager, RE::BSSoundHandle* a2, int a3, int a4)  // sub_140BEEE70
    {
        using func_t = decltype(&soundHelper_a);
        REL::Relocation<func_t> func{RELOCATION_ID(66401, 67663)};
        return func(manager, a2, a3, a4);
    }

    // From: https://github.com/D7ry/valhallaCombat/blob/2d686d2bddca3b3448af3af3c6b43e2fb3f5ced9/src/include/offsets.h#L55
    inline void soundHelper_b(RE::BSSoundHandle* a1, RE::NiAVObject* source_node)  // sub_140BEDB10
    {
        using func_t = decltype(&soundHelper_b);
        REL::Relocation<func_t> func{RELOCATION_ID(66375, 67636)};
        return func(a1, source_node);
    }

    // From: https://github.com/D7ry/valhallaCombat/blob/2d686d2bddca3b3448af3af3c6b43e2fb3f5ced9/src/include/offsets.h#L62
    inline char __fastcall soundHelper_c(RE::BSSoundHandle* a1)  // sub_140BED530
    {
        using func_t = decltype(&soundHelper_c);
        REL::Relocation<func_t> func{RELOCATION_ID(66355, 67616)};
        return func(a1);
    }

    // From: https://github.com/D7ry/valhallaCombat/blob/2d686d2bddca3b3448af3af3c6b43e2fb3f5ced9/src/include/offsets.h#L69
    inline char set_sound_position(RE::BSSoundHandle* a1, float x, float y, float z) {
        using func_t = decltype(&set_sound_position);
        REL::Relocation<func_t> func{RELOCATION_ID(66370, 67631)};
        return func(a1, x, y, z);
    }

  

#pragma warning(pop)
}  // namespace OnMeleeHit

