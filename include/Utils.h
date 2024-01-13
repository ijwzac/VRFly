#pragma once

#include <string>

bool play_impact_1(RE::Actor* actor, const RE::BSFixedString& nodeName);
bool play_impact_2(RE::TESObjectREFR* a, RE::BGSImpactData* impact, RE::NiPoint3* P_V, RE::NiPoint3* P_from,
                   RE::NiNode* bone);
bool play_impact_3(RE::TESObjectCELL* cell, float a_lifetime, const char* model, RE::NiPoint3* a_rotation,
                   RE::NiPoint3* a_position, float a_scale, uint32_t a_flags, RE::NiNode* a_target);

bool HasShield(RE::Actor* actor);


void vibrateController(int hapticFrame, bool isLeft);

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

RE::TESGlobal* GetMyGlobal(RE::FormID partFormID);
RE::TESGlobal* GetTriggerL();
RE::TESGlobal* GetTriggerR();

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
