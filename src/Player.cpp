#include "Player.h"

//void HookGetVelocity(RE::bhkCharProxyController* controller, RE::hkVector4& a_velocity);
//
//static REL::Relocation<decltype(HookGetVelocity)> _GetVelocity;
//
//void InstallHook() {
//    REL::Relocation<std::uintptr_t> ProxyVTable{RE::VTABLE_bhkCharProxyController[1]};
//    // Now we need to overwrite function 0x6 in RE/B/bhkCharProxyController.h, which is:
//    // "void GetLinearVelocityImpl(hkVector4& a_velocity) const override;  // 06"
//    // Thanks to the author of PLANCK, this is discovered to be where proxy controller gets the velocity
//    _GetVelocity = ProxyVTable.write_vfunc(0x6, HookGetVelocity);
//}
//
//void HookGetVelocity(RE::bhkCharProxyController* controller, RE::hkVector4& a_velocity) {
//    _GetVelocity(controller, a_velocity);
//    a_velocity.quad.m128_f32[2] = 5.0f;
//}