#include "OnMeleeHit.h"
#include "Utils.h"

using namespace SKSE;
using namespace SKSE::log;


bool play_impact_1(RE::Actor* actor, const RE::BSFixedString& nodeName) {
    auto root = netimmerse_cast<RE::BSFadeNode*>(actor->Get3D());
    if (!root) return false;
    auto bone = netimmerse_cast<RE::NiNode*>(root->GetObjectByName(nodeName));
    if (!bone) return false;

    float reach = 20.0f;
    auto weaponDirection =
        RE::NiPoint3{bone->world.rotate.entry[0][1], bone->world.rotate.entry[1][1], bone->world.rotate.entry[2][1]};
    RE::NiPoint3 to = bone->world.translate + weaponDirection * reach;
    RE::NiPoint3 P_V = {0.0f, 0.0f, 0.0f};

    return play_impact_2(actor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x0004BB52), &P_V, &to, bone);
}

// From: https://github.com/fenix31415/UselessFenixUtils
bool play_impact_2(RE::TESObjectREFR* a, RE::BGSImpactData* impact, RE::NiPoint3* P_V, RE::NiPoint3* P_from,
                               RE::NiNode* bone) {
    return play_impact_3(a->GetParentCell(), 1.0f, impact->GetModel(), P_V, P_from, 1.0f, 7, bone);
}

bool play_impact_3(RE::TESObjectCELL* cell, float a_lifetime, const char* model, RE::NiPoint3* a_rotation,
                               RE::NiPoint3* a_position, float a_scale, uint32_t a_flags, RE::NiNode* a_target) {
    return RE::BSTempEffectParticle::Spawn(cell, a_lifetime, model, *a_rotation, *a_position, a_scale, a_flags,
                                           a_target);
}

bool HasShield(RE::Actor* actor) {
    if (auto equipL = actor->GetEquippedObject(true); equipL) {
        if (equipL->IsArmor()) {
            return true;
        }
    }
    return false;
}

std::string formatNiPoint3(RE::NiPoint3& pos) {
    std::ostringstream stream;
    stream << "(" << pos.x << ", " << pos.y << ", " << pos.z << ")";
    return stream.str();
}

bool IsNiPointZero(const RE::NiPoint3& pos) {
    if (pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f) {
        return true;
    } else {
        return false;
    }
}

bool AnyPointZero(const RE::NiPoint3& A, const RE::NiPoint3& B, const RE::NiPoint3& C, const RE::NiPoint3& D) { 
    if (IsNiPointZero(A) || IsNiPointZero(B) || IsNiPointZero(C) || IsNiPointZero(D)) {
        return true;
    }
    return false;
}

RE::NiMatrix3 ConvertToPlayerSpace(const RE::NiMatrix3& R_weapon_world_space,
                                   const RE::NiMatrix3& R_player_world_space) {
    // Transpose of player's rotation matrix in world space
    RE::NiMatrix3 R_player_world_space_transposed;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R_player_world_space_transposed.entry[i][j] = R_player_world_space.entry[j][i];
        }
    }

    // Multiplying with weapon's rotation matrix in world space
    RE::NiMatrix3 R_weapon_player_space;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R_weapon_player_space.entry[i][j] = 0.0f;
            for (int k = 0; k < 3; ++k) {
                R_weapon_player_space.entry[i][j] +=
                    R_player_world_space_transposed.entry[i][k] * R_weapon_world_space.entry[k][j];
            }
        }
    }

    return R_weapon_player_space;
}

RE::NiMatrix3 ConvertToWorldSpace(const RE::NiMatrix3& R_weapon_player_space,
                                  const RE::NiMatrix3& R_player_world_space) {
    RE::NiMatrix3 R_weapon_world_space;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R_weapon_world_space.entry[i][j] = 0.0f;
            for (int k = 0; k < 3; ++k) {
                R_weapon_world_space.entry[i][j] +=
                    R_player_world_space.entry[i][k] * R_weapon_player_space.entry[k][j];
            }
        }
    }
    return R_weapon_world_space;
}


RE::NiMatrix3 adjustNodeRotation(RE::NiNode* baseNode, RE::NiMatrix3& rotation, RE::NiPoint3 adjust, bool useAdjust2) {
    RE::NiMatrix3 newRotation = rotation;
    if (baseNode) {
        auto rotation_base = baseNode->world.rotate;
        newRotation = ConvertToPlayerSpace(rotation, rotation_base);

        if (useAdjust2) {
            RE::NiMatrix3 adjust2;
            // Set up a 90-degree rotation matrix around Z-axis
            adjust2.entry[0][0] = 0.0f;
            adjust2.entry[0][1] = 0.0f;
            adjust2.entry[0][2] = 1.0f;
            adjust2.entry[1][0] = 0.0f;
            adjust2.entry[1][1] = 1.0f;
            adjust2.entry[1][2] = 0.0f;
            adjust2.entry[2][0] = -1.0f;
            adjust2.entry[2][1] = 0.0f;
            adjust2.entry[2][2] = 0.0f;

            newRotation = newRotation * adjust2;
        } else {
            newRotation = newRotation * RE::NiMatrix3(adjust.x, adjust.y, adjust.z);
        }


        newRotation = ConvertToWorldSpace(newRotation, rotation_base);
    } else {
        log::warn("Base node is null");
    }
    return newRotation;
}

uint32_t GetBaseFormID(uint32_t formId) { return formId & 0x00FFFFFF; }

uint32_t GetFullFormID(const uint8_t modIndex, uint32_t formLower) { return (modIndex << 24) | formLower; }

uint32_t GetFullFormID_ESL(const uint8_t modIndex, const uint16_t esl_index, uint32_t formLower) {
    return (modIndex << 24) | (esl_index << 12) | formLower;
}

// The time slow spell is given by SpeelWheel VR by Shizof. Thanks for the support!
// May return nullptr
RE::SpellItem* GetTimeSlowSpell_SpeelWheel() { 
    auto handler = RE::TESDataHandler::GetSingleton();
    if (!handler) {
        log::error("GetTimeSlowSpell: failed to get TESDataHandler");
        return nullptr;
    }
    auto spellWheelIndex = handler->GetLoadedModIndex("SpellWheelVR.esp");
    if (!spellWheelIndex.has_value()) {
        log::trace("GetTimeSlowSpell: failed to get spellWheel");
        return nullptr;
    }

    //auto spellWheelMod = handler->LookupLoadedModByName("SpellWheelVR.esp");
    //if (spellWheelMod) {
    //    log::trace("GetTimeSlowSpell: get spellWheel mod");
    //} else {
    //    log::trace("GetTimeSlowSpell: didn't get spellWheel mod");
    //}

    auto vrikMod = handler->LookupLoadedModByName("vrik.esp");
    if (vrikMod) {
        log::trace("GetTimeSlowSpell: get vrik mod");
    } else {
        log::trace("GetTimeSlowSpell: didn't get vrik mod");
    }

    // Thanks to Shizof, there is a spell with formID 20A00 that is specifically added in Spell Wheel VR for our convenience 
    // So we should try this one first
    RE::FormID partFormID = 0x000EA5; // this is the spell that Spell wheel itself uses to slow time, not the one given to our weap collision mod
    RE::FormID fullFormID = GetFullFormID(spellWheelIndex.value(), partFormID);
    RE::SpellItem* timeSlowSpell = RE::TESForm::LookupByID<RE::SpellItem>(fullFormID); 
    if (!timeSlowSpell) {
        log::error("GetTimeSlowSpell: failed to get timeslow spell");
        return nullptr;
    }
    return timeSlowSpell;
}

RE::SpellItem* GetTimeSlowSpell_Mine() {

    RE::FormID partFormID = 0x01C881;
    RE::SpellItem* timeSlowSpell = GetMySpell(partFormID);
    return timeSlowSpell;
}

RE::SpellItem* GetMySpell(RE::FormID partFormID) {
    RE::SpellItem* spell;

    // First, try to find the spell using normal ESP
    auto handler = RE::TESDataHandler::GetSingleton();
    if (!handler) {
        log::error("GetMySpell: failed to get TESDataHandler");
        return nullptr;
    }
    auto espIndex = handler->GetLoadedModIndex("VRFly.esp");
    if (!espIndex.has_value()) {
        log::trace("GetMySpell: failed to get VRFly.esp");
    } else {
        RE::FormID fullFormID = GetFullFormID(espIndex.value(), partFormID);
        spell = RE::TESForm::LookupByID<RE::SpellItem>(fullFormID);
        if (spell) return spell;
    }
    
    // Second, try to find the spell using ESL
    // TODO: is this really OK?
    for (uint16_t i = 0; i <= 0xFFF; i++) {
        RE::FormID fullFormID = GetFullFormID_ESL(0xFE, i, partFormID);
        spell = RE::TESForm::LookupByID<RE::SpellItem>(fullFormID);
        if (spell) return spell;
    }

    log::error("GetMySpell: failed to get the spell {:x}", partFormID);
    return nullptr;
}

RE::SpellItem* GetLiftSpell() { 
    static RE::SpellItem* s;
    if (!s) {
        s = GetMySpell(0x000D65);
    }
    return s; 
}
RE::SpellItem* GetXYSpell() {
    static RE::SpellItem* s;
    if (!s) {
        s = GetMySpell(0x000D69);
    }
    return s;
}
RE::SpellItem* GetXYZSpell() {
    static RE::SpellItem* s;
    if (!s) {
        s = GetMySpell(0x000D6B);
    }
    return s;
}
RE::SpellItem* GetEmitSpell() {
    static RE::SpellItem* s;
    if (!s) {
        s = GetMySpell(0x001D9F);
    }
    return s;
}

RE::SpellItem* GetEmitFireSpell() {
    static RE::SpellItem* s;
    if (!s) {
        s = GetMySpell(0x00183A); 
    }
    return s;
}

RE::SpellItem* GetEmitForceSpell() {
    static RE::SpellItem* s;
    if (!s) {
        s = GetMySpell(0x002305); 
    }
    return s;
}

RE::SpellItem* GetWingsFlagSpell() {
    static RE::SpellItem* s;
    if (!s) {
        s = GetMySpell(0x0051D6);
    }
    return s;
}
RE::SpellItem* GetShockWaveSpell() {
    static RE::SpellItem* s;
    if (!s) {
        s = GetMySpell(0x01C886);
    }
    return s;
}
// no fall damage perk 183C


RE::TESGlobal* GetMyGlobal(RE::FormID partFormID) {
    RE::TESGlobal* g;

    // First, try to find the spell using normal ESP
    auto handler = RE::TESDataHandler::GetSingleton();
    if (!handler) {
        log::error("GetMyGlobal: failed to get TESDataHandler");
        return nullptr;
    }
    auto espIndex = handler->GetLoadedModIndex("VRFly.esp");
    if (!espIndex.has_value()) {
        log::trace("GetMyGlobal: failed to get VRFly.esp");
    } else {
        RE::FormID fullFormID = GetFullFormID(espIndex.value(), partFormID);
        g = RE::TESForm::LookupByID<RE::TESGlobal>(fullFormID);
        if (g) return g;
    }

    // Second, try to find the spell using ESL
    // TODO: is this really OK?
    for (uint16_t i = 0; i <= 0xFFF; i++) {
        RE::FormID fullFormID = GetFullFormID_ESL(0xFE, i, partFormID);
        g = RE::TESForm::LookupByID<RE::TESGlobal>(fullFormID);
        if (g) return g;
    }

    log::error("GetMyGlobal: failed to get the global {:x}", partFormID);
    return nullptr;
}

RE::TESGlobal* GetTriggerL() {
    static RE::TESGlobal* g;
    if (!g) {
        g = GetMyGlobal(0x0012D4);
    }
    return g;
}
RE::TESGlobal* GetTriggerR() {
    static RE::TESGlobal* g;
    if (!g) {
        g = GetMyGlobal(0x0012D5);
    }
    return g;
}

RE::TESGlobal* GetGripL() {
    static RE::TESGlobal* g;
    if (!g) {
        g = GetMyGlobal(0x0012D6);
    }
    return g;
}
RE::TESGlobal* GetGripR() {
    static RE::TESGlobal* g;
    if (!g) {
        g = GetMyGlobal(0x0012D7);
    }
    return g;
}


RE::TESForm* GetMyForm(RE::FormID partFormID) {
    RE::TESForm* g;

    // First, try to find the spell using normal ESP
    auto handler = RE::TESDataHandler::GetSingleton();
    if (!handler) {
        log::error("GetMyForm: failed to get TESDataHandler");
        return nullptr;
    }
    auto espIndex = handler->GetLoadedModIndex("VRFly.esp");
    if (!espIndex.has_value()) {
        log::trace("GetMyForm: failed to get VRFly.esp");
    } else {
        RE::FormID fullFormID = GetFullFormID(espIndex.value(), partFormID);
        g = RE::TESForm::LookupByID(fullFormID);
        if (g) return g;
    }

    // Second, try to find the spell using ESL
    // TODO: is this really OK?
    for (uint16_t i = 0; i <= 0xFFF; i++) {
        RE::FormID fullFormID = GetFullFormID_ESL(0xFE, i, partFormID);
        g = RE::TESForm::LookupByID(fullFormID);
        if (g) return g;
    }

    log::error("GetMyForm: failed to get the form {:x}", partFormID);
    return nullptr;
}

RE::TESObjectACTI* GetSteamSm() {
    static RE::TESObjectACTI* g;
    if (!g) {
        g = static_cast<RE::TESObjectACTI*>(GetMyForm(0x0080A3));
    }
    return g;
}
RE::TESObjectACTI* GetSteamLg() {
    static RE::TESObjectACTI* g;
    if (!g) {
        g = static_cast<RE::TESObjectACTI*>(GetMyForm(0x0254E8));
    }
    return g;
}
RE::TESObjectACTI* GetWindMid() {
    static RE::TESObjectACTI* g;
    if (!g) {
        g = static_cast<RE::TESObjectACTI*>(GetMyForm(0x0080A4));
    }
    return g;
}
RE::TESObjectACTI* GetWindLg() {
    static RE::TESObjectACTI* g;
    if (!g) {
        g = static_cast<RE::TESObjectACTI*>(GetMyForm(0x0080A5));
    }
    return g;
}
RE::TESObjectACTI* GetWindEx() {
    static RE::TESObjectACTI* g;
    if (!g) {
        g = static_cast<RE::TESObjectACTI*>(GetMyForm(0x0080A6));
    }
    return g;
}

RE::BGSExplosion* GetExploSm() {
    static RE::BGSExplosion* g;
    if (!g) {
        g = static_cast<RE::BGSExplosion*>(GetMyForm(0x00DE96));
    }
    return g;
}
RE::BGSExplosion* GetExploMid() {
    static RE::BGSExplosion* g;
    if (!g) {
        g = static_cast<RE::BGSExplosion*>(GetMyForm(0x00DE97));
    }
    return g;
}
RE::BGSExplosion* GetExploLg() {
    static RE::BGSExplosion* g;
    if (!g) {
        g = static_cast<RE::BGSExplosion*>(GetMyForm(0x00DE98));
    }
    return g;
}

RE::BGSExplosion* GetExploRock() {
    static RE::BGSExplosion* g;
    if (!g) {
        g = static_cast<RE::BGSExplosion*>(GetMyForm(0x0080AA));
    }
    return g;
}


// From: https://github.com/fenix31415/UselessFenixUtils
void play_sound(RE::TESObjectREFR* object, RE::FormID formid, float volume) {
    RE::BSSoundHandle handle;
    handle.soundID = static_cast<uint32_t>(-1);
    handle.assumeSuccess = false;
    *(uint32_t*)&handle.state = 0;

    auto manager = RE::BSAudioManager::GetSingleton();
    if (manager) {
        soundHelper_a(manager, &handle, formid, 16);
        if (set_sound_position(&handle, object->data.location.x, object->data.location.y, object->data.location.z)) {
            handle.SetVolume(volume);
            soundHelper_b(&handle, object->Get3D());
            soundHelper_c(&handle);
        }
    }
}

float generateRandomFloat(float min, float max) {
    // Create a random device and use it to seed the Mersenne Twister engine
    std::random_device rd;
    std::mt19937 gen(rd());

    // Define the distribution, range is min to max
    std::uniform_real_distribution<float> dis(min, max);

    // Generate and return the random number
    return dis(gen);
}

twoNodes HandleClawRaces(RE::Actor* actor, RE::NiPoint3& posWeaponBottomL, RE::NiPoint3& posWeaponBottomR,
                       RE::NiPoint3& posWeaponTopL, RE::NiPoint3& posWeaponTopR) {

    if (actor && actor->GetRace() &&
        (actor->GetRace()->formID == werewolfRace || GetBaseFormID(actor->GetRace()->formID) == raceWereBear  ||
         GetBaseFormID(actor->GetRace()->formID) == vampLord)) {
    } else {
        return twoNodes(nullptr, nullptr);
    }

    bool isRace = false;

    const auto actorRoot = netimmerse_cast<RE::BSFadeNode*>(actor->Get3D());
    if (!actorRoot) {
        log::warn("Fail to get actorRoot:{}", actor->GetBaseObject()->GetName());
        return twoNodes(nullptr, nullptr);
    }

    const auto weaponNodeNameL = "NPC L Hand [RHnd]"sv;  // yes, werewolves have [RHnd] even for left hand
    const auto weaponNodeNameL_Alt = "NPC L Hand [LHnd]"sv;
    const auto weaponNodeNameR = "NPC R Hand [RHnd]"sv;
    auto weaponNodeL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameL));
    auto weaponNodeR = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameR));
    if (!weaponNodeL) weaponNodeL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameL_Alt));

    // for bears
    const auto weaponNodeNameL_Alt2 = "NPC LThumb02"sv;
    const auto weaponNodeNameR_Alt2 = "NPC RThumb02"sv;
    if (!weaponNodeL) weaponNodeL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameL_Alt2));
    if (!weaponNodeR) weaponNodeR = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(weaponNodeNameR_Alt2));

    
    const auto nodeBaseStr = "NPC Pelvis [Pelv]"sv;  // base of at least werewolves
    const auto baseNode = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeBaseStr));

    if (weaponNodeL && weaponNodeR) { 
        isRace = true;

        float reachR(5.0f), handleR(34.0f); // our rotation matrix kinda inverts the rotation
        posWeaponBottomR = weaponNodeR->world.translate;

        auto rotationR = weaponNodeR->world.rotate;

        // Adjust right claw rotation. Original rotation is like the claw is holding a dagger, but we want it sticking out
        rotationR = adjustNodeRotation(baseNode, rotationR, RE::NiPoint3(1.5f, 0.0f, 0.0f), false);

        auto weaponDirectionR = RE::NiPoint3{rotationR.entry[0][1], rotationR.entry[1][1], rotationR.entry[2][1]};

        posWeaponTopR = posWeaponBottomR + weaponDirectionR * reachR;
        posWeaponBottomR = posWeaponBottomR - weaponDirectionR * handleR;

        float reachL(5.0f), handleL(34.0f);
        posWeaponBottomL = weaponNodeL->world.translate;

        auto rotationL = weaponNodeL->world.rotate;
        rotationL = adjustNodeRotation(baseNode, rotationL, RE::NiPoint3(1.5f, 0.0f, 0.0f), false);

        auto weaponDirectionL = RE::NiPoint3{rotationL.entry[0][1], rotationL.entry[1][1], rotationL.entry[2][1]};

        posWeaponTopL = posWeaponBottomL + weaponDirectionL * reachL;
        posWeaponBottomL = posWeaponBottomL - weaponDirectionL * handleL;


        log::trace("Is werewolf or werebear or trolls. actor:{}", actor->GetBaseObject()->GetName());
    }

    if (isRace) {
        return twoNodes(weaponNodeL, weaponNodeR);
    } else {
        return twoNodes(nullptr, nullptr);
    }
}

void vibrateController(int hapticFrame, int length, bool isLeft) {
    auto papyrusVM = RE::BSScript::Internal::VirtualMachine::GetSingleton();

    if (papyrusVM) {
        hapticFrame = hapticFrame < iHapticStrMin ? iHapticStrMin : hapticFrame;
        hapticFrame = hapticFrame > iHapticStrMax ? iHapticStrMax : hapticFrame;

        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;

        //log::trace("Calling papyrus");
        if (papyrusVM->TypeIsValid("VRIK"sv)) {
            //log::trace("VRIK is installed");
            RE::BSScript::IFunctionArguments* hapticArgs;
            if (isLeft) {
                hapticArgs = RE::MakeFunctionArguments(true, (int)hapticFrame, (int)length);
            } else {
                hapticArgs = RE::MakeFunctionArguments(false, (int)hapticFrame, (int)length);
            }
            // Function VrikHapticPulse(Bool onLeftHand, Int frames, Int microsec) native global
            papyrusVM->DispatchStaticCall("VRIK"sv, "VrikHapticPulse"sv, hapticArgs, callback);
        } else {
            log::trace("VRIK not installed");
            // Now we call vanilla's script Game.ShakeController(float afLeftStrength, float afRightStrength, float
            // afDuration) afLeftStrength: The strength of the left motor. Clamped from 0 to 1. afRightStrength: The
            // strength of the right motor.Clamped from 0 to 1. afDuration : How long to shake the controller - in
            // seconds.
            if (papyrusVM->TypeIsValid("Game"sv)) {
                RE::BSScript::IFunctionArguments* hapticArgs;
                if (isLeft) {
                    hapticArgs = RE::MakeFunctionArguments(((float)hapticFrame) / ((float)length), 0.0f,
                                                           (float)iHapticLengthMicroSec / 1000000);
                } else {
                    hapticArgs = RE::MakeFunctionArguments(0.0f, ((float)hapticFrame) / ((float)length),
                                                           (float)iHapticLengthMicroSec / 1000000);
                }
                papyrusVM->DispatchStaticCall("Game"sv, "ShakeController"sv, hapticArgs, callback);
            } else {
                log::trace("Failed to find vanilla Game script");
            }
        }

        //log::trace("Finished calling papyrus");
    }
}

RE::NiPoint3 GetPlayerHandPos_2(bool isLeft, RE::Actor* player, bool minusBase) {
    const auto actorRoot = netimmerse_cast<RE::BSFadeNode*>(player->Get3D());
    if (!actorRoot) {
        log::warn("GetPlayerHandPos:Fail to find player");
        return RE::NiPoint3();
    }
    const auto nodeNameL = "NPC L Hand [LHnd]"sv;
    const auto nodeNameR = "NPC R Hand [RHnd]"sv;
    const auto weaponL = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeNameL));
    const auto weaponR = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeNameR));
    const auto nodeBaseStr = "NPC Pelvis [Pelv]"sv;  // base of player
    const auto baseNode = netimmerse_cast<RE::NiNode*>(actorRoot->GetObjectByName(nodeBaseStr));

    RE::NiPoint3 pos, result;
    if (isLeft && weaponL && baseNode) {
        if (minusBase) {
            pos = weaponL->world.translate - baseNode->world.translate;
        } else {
            pos = weaponL->world.translate;
        }
    } else if (!isLeft && weaponR && baseNode) {
        if (minusBase) {
            pos = weaponR->world.translate - baseNode->world.translate;
        } else {
            pos = weaponR->world.translate;
        }
        
    } else {
        log::warn("GetPlayerHandPos:Fail to get player node for isLeft:{}", isLeft);
        return RE::NiPoint3();
    }

    auto help = baseNode->world.translate - player->GetPosition();
    log::trace("Diff between baseNode and player pos {}:{}, {}, {}", help.Length(), help.x, help.y, help.z);
    // Diff: 11.251953, 1.1650391, 62.509766.
    // Diff: -9.108398, -13.617371, 64.5376

    result = pos;
    if (minusBase) result.z -= 65.0f;

    return result;
}

RE::NiPoint3 GetPlayerHandPos(bool isLeft, RE::Actor* player) {
    auto playerCh = RE::PlayerCharacter::GetSingleton();
    if (!playerCh) {
        log::error("Can't get player!");

    }
    auto vrData = playerCh->GetVRNodeData();
    const auto weaponL = vrData->NPCLHnd;
    const auto weaponR = vrData->NPCRHnd;
    const auto baseNode = vrData->UprightHmdNode;

    RE::NiPoint3 pos, result;
    if (isLeft && weaponL && baseNode) {
        pos = weaponL->world.translate - baseNode->world.translate;
    } else if (!isLeft && weaponR && baseNode) {
        pos = weaponR->world.translate - baseNode->world.translate;
    } else {
        log::warn("GetPlayerHandPos:Fail to get player node for isLeft:{}", isLeft);
        return RE::NiPoint3();
    }

    auto help = baseNode->world.translate - player->GetPosition();
    log::trace("Diff between hmdNode and player pos {}:{}, {}, {}", help.Length(), help.x, help.y, help.z);



    result = pos;
    result.z += 120.0f; // hmd is 120 above player pos

    return result;
}


bool IsPlayerHandCloseToHead(RE::Actor* player) {
    auto playerCh = RE::PlayerCharacter::GetSingleton();
    if (!playerCh) {
        log::error("Can't get player!");
    }
    auto vrData = playerCh->GetVRNodeData();
    const auto weaponL = vrData->NPCLHnd;
    const auto weaponR = vrData->NPCRHnd;
    const auto baseNode = vrData->UprightHmdNode;

    if (weaponR && weaponL && baseNode) {
        auto diffL = weaponL->world.translate - baseNode->world.translate;
        auto diffR = weaponR->world.translate - baseNode->world.translate;

        if (diffL.Length() < 20.0f || diffR.Length() < 20.0f) return true;
    }

    log::warn("IsPlayerHandCloseToHead:Fail to get player node for ");
    return false;
}

void debug_show_weapon_range(RE::Actor* actor, RE::NiPoint3& posWeaponBottom, RE::NiPoint3& posWeaponTop,
                             RE::NiNode* bone) {
    RE::NiPoint3 P_V = {0.0f, 0.0f, 0.0f};
    if (iFrameCount % 3 == 0 && iFrameCount % 6 != 0) {
        play_impact_2(actor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x0004BB52), &P_V, &posWeaponTop, bone);
    } else if (iFrameCount % 3 == 0 && iFrameCount % 6 == 0) {
        play_impact_2(actor, RE::TESForm::LookupByID<RE::BGSImpactData>(0x0004BB52), &P_V, &posWeaponBottom, bone);
    }
}

RE::NiPoint3 Quad2Velo(RE::hkVector4& a_velocity) {
    return RE::NiPoint3(a_velocity.quad.m128_f32[0], a_velocity.quad.m128_f32[1], a_velocity.quad.m128_f32[2]);
}

float CurrentSpellWheelSlowRatio(RE::Actor* player) {
    float ratio = 1.0f;
    static RE::SpellItem* timeSlowSpell;
    if (!timeSlowSpell) {
        timeSlowSpell = GetTimeSlowSpell_SpeelWheel();
        if (!timeSlowSpell) {
            log::warn("Can't find time slow spell from spellwheel");
            return ratio;
        }
    }

    if (!player->HasSpell(timeSlowSpell)) {
        return ratio;
    }

    for (RE::BSTArrayBase::size_type i = 0; i < timeSlowSpell->effects.size(); i++) {
        auto effect = timeSlowSpell->effects.operator[](i);
        if (!effect) {
            log::trace("TimeSlowEffect: effect[{}] is null", i);
            continue;
        }
        ratio = effect->effectItem.magnitude;
        //log::trace("Magnitude: {}", ratio);
    }

    return ratio;
}

// Get a float conf that is stored in a TESGlobal
float GetFConf(FConf name) {
    static RE::TESGlobal* g;
    if (!g) {
        g = static_cast<RE::TESGlobal*>(GetMyForm(0x0080A3));
    }
    return g->value;
}