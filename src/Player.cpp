#include "Player.h"

//void HookSetVelocity(RE::bhkCharProxyController* controller, RE::hkVector4& a_velocity);
//static REL::Relocation<decltype(HookSetVelocity)> _SetVelocity;
//
//void InstallHook() {
//
//    // ProxyVTable's address is 0x182fef8, same as yours
//    REL::Relocation<std::uintptr_t> ProxyVTable{RE::VTABLE_bhkCharProxyController[1]};
//
//    // Now we need to overwrite function 0x07 in RE/B/bhkCharProxyController.h, which is:
//    // "void SetLinearVelocityImpl(const hkVector4& a_velocity) override;  // 07"
//    // Thanks to the author of PLANCK, this is discovered to be where proxy controller that controls the movement of
//    // player and dragons sets the velocity
//    _SetVelocity = ProxyVTable.write_vfunc(0x07, HookSetVelocity);
//}
//
//void HookSetVelocity(RE::bhkCharProxyController* controller, RE::hkVector4& a_velocity) {
//    _SetVelocity(controller, a_velocity);
//
//    // In commonlib, `proxyController->proxy` is of type `bhkCharacterProxy`
//    // However, unlike your code, it doesn't have member `characterProxy`. It only has one member:
//    // "bhkCharacterPointCollector ignoredCollisionStartCollector;  // 020" -- From RE/B/bhkCharacterProxy.h
//
//    // Thus, I access controller->GetCharacterProxy(), which returns a `hkpCharacterProxy*`
//    if (auto proxy = controller->GetCharacterProxy(); proxy) {
//        // The next line is fine. The output is 0.0
//        log::trace("{}", proxy->dynamicFriction); // dynamicFriction is a float
//
//        // The next line that just reads velocity crashes the game
//        proxy->velocity; // velocity is a hkVector4
//
//        // The next line also crashes the game
//        proxy->velocity = RE::hkVector4();
//    }
//}

