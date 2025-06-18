#include "weapon.hpp"

#include "game.hpp"
#include "gfx/engine.hpp"
#include "gfx/mesh.hpp"
#include "network/network.hpp"
#include "resource.hpp"
#include "wplayer.hpp"
namespace ww {
Weapon::Weapon(net::NetworkManager* manager, net::EntityId id)
    : net::Entity(manager, id) {
  ownerRef = NULL;
}

void Weapon::primaryFire() {}

void Weapon::secondaryFire() {}

bool Weapon::getOwnership(rdm::network::Peer* peer) {
  return peer->playerEntity == getOwnerRef();
}

void Weapon::renderView() {
  if (viewModel.empty()) return;
  if (!ownerRef) return;

  using namespace rdm;

  Graph::Node node;
  node.parent = ownerRef->getNode();
  node.origin = glm::vec3(-1, -2, 2);

  resource::Model* model =
      getGame()->getResourceManager()->load<resource::Model>(viewModel.c_str());
  model->render(getGfxEngine()->getDevice(), NULL, NULL,
                [&node](gfx::BaseProgram* bp) {
                  bp->setParameter("model", rdm::gfx::DtMat4,
                                   {.matrix4x4 = node.worldTransform()});
                });
}

void Weapon::renderWorld() {
  if (worldModel.empty()) return;
  if (!ownerRef) return;

  using namespace rdm;

  Graph::Node node;
  node.parent = ownerRef->getNode();
  node.origin = glm::vec3(-1, -2, 2);

  resource::Model* model =
      getGame()->getResourceManager()->load<resource::Model>(
          worldModel.c_str());
  model->render(getGfxEngine()->getDevice(), NULL, NULL,
                [&node](gfx::BaseProgram* bp) {
                  bp->setParameter("model", rdm::gfx::DtMat4,
                                   {.matrix4x4 = node.worldTransform()});
                });
}
};  // namespace ww
