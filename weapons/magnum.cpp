#include "magnum.hpp"

#include "game.hpp"
#include "network/network.hpp"
namespace ww {
WeaponMagnum::WeaponMagnum(net::NetworkManager* manager, net::EntityId id)
    : Weapon(manager, id) {
  viewModel = "rdm/weapons/w_magnum.glb";
  worldModel = "rdm/weapons/w_magnum.glb";
}

void WeaponMagnum::precache(net::NetworkManager* manager) {
  manager->getGame()->getResourceManager()->load<rdm::resource::Model>(
      "rdm/weapons/w_magnum.glb");
}
}  // namespace ww
