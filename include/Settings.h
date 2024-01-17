#pragma once

// Whole mod
extern bool bEnableWholeMod;

// Global
extern int64_t iFrameCount;
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


        }

        return RE::BSEventNotifyControl::kContinue;
    }
};
