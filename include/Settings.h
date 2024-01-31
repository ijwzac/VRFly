#pragma once

// Whole mod
//extern bool bEnableWholeMod;

// Global
extern int64_t iFrameCount;
extern int64_t iLastPressGrip;
extern std::chrono::steady_clock::time_point last_time;

extern float fTimeSlowRatio; // 0.1 means time flows at 10% of normal time
extern int64_t iTimeSlowFrameNormal; // for how many frames will time be slow
extern int64_t iTimeSlowFrameStop;
extern int64_t iTimeSlowFrameLargeRecoil;

// Haptic feedback
extern int iHapticStrMin;
extern int iHapticStrMax;
extern float fHapticMulti;  // same logic above. Haptic decided by stamina cost
extern int iHapticLengthMicroSec; 

// Settings for internal usage. Don't change unless you understand the code
extern int iTraceLevel;           // Turn on the trace
extern bool bShowEmitSpellDirection;  // For debug. This shows the emit spell direction in this mod. May hurt your eyes

extern float fMagicNum1;
extern float fMagicNum2;
extern float fMagicNum3;

spdlog::level::level_enum TraceLevel(int level);


// Thanks to: https://github.com/powerof3/CLibUtil
namespace string {
    template <class T>
    T to_num(const std::string& a_str, bool a_hex = false) {
        const int base = a_hex ? 16 : 10;

        if constexpr (std::is_same_v<T, double>) {
            return static_cast<T>(std::stod(a_str, nullptr));
        } else if constexpr (std::is_same_v<T, float>) {
            return static_cast<T>(std::stof(a_str, nullptr));
        } else if constexpr (std::is_same_v<T, std::int64_t>) {
            return static_cast<T>(std::stol(a_str, nullptr, base));
        } else if constexpr (std::is_same_v<T, std::uint64_t>) {
            return static_cast<T>(std::stoull(a_str, nullptr, base));
        } else if constexpr (std::is_signed_v<T>) {
            return static_cast<T>(std::stoi(a_str, nullptr, base));
        } else {
            return static_cast<T>(std::stoul(a_str, nullptr, base));
        }
    }

    inline std::vector<std::string> split(const std::string& a_str, std::string_view a_delimiter) {
        auto range = a_str | std::ranges::views::split(a_delimiter) |
                     std::ranges::views::transform([](auto&& r) { return std::string_view(r); });
        return {range.begin(), range.end()};
    }

    // https://stackoverflow.com/a/35452044
    inline std::string join(const std::vector<std::string>& a_vec, std::string_view a_delimiter) {
        return std::accumulate(a_vec.begin(), a_vec.end(), std::string{},
                               [a_delimiter](const auto& str1, const auto& str2) {
                                   return str1.empty() ? str2 : str1 + a_delimiter.data() + str2;
                               });
    }
};  // namespace string

class Settings {
public:
    [[nodiscard]] static Settings* GetSingleton();

    void Load();

    struct Main {
        void Load(CSimpleIniA& a_ini);
    } sMain;

    struct Feedback {
        void Load(CSimpleIniA& a_ini);
    } sFeedback;

    
    struct Technique {
        void Load(CSimpleIniA& a_ini);
    } sTechnique;

private:
    Settings() = default;
    Settings(const Settings&) = delete;
    Settings(Settings&&) = delete;
    ~Settings() = default;

    Settings& operator=(const Settings&) = delete;
    Settings& operator=(Settings&&) = delete;

    struct detail {

        // Thanks to: https://github.com/powerof3/CLibUtil
        template <class T>
        static T& get_value(CSimpleIniA& a_ini, T& a_value, const char* a_section, const char* a_key, const char* a_comment,
                            const char* a_delimiter = R"(|)") {
            if constexpr (std::is_same_v<T, bool>) {
                a_value = a_ini.GetBoolValue(a_section, a_key, a_value);
                a_ini.SetBoolValue(a_section, a_key, a_value, a_comment);
            } else if constexpr (std::is_floating_point_v<T>) {
                a_value = static_cast<float>(a_ini.GetDoubleValue(a_section, a_key, a_value));
                a_ini.SetDoubleValue(a_section, a_key, a_value, a_comment);
            } else if constexpr (std::is_enum_v<T>) {
                a_value = string::template to_num<T>(
                    a_ini.GetValue(a_section, a_key, std::to_string(std::to_underlying(a_value)).c_str()));
                a_ini.SetValue(a_section, a_key, std::to_string(std::to_underlying(a_value)).c_str(), a_comment);
            } else if constexpr (std::is_arithmetic_v<T>) {
                a_value = string::template to_num<T>(a_ini.GetValue(a_section, a_key, std::to_string(a_value).c_str()));
                a_ini.SetValue(a_section, a_key, std::to_string(a_value).c_str(), a_comment);
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                a_value = string::split(a_ini.GetValue(a_section, a_key, string::join(a_value, a_delimiter).c_str()),
                                        a_delimiter);
                a_ini.SetValue(a_section, a_key, string::join(a_value, a_delimiter).c_str(), a_comment);
            } else {
                a_value = a_ini.GetValue(a_section, a_key, a_value.c_str());
                a_ini.SetValue(a_section, a_key, a_value.c_str(), a_comment);
            }
            return a_value;
        }
    };
};

// Code from: https://www.youtube.com/watch?v=afGRuSM2IIc
class EventProcessor : public RE::BSTEventSink<RE::MenuOpenCloseEvent>, public RE::BSTEventSink<RE::InputEvent*> {
    // Pretty typical singleton setup
    // *Private* constructor/destructor
    // And we *delete* the copy constructors and move constructors.
    EventProcessor() = default;
    ~EventProcessor() = default;
    EventProcessor(const EventProcessor&) = delete;
    EventProcessor(EventProcessor&&) = delete;
    EventProcessor& operator=(const EventProcessor&) = delete;
    EventProcessor& operator=(EventProcessor&&) = delete;

public:
    // Returns a reference to the one and only instance of EventProcessor :)
    //
    // Note: this is returned as a & reference. When you need this as a pointer, you'll want to use & (see below)
    static EventProcessor& GetSingleton() {
        static EventProcessor singleton;
        return singleton;
    }

    // Log information about Menu open/close events that happen in the game
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        // logger::trace("Menu {} Open? {}", event->menuName, event->opening); 
        if (event->menuName == "Console"sv && event->opening == false) {
            logger::trace("Console close. Now reload config"); 
            Settings::GetSingleton()->Load();
            spdlog::level::level_enum level = TraceLevel(iTraceLevel);
            spdlog::default_logger()->set_level(level);
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* eventPtr, RE::BSTEventSource<RE::InputEvent*>*) {
        if (!eventPtr) return RE::BSEventNotifyControl::kContinue;

        auto* event = *eventPtr;
        if (!event) return RE::BSEventNotifyControl::kContinue;

        if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
            auto* buttonEvent = event->AsButtonEvent();
            auto dxScanCode = buttonEvent->GetIDCode();
            //logger::trace("Pressed key {}", dxScanCode);

            if (dxScanCode == 2) iLastPressGrip = iFrameCount;

        }

        return RE::BSEventNotifyControl::kContinue;
    }
};

enum class FConf {
    MaxVeloZ,
    MaxVeloLength,
};

uint32_t GetBaseFormID_Settings(uint32_t formId);

uint32_t GetFullFormID_Settings(const uint8_t modIndex, uint32_t formLower);

uint32_t GetFullFormID_ESL_Settings(const uint8_t modIndex, const uint16_t esl_index, uint32_t formLower);

RE::TESForm* GetMyForm_Settings(RE::FormID partFormID);


// Settings read from Global in game
// Main
const RE::FormID bEnableWholeMod = 0x36DCC;


// Speed and force of wings
const RE::FormID fMaxSpeed = 0x2B27E;
const RE::FormID fMaxSpeedZ = 0x2B27F;
const RE::FormID fMaxLiftXY = 0x2B280;
const RE::FormID fMaxLiftZ = 0x33F00;
const RE::FormID fLiftMaintainer = 0x33F01;
const RE::FormID fMaxDrag = 0x2B281;
const RE::FormID fMaxHelper = 0x2B282;
const RE::FormID fMultiSpeed = 0x2B285;
const RE::FormID fMultiLiftXY = 0x2B286;
const RE::FormID fMultiLiftZ = 0x3CB63;
const RE::FormID fLiftQuadCoef = 0x3CB64;
const RE::FormID fLiftLinearCoef = 0x3CB65;
const RE::FormID fMultiDrag = 0x2B287;
const RE::FormID fMultiHelper = 0x2B288;
const RE::FormID fGravity = 0x2B289;

// Wings flap
const RE::FormID fFlapStrength = 0x2B290;
const RE::FormID fFlapStaminaCost = 0x39C95;
const RE::FormID fMultiFlapStaminaCost = 0x39C96;
const RE::FormID fFlapStaminaReduceTime = 0x39C97;
const RE::FormID fFlapThres = 0x3CB67;

// Wings effects
const RE::FormID fShockSmThres = 0x2B29C;
const RE::FormID fShockMidThres = 0x2B29D;
const RE::FormID fShockLgThres = 0x2B29E;
const RE::FormID fWindSmThres = 0x2B298;
const RE::FormID fWindLgThres = 0x2B299;
const RE::FormID fWindExThres = 0x2B29A;
const RE::FormID lWindInterval = 0x2B29B;
const RE::FormID lWindIntervalLg = 0x2B2A1;

// Wings perks
const RE::FormID bEnableShockwave = 0x2B29F;
const RE::FormID bEnableShockwaveSlowMotion = 0x2B29A0;
const RE::FormID bEnableSkydiving = 0x2B284;
const RE::FormID fSpiritualLiftStrength = 0x2B292;
const RE::FormID fFireLiftStrength = 0x2B293;
const RE::FormID lSpiritualLiftRadius = 0x2B296;
const RE::FormID lFireLiftRadius = 0x2B297;

// Spell Velo
const RE::FormID fSPVeloGravity = 0x2B291;
const RE::FormID fSPVeloIdleLifetime = 0x2B294;
const RE::FormID fSPVeloLength = 0x2B28D;
const RE::FormID fSPVeloMaxZ = 0x2B28E;
const RE::FormID fSPVeloSmoothTime = 0x2B28F;
const RE::FormID fSPForceIdleLifetime = 0x2B295;
const RE::FormID fSPForceMaxXY = 0x2B28C;
const RE::FormID fSPForceStrength = 0x2B28A;
const RE::FormID fSPForceStrengthSecondHand = 0x2B28B;

// Other
const RE::FormID fShoulderHeight = 0x2B283;
const RE::FormID bEnableNotification = 0x33F02;

float GetMyConf(RE::FormID partFormID);
bool GetMyBoolConf(RE::FormID partFormID);
int GetMyIntConf(RE::FormID partFormID);