#pragma once
#include "../weapon.hpp"
#include "network/network.hpp"

namespace ww {
class WeaponMagnum : public Weapon {
 public:
  WeaponMagnum(net::NetworkManager* manager, net::EntityId id);

  static void precache(net::NetworkManager* manager);

  virtual const char* getTypeName() { return "WeaponMagnum"; };
};
}  // namespace ww
