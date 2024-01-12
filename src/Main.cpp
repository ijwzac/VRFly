/*
For those who are looking at the source code, feel free to use any code or assets in this project.
In fact, the code you see here is based on many other projects and I received help from many modders.
*/


#include <stddef.h>
#include "OnMeleeHit.h"
#include "Settings.h"
#include "Utils.h"
#include "OnFrame.h"

using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

ZacOnFrame::SpeedRing ZacOnFrame::speedBuf = ZacOnFrame::SpeedRing(90);
ZacOnFrame::SlowTimeEffect ZacOnFrame::slowTimeData = ZacOnFrame::SlowTimeEffect(0);

namespace {
    /**
     * Setup logging.
     */
    void InitializeLogging() {
        auto path = log_directory();
        if (!path) {
            report_and_fail("Unable to lookup SKSE logs directory.");
        }
        *path /= PluginDeclaration::GetSingleton()->GetName();
        *path += L".log";

        std::shared_ptr<spdlog::logger> log;
        if (IsDebuggerPresent()) {
            log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::msvc_sink_mt>());
        } else {
            log = std::make_shared<spdlog::logger>(
                "Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
        }

        const auto level = spdlog::level::info;

        log->set_level(level);
        log->flush_on(level);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");

    }

    /**
     * Initialize the hooks.
     */
    void InitializeHooks() {
        
        log::info("About to hook frame update");

        ZacOnFrame::InstallFrameHook();

        log::trace("Hooks initialized.");
    }

    void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
        log::trace("MessageHandler called");
        switch (a_msg->type) {
            case SKSE::MessagingInterface::kInputLoaded: {
                auto& eventProcessor = EventProcessor::GetSingleton();
                RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&eventProcessor);
            }
            case SKSE::MessagingInterface::kPostPostLoad: {
                /*log::info("kPostPostLoad");
                */
            }
                break;
            case SKSE::MessagingInterface::kPreLoadGame: {
                log::info("kPreLoadGame");
                ZacOnFrame::CleanBeforeLoad();
            } break;
            case SKSE::MessagingInterface::kDataLoaded: {
                log::info("kDataLoaded"); 
                auto planckHandle = GetModuleHandleA("activeragdoll.dll");
                if (planckHandle) {
                    logger::info("Planck loaded");
                }
            }
                break;
            
        }
    }
}  // namespace



/**
 * This is the main callback for initializing the SKSE plugin, called just before Skyrim runs its main function.
 */
SKSEPluginLoad(const LoadInterface* skse) {
    InitializeLogging();

    auto* plugin = PluginDeclaration::GetSingleton();
    auto version = plugin->GetVersion();
    log::info("{} {} is loading...", plugin->GetName(), version);

    Init(skse);

    try {
        Settings::GetSingleton()->Load();
    } catch (...) {
        logger::error("Exception caught when loading settings! Default settings will be used");
    }

    spdlog::level::level_enum level = TraceLevel(iTraceLevel);
    spdlog::default_logger()->set_level(level);

    if (bEnableWholeMod == false) {
        log::info("{} is disabled.", plugin->GetName());
        return true;
    }

    log::info("Init data struct");

    InitializeHooks();

    SKSE::GetMessagingInterface()->RegisterListener(MessageHandler);

    log::info("Registered main hooks. About to register Menu Open");
    auto& eventProcessor = EventProcessor::GetSingleton();
    RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&eventProcessor);

    log::info("{} has finished loading.", plugin->GetName());
    return true;
}
