#pragma once

#include "Settings.h"

#include <string>

bool play_impact_1(RE::Actor* actor, const RE::BSFixedString& nodeName);
bool play_impact_2(RE::TESObjectREFR* a, RE::BGSImpactData* impact, RE::NiPoint3* P_V, RE::NiPoint3* P_from,
                   RE::NiNode* bone);
bool play_impact_3(RE::TESObjectCELL* cell, float a_lifetime, const char* model, RE::NiPoint3* a_rotation,
                   RE::NiPoint3* a_position, float a_scale, uint32_t a_flags, RE::NiNode* a_target);

bool HasShield(RE::Actor* actor);


void vibrateController(int hapticFrame, int length, bool isLeft);

std::string formatNiPoint3(RE::NiPoint3& pos);

bool IsNiPointZero(const RE::NiPoint3& pos);

bool AnyPointZero(const RE::NiPoint3& A, const RE::NiPoint3& B, const RE::NiPoint3& C, const RE::NiPoint3& D);

RE::NiMatrix3 ConvertToPlayerSpace(const RE::NiMatrix3& R_weapon_world_space,
                                   const RE::NiMatrix3& R_player_world_space);

RE::NiMatrix3 ConvertToWorldSpace(const RE::NiMatrix3& R_weapon_player_space,
                                  const RE::NiMatrix3& R_player_world_space);

RE::NiMatrix3 adjustNodeRotation(RE::NiNode* baseNode, RE::NiMatrix3& rotation, RE::NiPoint3 adjust, bool useAdjust2);

uint32_t GetBaseFormID(uint32_t formId);
uint32_t GetFullFormID(const uint8_t modIndex, uint32_t formLower);
uint32_t GetFullFormID_ESL(const uint8_t modIndex, const uint16_t esl_index, uint32_t formLower);

RE::SpellItem* GetTimeSlowSpell_SpeelWheel();
RE::SpellItem* GetTimeSlowSpell_Mine();

RE::SpellItem* GetMySpell(RE::FormID partFormID);
RE::SpellItem* GetLiftSpell();
RE::SpellItem* GetXYSpell();
RE::SpellItem* GetXYZSpell();
RE::SpellItem* GetEmitSpell();
RE::SpellItem* GetEmitFireSpell();
RE::SpellItem* GetEmitForceSpell();
RE::SpellItem* GetWingsFlagSpell();
RE::SpellItem* GetShockWaveSpell();
RE::SpellItem* GetSpiritualLiftEffSpell();
RE::SpellItem* GetHeatLiftEffSpell();
RE::SpellItem* GetFlapStaRegenEffSpell();

RE::TESGlobal* GetMyGlobal(RE::FormID partFormID);
RE::TESGlobal* GetTriggerL();
RE::TESGlobal* GetTriggerR();
RE::TESGlobal* GetGripL();
RE::TESGlobal* GetGripR();


RE::TESForm* GetMyForm(RE::FormID partFormID);
RE::TESObjectACTI* GetSteamSm();
RE::TESObjectACTI* GetSteamLg();
RE::TESObjectACTI* GetWindMid();
RE::TESObjectACTI* GetWindLg();
RE::TESObjectACTI* GetWindEx();


RE::BGSExplosion* GetExploSm();
RE::BGSExplosion* GetExploMid();
RE::BGSExplosion* GetExploLg();
RE::BGSExplosion* GetExploRock();

RE::BGSListForm* GetHeatSourceFire();
RE::BGSListForm* GetHeatSourceFirepit();

// From: https://github.com/fenix31415/UselessFenixUtils
void play_sound(RE::TESObjectREFR* object, RE::FormID formid, float volume);

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

class twoNodes {
public:
    RE::NiNode* nodeL;
    RE::NiNode* nodeR;

    twoNodes(RE::NiNode* nodeL, RE::NiNode* nodeR) : nodeL(nodeL), nodeR(nodeR) {}

    bool isEmpty() { return nodeL == nullptr || nodeR == nullptr;
    }
};

twoNodes HandleClawRaces(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                       RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR);

static RE::FormID werewolfRace = 0xCDD84;
static RE::FormID raceWereBear = 0x1e17b;  // need to remove higher bits from race
static RE::FormID vampLord = 0x0283a;  // need to remove higher bits

RE::NiPoint3 GetPlayerHandPos(bool isLeft, RE::Actor* player);
RE::NiPoint3 GetPlayerHmdPos(RE::Actor* player);

bool IsPlayerHandCloseToHead(RE::Actor* player);

void debug_show_weapon_range(RE::Actor* actor, RE::NiPoint3& posWeaponBottom, RE::NiPoint3& posWeaponTop,
                             RE::NiNode* bone);

RE::NiPoint3 Quad2Velo(RE::hkVector4& a_velocity);

float CurrentSpellWheelSlowRatio(RE::Actor* player);

float CurrentMyTimeSlowRatio(RE::Actor* player);

// Get a float conf that is stored in a TESGlobal
float GetFConf(FConf name);