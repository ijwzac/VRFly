#include "Settings.h"

using namespace SKSE;
using namespace SKSE::log;

// Global
//bool bEnableWholeMod = true;
bool bShowEmitSpellDirection = false;
int64_t iFrameCount = 0;
int64_t iLastPressGrip = 0;
int iTraceLevel = 0;
std::chrono::steady_clock::time_point last_time;


// Feedback
int64_t iTimeSlowFrameNormal = 12;
int64_t iTimeSlowFrameStop = 25;
int64_t iTimeSlowFrameLargeRecoil = 50;
float fTimeSlowRatio = 0.1f;
int iHapticStrMin = 0;
int iHapticStrMax = 50;
float fHapticMulti = 1.0f;
int iHapticLengthMicroSec = 100000;  // 100 ms

float fMagicNum1 = 0.1f;
float fMagicNum2 = 0.0f;
float fMagicNum3 = 0.0f;


Settings* Settings::GetSingleton() {
    static Settings singleton;
    return std::addressof(singleton);
}

void Settings::Load() {
    constexpr auto path = L"Data/SKSE/Plugins/VRFly.ini";

    CSimpleIniA ini;
    ini.SetUnicode();

    auto err = ini.LoadFile(path);

    sMain.Load(ini);
    sFeedback.Load(ini);
    sTechnique.Load(ini);

    err = ini.SaveFile(path);
}

void Settings::Main::Load(CSimpleIniA& a_ini) { 
    static const char* section = "==========Main==========";

    //detail::get_value(
    //    a_ini, bEnableWholeMod, section, "EnableWholeMod",
    //    ";;;;;;;;;;;;;;;;;;;;;;;;\n; While playing the game, you can DYNAMICALLY CHANGE SETTINGS below. Steps:\n"
    //    ";(1) Edit settings; (2) Save and close this file; (3) When not in combat, open Skyrim console by \"`\"\n"
    //    ";(4) Close the console, no need to type anything; (5) Now settings are updated.\n;;;;;;;;;;;;;;;;;;;;;;;;\n;\n;\n"
    //    "; Set this to false if you want to completely disable this mod. Default:\"true\".");

    
}
void Settings::Feedback::Load(CSimpleIniA& a_ini) {
    static const char* section = "==========2. Parry Feedback==========";

    detail::get_value(
        a_ini, fHapticMulti, section, "HapticStrengthMultiplier",
        "; haptic_strength_on_controller = Stamina_cost_to_player * HapticStrengthMultiplier.\n"
        "; The final haptic_strength_on_controller is modified to fit in [HapticStrengthMin, HapticStrengthMax].\n"
        ";\n"
        "; As shown above, the actual stamina cost multiplies this value is the haptic strength. Default:\"1.0\"");

    detail::get_value(a_ini, iHapticStrMin, section, "HapticStrengthMin",
                      "; The minimal haptic strength. Default:\"10\"");

    detail::get_value(a_ini, iHapticStrMax, section, "HapticStrengthMax",
                      "; The maximal haptic strength. Default:\"50\"");

    detail::get_value(a_ini, iHapticLengthMicroSec, section, "HapticDuration",
                      "; The haptic duration. After configured here, it won't be changed by game events. Unit: "
                      "microsecond. \"100000\" is 0.1 seconds. Default:\"100000\"");

    detail::get_value(
        a_ini, fTimeSlowRatio, section, "TimeSlowRatio",
        ";===If you are a SE/AE player, no need to read the four settings below, because SpellWheelVR is required\n"
        "; After a parry, time scale will be set to this ratio for a few frames. \n"
        "; \"0.1\" means time flows at 10% of normal speed. Default:\"0.1\"");

    detail::get_value(a_ini, iTimeSlowFrameNormal, section, "TimeSlowFrameNormal",
                      "; After a normal parry, for how many frames should time be slow. \n"
                      "; Default:\"12\"");
    detail::get_value(a_ini, iTimeSlowFrameStop, section, "TimeSlowFrameStop",
                      "; After a parry that stops enemy, for how many frames should time be slow. \n"
                      "; Default:\"25\"");
    detail::get_value(a_ini, iTimeSlowFrameLargeRecoil, section, "TimeSlowFrameStagger",
                      "; After a parry that staggers enemy, for how many frames should time be slow. \n"
                      "; Default:\"50\"");
}

void Settings::Technique::Load(CSimpleIniA& a_ini) {
    static const char* section = "==========Other Settings==========";

    detail::get_value(a_ini, iTraceLevel, section, "TraceLevel",
                      "; ===WARNING: normally you don't need to change any value below. Please read instructions "
                      "carefully before any "
                      "change.\n"
                      ";\n"
                      "; Set to 2 to only have info. 1 to turn on debug. 0 to turn on trace. Printing to "
                      "C:\\Users\\XXX\\Documents\\My Games\\Skyrim VR\\SKSE\\WeaponCollision.log\n"
                      "; In the worst case, setting to 0 makes the file grow at 10MB/minute, and might slow the game "
                      "down (very unlikely), but useful when you report bugs to me. Default:\"2\"");

    detail::get_value(
        a_ini, bShowEmitSpellDirection, section, "DebugShowEmitSpellDirection",
        "; Display some effect of the direction when player is casting emit spell\n"
        "; I turn this on to verify if direction calculated by this mod is correct. Default:\"false\"");

     detail::get_value(
         a_ini, fMagicNum1, section, "DebugTuneAngle1",
         "; ===No need to read this option\n"
         "; A number I use to debug weapon angle of special enemies who don't have hands. Default:\"0.0\"");
     detail::get_value(
         a_ini, fMagicNum2, section, "DebugTuneAngle2",
         "; ===No need to read this option\n"
         "; A number I use to debug weapon angle of special enemies who don't have hands. Default:\"0.0\"");
     detail::get_value(
         a_ini, fMagicNum3, section, "DebugTuneAngle3",
         "; ===No need to read this option\n"
         "; A number I use to debug weapon angle of special enemies who don't have hands. Default:\"0.0\"");

}

    
spdlog::level::level_enum TraceLevel(int level) {
    switch (iTraceLevel) {
        case 0:
            return spdlog::level::trace;
        case 1:
            return spdlog::level::debug;
        case 2:
            return spdlog::level::info;
        case 3:
            return spdlog::level::warn;
        case 4:
            return spdlog::level::err;
    }
    return spdlog::level::info;
}

uint32_t GetBaseFormID_Settings(uint32_t formId) { return formId & 0x00FFFFFF; }

uint32_t GetFullFormID_Settings(const uint8_t modIndex, uint32_t formLower) { return (modIndex << 24) | formLower; }

uint32_t GetFullFormID_ESL_Settings(const uint8_t modIndex, const uint16_t esl_index, uint32_t formLower) {
    return (modIndex << 24) | (esl_index << 12) | formLower;
}


RE::TESForm* GetMyForm_Settings(RE::FormID partFormID) {
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
        RE::FormID fullFormID = GetFullFormID_Settings(espIndex.value(), partFormID);
        g = RE::TESForm::LookupByID(fullFormID);
        if (g) return g;
    }

    // Second, try to find the spell using ESL
    // TODO: is this really OK?
    for (uint16_t i = 0; i <= 0xFFF; i++) {
        RE::FormID fullFormID = GetFullFormID_ESL_Settings(0xFE, i, partFormID);
        g = RE::TESForm::LookupByID(fullFormID);
        if (g) return g;
    }

    log::error("GetMyForm: failed to get the form {:x}", partFormID);
    return nullptr;
}

float GetMyConf(RE::FormID partFormID) {
    RE::TESGlobal* g = static_cast<RE::TESGlobal*>(GetMyForm_Settings(partFormID));
    if (!g) {
        log::error("GetMyConf: Failed to find TESGlobal {:x}", partFormID);
        return 0.0f;
    }

    return g->value;
}


bool GetMyBoolConf(RE::FormID partFormID) {
    float x = GetMyConf(partFormID);
    int round = std::round(x);
    return round == 1;
}


int GetMyIntConf(RE::FormID partFormID) {
    float x = GetMyConf(partFormID);
    int round = std::round(x);
    return round;
}