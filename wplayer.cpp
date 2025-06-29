#include "wplayer.hpp"

#include <cmath>
#include <cstdio>
#include <glm/ext/matrix_transform.hpp>
#include <memory>

#include "console.hpp"
#include "fun.hpp"
#include "gfx/base_types.hpp"
#include "gfx/engine.hpp"
#include "gfx/imgui/imgui.h"
#include "gfx/material.hpp"
#include "gfx/mesh.hpp"
#include "input.hpp"
#include "logging.hpp"
#include "network/entity.hpp"
#include "physics.hpp"
#include "putil/fpscontroller.hpp"
#include "settings.hpp"
#include "sound.hpp"
#include "wgame.hpp"
#include "world.hpp"
#include "worldspawn.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#define PLAYERMODEL "rdm/models/playermodel_rdm.glb"

static rdm::CVar desired_fov("desired_fov", "75.0", CVARF_SAVE);

namespace gfx = rdm::gfx;
namespace ww {
class PlayerEntity : public gfx::Entity {
  WPlayer* player;
  gfx::Model* playerModel;

 public:
  PlayerEntity(WPlayer* player, gfx::Model* model, rdm::Graph::Node* node)
      : gfx::Entity(node) {
    this->player = player;
    this->playerModel = model;
  }
  virtual void renderTechnique(gfx::BaseDevice* device, int id) {
    playerModel->render(device);
  }
};

#define UI_FONT "engine/gui/eras.ttf", 32

class PlayerWeaponsUI : public rdm::gfx::gui::NGui {
  std::map<std::string, resource::Texture*> weaponTextures;
  gfx::gui::Font* font;

 public:
  PlayerWeaponsUI(gfx::gui::NGuiManager* gui, gfx::Engine* engine)
      : NGui(gui, engine) {
    font = gui->getFontCache()->get(UI_FONT);
    /*    weaponTextures["WeaponSniper"] =
          getGame()->getResourceManager()->load<resource::Texture>("");*/
  }

  virtual void render(gfx::gui::NGuiRenderer* renderer) {
    if (!getGame()
             ->getWorld()
             ->getNetworkManager()
             ->getLocalPeer()
             .playerEntity)
      return;
    renderer->setColor(glm::vec3(1.f));
    WPlayer* player = dynamic_cast<WPlayer*>(getGame()
                                                 ->getWorld()
                                                 ->getNetworkManager()
                                                 ->getLocalPeer()
                                                 .playerEntity);
    if (player->getStatus() != WPlayer::InGame) return;

    glm::vec2 res = getEngine()->getTargetResolution();

    gfx::Camera& cam = getEngine()->getCamera();
    glm::vec4 p(1.f, 1.f, 1.f, 1.0f);

    std::vector<Weapon*> weapons = player->getOwnedWeapons();
    int xoff = 0;
    for (Weapon* weapon : weapons) {
      xoff += renderer
                  ->text(glm::ivec2(xoff, 0.0), font, 0, "%s",
                         weapon->getTypeName())
                  .first;
      for (auto position : weapon->positions) {
        renderer->image(getEngine()->getWhiteTexture(),
                        getEngine()->getCurrentViewport()->project(position),
                        glm::vec2(10, 10));
      }
    }
  }
};

NGUI_INSTANTIATOR(PlayerWeaponsUI);

#ifndef NDEBUG
static CVar pstui_change_position("pstui_change_position", "0");
#endif

class PlayerStatusUI : public rdm::gfx::gui::NGui {
  gfx::gui::Font* font;
  resource::Texture* healthImage;
  resource::Texture* armorImage;
  resource::Model* playerModel;
  std::unique_ptr<resource::Model::Animator> playerModelAnimator;
  std::unique_ptr<gfx::Viewport> playerViewport;

  glm::vec3 target;
  glm::vec3 eye;

 public:
  PlayerStatusUI(gfx::gui::NGuiManager* gui, gfx::Engine* engine)
      : NGui(gui, engine) {
    font = gui->getFontCache()->get(UI_FONT);

    healthImage = getGame()->getResourceManager()->load<resource::Texture>(
        "rdm/gui/health.png");
    armorImage = getGame()->getResourceManager()->load<resource::Texture>(
        "rdm/gui/armor.png");
    playerModel =
        getGame()->getResourceManager()->load<resource::Model>(PLAYERMODEL);
    playerModelAnimator.reset(new resource::Model::Animator());
    playerModelAnimator->initBuffer(engine->getDevice());
    playerModelAnimator->speed = 1.f;

    gfx::ViewportGfxSettings settings = gfx::ViewportGfxSettings();
    settings.resolution = glm::ivec2(128, 64);
    settings.format = rdm::gfx::BaseTexture::RGBA8;
    playerViewport.reset(new gfx::Viewport(engine, settings));

    eye = glm::vec3(0, 2.709, 1.709);
    target = glm::vec3(0, 0, 2.409);
  }

  virtual void render(gfx::gui::NGuiRenderer* renderer) {
    if (!getGame()
             ->getWorld()
             ->getNetworkManager()
             ->getLocalPeer()
             .playerEntity)
      return;

    WPlayer* player = dynamic_cast<WPlayer*>(getGame()
                                                 ->getWorld()
                                                 ->getNetworkManager()
                                                 ->getLocalPeer()
                                                 .playerEntity);
    if (player->getStatus() != WPlayer::InGame) return;

    glm::vec2 res = getEngine()->getTargetResolution();

    gfx::Camera& camera = playerViewport->getCamera();
    camera.setFOV(45.f);
    camera.setUp(glm::vec3(0.f, 0.f, 1.0));
    camera.setNear(0.1f);
    camera.setFar(1000.f);
    camera.setPosition(eye);
    camera.setTarget(target);

#ifndef NDEBUG
    if (pstui_change_position.getBool()) {
      ImGui::Begin("HI");
      ImGui::SliderFloat("Eye Y", &eye.y, 0.f, 200.f);
      ImGui::SliderFloat("Eye Z", &eye.z, 0.f, 200.f);
      ImGui::SliderFloat("Target Z", &target.z, 0.f, 200.f);
      ImGui::End();
    }
#endif

    void* _ = getEngine()->setViewport(playerViewport.get());
    getEngine()->getDevice()->clear(0.f, 0.f, 0.f, 0.f);
    getEngine()->getDevice()->clearDepth();
    playerModelAnimator->animation =
        playerModel->getAnimation("idle_animation");
    playerModel->updateAnimator(getEngine(), playerModelAnimator.get());
    playerModel->render(getEngine()->getDevice(), playerModelAnimator.get(),
                        NULL, [](gfx::BaseProgram* bp) {
                          bp->setParameter(
                              "model", rdm::gfx::DtMat4,
                              {.matrix4x4 = glm::identity<glm::mat4>()});
                          bp->setParameter("sun_direction", rdm::gfx::DtVec3,
                                           {.vec3 = glm::vec3(0.f, 0.f, 1.f)});
                        });
    getEngine()->finishViewport(_);

    renderer->setColor(glm::vec3(0.843, 0.482, 0.729));
    renderer->image(healthImage->getTexture(), glm::vec2(0, 0),
                    glm::vec2(32, 32));
    renderer->text(glm::ivec2(32, 0), font, 0, "%0.0f", 100.f);
    renderer->image(armorImage->getTexture(), glm::vec2(200, 0),
                    glm::vec2(32, 32));
    renderer->text(glm::ivec2(232, 0), font, 0, "%0.0f", 100.f);

    renderer->setColor(glm::vec3(1));
    renderer->image(playerViewport->get(), glm::vec2(res.x / 2 - 64, 0),
                    glm::vec2(128, 64));
  }
};

NGUI_INSTANTIATOR(PlayerStatusUI);

static CVar cl_showpos("cl_showpos", "0", CVARF_SAVE);

std::map<int, std::string> WPlayer::weaponIds = {
    {1, "WeaponSniper"},
    {2, "WeaponMagnum"},
};

void WPlayer::listWeapons() {
  for (auto weapon : ownedWeapons) {
    Log::printf(LOG_INFO, "Weapon %s", weapon->getTypeName());
  }
}

std::string WPlayer::getEntityInfo() {
  std::string r = Player::getEntityInfo();
  r += "\nWeapons:\n";
  for (auto weapon : ownedWeapons) {
    r += std::string(weapon->getTypeName()) + " " +
         std::to_string(weapon->getEntityId()) + "\n";
  }
  return r;
}

static ConsoleCommand player_weapons(
    "player_weapons", "player_weapons", "list all weapons",
    [](Game* game, ConsoleArgReader reader) {
      if (!game->getWorld()) throw std::runtime_error("No local world");
      WPlayer* player = dynamic_cast<WPlayer*>(
          game->getWorld()->getNetworkManager()->getLocalPeer().playerEntity);
      if (player) {
        player->listWeapons();
      }
    });

static ConsoleCommand player_give_weapon(
    "player_give_weapon", "player_give_weapon [peer_id] [class]",
    "give weapon to player with peer_id",
    [](Game* game, ConsoleArgReader reader) {
      if (!game->getServerWorld()) throw std::runtime_error("Must be hosting");
      int peerId = std::atoi(reader.next().c_str());
      net::Peer* peer =
          game->getServerWorld()->getNetworkManager()->getPeerById(peerId);
      if (!peer) throw std::runtime_error("Invalid peer_id");
      std::string className = reader.next();
      WPlayer* player = dynamic_cast<WPlayer*>(peer->playerEntity);
      Weapon* weapon = dynamic_cast<Weapon*>(
          game->getServerWorld()->getNetworkManager()->instantiate(className));
      if (!weapon) throw std::runtime_error("Invalid class");
      player->giveWeapon(weapon);
    });

WPlayer::WPlayer(net::NetworkManager* manager, net::EntityId id)
    : Player(manager, id) {
  controller.reset(
      new rdm::putil::FpsController(manager->getWorld()->getPhysicsWorld()));
  controller->setUser(this);
  controller->setLocalPlayer(false);
  entityNode = new rdm::Graph::Node();
  entityNode->scale = glm::vec3(6.f);
  wantedWeaponId = getManager()->isBackend() ? -1 : 1;
  heldWeaponRef = NULL;
  status = Spectator;

  resource::Model* model =
      getGame()->getResourceManager()->load<resource::Model>(PLAYERMODEL);
  resource::Model::Animator* animator = new resource::Model::Animator();

  firingState[0] = false;
  firingState[1] = false;
  if (!getManager()->isBackend()) {
    soundEmitter.reset(getGame()->getSoundManager()->newEmitter());
    soundEmitter->node = entityNode;
    soundEmitter->play(getGame()
                           ->getSoundManager()
                           ->getSoundCache()
                           ->get("rdm/walking.ogg")
                           .value());
    soundEmitter->setPitch(0.f);
    soundEmitter->setLooping(true);

    if (isLocalPlayer()) getManager()->addPendingUpdate(getEntityId());

    worldJob = getWorld()->stepped.listen([this] {
      if (isLocalPlayer()) {
        getGfxEngine()->getCamera().setFOV(
            std::clamp(desired_fov.getFloat(), 30.f, 90.f));
        controller->updateCamera(getGfxEngine()->getCamera());
      }
    });
    gfxJob = getGfxEngine()->renderStepped.listen([this, model, animator] {
      {
        std::scoped_lock lock(getWorld()->getPhysicsWorld()->mutex);
        btTransform transform;
        controller->getMotionState()->getWorldTransform(transform);
        entityNode->origin =
            rdm::BulletHelpers::fromVector3(transform.getOrigin());
        entityNode->basis = rdm::BulletHelpers::fromMat3(transform.getBasis()) *
                            glm::mat3(glm::rotate(M_PI_2f, glm::vec3(0, 0, 1)));
      }
      Worldspawn* worldspawn = dynamic_cast<Worldspawn*>(
          getManager()->findEntityByType("Worldspawn"));

      if (isLocalPlayer() && cl_showpos.getBool()) {
        ImGui::Begin("Debug");
        if (worldspawn && worldspawn->getFile()) {
          ImGui::Text("Cluster: %i", worldspawn->getFile()->getVisCluster());
          ImGui::Text("Rendered Faces: %i",
                      worldspawn->getFile()->getFacesRendered());
          ImGui::Text("Rendered Leafs: %i",
                      worldspawn->getFile()->getLeafsRendered());

          btVector3 vel = controller->getRigidBody()->getLinearVelocity();
          ImGui::Text("Velocity: %0.2f, %0.2f, %0.2f", vel.x(), vel.y(),
                      vel.z());
          ImGui::Text("Velocity Length: %0.2f (%0.2f)", vel.length(),
                      glm::length((glm::vec2){vel.x(), vel.y()}));
          glm::vec2 accel = controller->getWishDir();
          ImGui::Text("Wish Dir: %0.2f, %0.2f", accel.x, accel.y);
        } else {
          ImGui::Text("No worldspawn found/map not loaded");
        }

        btTransform transform = controller->getTransform();
        btVector3 origin = transform.getOrigin();
        ImGui::Text("Position: %0.2f, %0.2f, %0.2f", origin.x(), origin.y(),
                    origin.z());

        ImGui::Separator();

        network::Peer& peer = getManager()->getLocalPeer();
        ImGui::Text("Round trip time: %i ±%ims", peer.peer->roundTripTime,
                    peer.peer->roundTripTimeVariance);
        ImGui::Text("Packet loss: %i ±%i", peer.peer->packetLoss,
                    peer.peer->packetLossVariance);
        ImGui::Text("Packets sent: %i, lost: %i", peer.peer->packetsSent,
                    peer.peer->packetsLost);

        ImGui::End();

        controller->imguiDebug();
      }

      // if (getManager()->getLocalPeer().peerId == remotePeerId.get()) return;

      if (worldspawn->getStatus() == Worldspawn::InGame) {
        if (!isLocalPlayer()) {
          animator->initBuffer(getGfxEngine()->getDevice());
          animator->animation = model->getAnimation("idle_animation");
          model->updateAnimator(getGfxEngine(), animator);
          model->render(getGfxEngine()->getDevice(), animator, NULL,
                        [this](gfx::BaseProgram* program) {
                          program->setParameter(
                              "model", gfx::DtMat4,
                              gfx::BaseProgram::Parameter{
                                  .matrix4x4 = getNode()->worldTransform()});
                        });

          if (heldWeaponRef) heldWeaponRef->renderWorld();
        } else {
          getGame()->getSoundManager()->listenerNode = entityNode;
          if (heldWeaponRef) heldWeaponRef->renderView();
        }
      }
    });
    /*getGfxEngine()->renderStepped.addClosure([this] {
      gfx::Entity* ent = getGfxEngine()->addEntity<PlayerEntity>(
          this,
          entityNode);
      ent->setMaterial(
          getGfxEngine()->getMaterialCache()->getOrLoad("Mesh").value());
          });*/
    rdm::Log::printf(rdm::LOG_DEBUG, "worldJob = %i", worldJob);
  } else {
    Worldspawn* worldspawn =
        dynamic_cast<Worldspawn*>(getManager()->findEntityByType("Worldspawn"));
    if (worldspawn && worldspawn->getStatus() == Worldspawn::InGame) {
      setStatus(InGame);
      controller->teleport(worldspawn->spawnLocation());
      getManager()->addPendingUpdateUnreliable(getEntityId());
    }
    giveWeapon((Weapon*)getManager()->instantiate("WeaponSniper"));
  }
}

WPlayer::~WPlayer() {
  if (!getManager()->isBackend()) {
    getGfxEngine()->renderStepped.removeListener(gfxJob);
    getWorld()->stepped.removeListener(worldJob);
  }
}

void WPlayer::giveWeapon(Weapon* weapon) {
  if (getManager()->isBackend()) {
    weapon->setOwnerRef(this);
    heldWeaponIndex = ownedWeapons.size();
    ownedWeapons.push_back(weapon);
    getManager()->addPendingUpdate(getEntityId());
  }
}

void WPlayer::tick() {
  Worldspawn* worldspawn =
      dynamic_cast<Worldspawn*>(getManager()->findEntityByType("Worldspawn"));

  bool needsUpdate = false;

  if (getManager()->isBackend() && isBot()) {
    controller->setSimulate(true);
    btTransform transform = controller->getTransform();
    btVector3 origin_old = transform.getOrigin();
    btVector3 origin_new = oldTransform.getOrigin();

    Log::printf(LOG_DEBUG, "Dist %f", origin_old.distance(origin_new));
    if (!controller->getRigidBody()->getLinearVelocity().fuzzyZero()) {
      needsUpdate = true;
      getController()->setTransformDirty();
    }
    if (oldFront.distance(getController()->getFront()) > 0.1) {
      needsUpdate = true;
      getController()->setRotationDirty();
    }

    if (needsUpdate) {
      oldTransform = transform;
      oldFront = getController()->getFront();

      getManager()->addPendingUpdateUnreliable(getEntityId());
    }
  }

  if (worldspawn && worldspawn->getFile()) {
    if (!isLocalPlayer()) {
      if (heldWeaponRef) {
        if (firingState[0])
          heldWeaponRef->primaryFire();
        else if (firingState[1])
          heldWeaponRef->secondaryFire();
      }
    }

    if (!getManager()->isBackend()) {
      if (isLocalPlayer()) {
        for (int i = 0; i < 9; i++) {
          if (rdm::Input::singleton()->isKeyDown(SDLK_1 + i)) {
            Log::printf(LOG_DEBUG, "%i", i);
            auto it = weaponIds.find(i + 1);
            if (it != weaponIds.end()) {
              wantedWeaponId = i + 1;
              getManager()->addPendingUpdate(getEntityId());
            }
            break;
          }
        }

        if (rdm::Input::singleton()->isMouseButtonDown(1)) {
          if (heldWeaponRef) {
            if (!firingState[0]) needsUpdate = true;
            firingState[0] = true;
            heldWeaponRef->primaryFire();
          }
        } else {
          if (firingState[0]) needsUpdate = true;
          firingState[0] = false;
        }
      }
    }

    controller->setEnable(true);
    if (!getManager()->isBackend() && isLocalPlayer()) {
      btTransform transform = controller->getTransform();

      btVector3 vel = controller->getRigidBody()->getLinearVelocity();
      soundEmitter->setPitch(
          controller->isGrounded() ? ((vel.length() < 1) ? 0.0 : 1.f) : 0.f);

      if (worldspawn && worldspawn->getFile()) {
        btVector3 origin_old = transform.getOrigin();
        btVector3 origin_new = oldTransform.getOrigin();
        // Log::printf(LOG_DEBUG, "%f, %f", forward_old.dot(forward_new),
        //            origin_old.distance(origin_new));

        if (origin_old.distance(origin_new) > 0.1) {
          needsUpdate = true;
          controller->setTransformDirty();
        }
        if (oldFront.distance(getController()->getFront()) > 0.01) {
          needsUpdate = true;
          controller->setRotationDirty();
        }

        if (needsUpdate) {
          oldTransform = transform;
          oldFront = getController()->getFront();

          getManager()->addPendingUpdateUnreliable(getEntityId());
        }
      }

      controller->setLocalPlayer(true);
    } else {
      controller->setLocalPlayer(false);
    }
  } else
    controller->setEnable(false);
}

void WPlayer::serialize(net::BitStream& stream) {
  Player::serialize(stream);
  if (getManager()->isBackend()) {
    stream.write<Status>(status);
    stream.write<unsigned char>(heldWeaponIndex);
    stream.write<int>(ownedWeapons.size());
    for (int i = 0; i < ownedWeapons.size(); i++) {
      stream.write<network::EntityId>(ownedWeapons[i]->getEntityId());
    }
  } else {
    stream.write<unsigned char>(wantedWeaponId);
  }
}

void WPlayer::deserialize(net::BitStream& stream) {
  Player::deserialize(stream);
  if (getManager()->isBackend()) {
    unsigned char wantedWeapon = stream.read<unsigned char>();
    /*if (wantedWeaponId != wantedWeapon) {
      auto it = weaponIds.find(wantedWeapon);
      if (it != weaponIds.end()) {
        if (heldWeaponRef) {
          getManager()->deleteEntity(heldWeaponRef->getEntityId());
        }
        heldWeaponRef =
            dynamic_cast<Weapon*>(getManager()->instantiate(it->second));
        Log::printf(LOG_DEBUG, "Instantiated weapon %s",
                    heldWeaponRef->getTypeName());
        wantedWeaponId = wantedWeapon;
        heldWeaponId = heldWeaponRef->getEntityId();
        getManager()->addPendingUpdate(getEntityId());
      } else {
        Log::printf(LOG_WARN, "Unknown wantedWeapon id %i", wantedWeapon);
      }
      }*/
    if (ownedWeapons.size()) {
      if (wantedWeapon < ownedWeapons.size()) {
        heldWeaponRef = ownedWeapons[wantedWeapon];
        wantedWeaponId = wantedWeapon;
      } else {
        wantedWeaponId = 0;
        heldWeaponRef = ownedWeapons[wantedWeaponId];
      }
    } else {
      heldWeaponRef = 0;
    }
  } else {
    status = stream.read<Status>();
    wantedWeaponId = stream.read<unsigned char>();
    if (wantedWeaponId > ownedWeapons.size()) wantedWeaponId = 0;
    heldWeaponIndex = wantedWeaponId;
    ownedWeapons.clear();
    int numWeapons = stream.read<int>();
    for (int i = 0; i < numWeapons; i++) {
      net::EntityId id = stream.read<net::EntityId>();
      Weapon* weapon = dynamic_cast<Weapon*>(getManager()->getEntityById(id));
      if (weapon) {
        weapon->setOwnerRef(this);
        ownedWeapons.push_back(weapon);
      } else {
        Log::printf(LOG_ERROR, "Invalid weapon id %i", id);
      }
    }
    if (ownedWeapons.size()) {
      heldWeaponRef = ownedWeapons[wantedWeaponId];
    }
  }
}

void WPlayer::serializeUnreliable(net::BitStream& stream) {
  {
    std::scoped_lock lock(getWorld()->getPhysicsWorld()->mutex);
    controller->serialize(stream);
  }

  stream.write<bool>(firingState[0]);
  stream.write<bool>(firingState[1]);
}

void WPlayer::precache(net::NetworkManager* manager) {
  manager->getGame()->getResourceManager()->load<resource::Model>(PLAYERMODEL);
}

void WPlayer::deserializeUnreliable(net::BitStream& stream) {
  {
    std::scoped_lock lock(getWorld()->getPhysicsWorld()->mutex);
    controller->deserialize(stream, getManager()->isBackend());
  }

  if (!isLocalPlayer()) {
    firingState[0] = stream.read<bool>();
    firingState[1] = stream.read<bool>();
  }
}
}  // namespace ww
