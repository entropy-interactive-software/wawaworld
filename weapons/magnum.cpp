#include "magnum.hpp"

#include "game.hpp"
#include "network/network.hpp"
namespace ww {
WeaponMagnum::WeaponMagnum(net::NetworkManager* manager, net::EntityId id)
    : Weapon(manager, id) {
  viewModel = "dat5/weapons/w_magnum.glb";
  worldModel = "dat5/weapons/w_magnum.glb";
}

void WeaponMagnum::precache(net::NetworkManager* manager) {
  manager->getGame()->getResourceManager()->load<rdm::resource::Model>(
      "dat5/weapons/w_magnum.glb");
}
}  // namespace ww
