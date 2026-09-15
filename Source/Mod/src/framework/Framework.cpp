#include "Framework.hpp"

#include "Logger.hpp"
#include "../integration/BedrockBuild.hpp"
#include "../integration/ChatCommands.hpp"
#include "../modules/AntiKnockbackModule.hpp"
#include "../modules/CriticalsModule.hpp"
#include "../modules/XrayModule.hpp"
#include "../modules/FullbrightModule.hpp"
#include "../modules/ReachModule.hpp"
#include "../modules/GhostHandModule.hpp"
#include "../modules/SpeedModule.hpp"
#include "../modules/ArrayListModule.hpp"
#include "../modules/CrosshairModule.hpp"
#include "../modules/GameplayModules.hpp"
#include "../modules/BaritoneModule.hpp"

#include <cstdio>
#include <memory>

namespace utility {

Framework& Framework::instance() noexcept {
    static Framework framework;
    return framework;
}

bool Framework::initialize(HMODULE module) noexcept {
    if (eject_requested_.load()) return false;
    bool expected = false;
    if (!initialized_.compare_exchange_strong(expected, true)) return true;

    module_ = module;
    if (!Logger::instance().initialize()) {
        initialized_.store(false);
        return false;
    }

    Logger::instance().info("BedrockUtilityFramework loaded successfully.");
    {
        const auto build = integration::current_bedrock_build();
        char message[160]{};
        if (build.image != nullptr) {
            std::snprintf(message, sizeof(message),
                "Minecraft executable profile: PE timestamp 0x%08X, SizeOfImage 0x%08X%s.",
                static_cast<unsigned>(build.timestamp), static_cast<unsigned>(build.image_size),
                integration::is_legacy_12645_build(build) ? " (legacy 1.26.45 profile)" : " (unvalidated profile)");
        } else {
            std::snprintf(message, sizeof(message), "Minecraft executable profile could not be read.");
        }
        Logger::instance().info(message);
    }

    if (splash_text_hook_.install()) Logger::instance().info("Native splash-text hook installed; replacement is 'made by Roundomegaboi'.");
    else Logger::instance().info("Native splash-text hook not installed: host/build/signature is unsupported.");

    config_.load_defaults();
    auto entity_reach = std::make_unique<modules::ReachModule>();
    auto* reach = entity_reach.get();
    modules_.add(std::move(entity_reach));
    modules_.add(std::make_unique<modules::BlockReachModule>(*reach));
    modules_.add(std::make_unique<modules::GhostHandModule>());
    modules_.add(std::make_unique<modules::AntiKnockbackModule>());
    modules_.add(std::make_unique<modules::CriticalsModule>());
    modules_.add(std::make_unique<modules::XrayModule>());
    modules_.add(std::make_unique<modules::FullbrightModule>());
    modules_.add(std::make_unique<modules::CrosshairModule>());
    modules_.add(std::make_unique<modules::SpeedModule>());
    modules_.add(std::make_unique<modules::ArrayListModule>());
    for (unsigned i=0; i<static_cast<unsigned>(modules::GameplayFeature::count); ++i)
        modules_.add(std::make_unique<modules::GameplayModule>(static_cast<modules::GameplayFeature>(i)));
    modules_.add(std::make_unique<modules::BaritoneModule>());
    modules_.initialize(events_);
    if (renderer_.initialize(module_, modules_)) Logger::instance().info("Click menu initialized; Tab opens, left click toggles, right click opens settings.");
    else Logger::instance().info("Menu overlay initialization failed.");
    modules_.commands().set_eject_handler([this]{return request_eject();});
    Logger::instance().info(integration::install_chat_commands(modules_)
        ? "Local comma commands installed: ,help, ,keybind, ,unbind and ,eject."
        : "Local commands unavailable: unsupported chat signature or hook installation failed.");
    return true;
}

void Framework::shutdown() noexcept {
    if (!initialized_.exchange(false)) return;
    integration::stop_chat_commands(true);
    renderer_.shutdown();
    splash_text_hook_.uninstall();
    modules_.deactivate();
    Logger::instance().info("BedrockUtilityFramework shutting down.");
    Logger::instance().shutdown();
    module_ = nullptr;
}

bool Framework::request_eject() noexcept {
    bool expected=false;
    if (!initialized_.load() || !eject_requested_.compare_exchange_strong(expected,true)) return false;
    const auto thread=CreateThread(nullptr,0,[](void* context)->DWORD {
        static_cast<Framework*>(context)->shutdown();
        return 0;
    },this,0,nullptr);
    if (!thread) {eject_requested_=false;return false;}
    CloseHandle(thread);
    return true;
}

bool Framework::initialized() const noexcept { return initialized_.load(); }

} // namespace utility
