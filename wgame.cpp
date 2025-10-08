#include "wgame.hpp"

#include <format>
#include <json.hpp>

#include "filesystem.hpp"
#include "gfx/base_types.hpp"
#include "gfx/heightmap.hpp"
#include "gfx/imgui/imgui.h"
#include "gstate.hpp"
#include "input.hpp"
#include "logging.hpp"
#include "map.hpp"
#include "network/entity.hpp"
#include "network/network.hpp"
#include "putil/fpscontroller.hpp"
#include "settings.hpp"
#include "sound.hpp"
#include "state.hpp"
#include "weapons/magnum.hpp"
#include "weapons/sniper.hpp"
#include "world.hpp"
#include "worldspawn.hpp"
#include "wplayer.hpp"

using json = nlohmann::json;

namespace ww {
enum UIState {
  MainMenu,
  HostPanel,
  ConnectPanel,
};

struct HostParameters {
  int port;
  char map[64];

  HostParameters() {
    port = 7938;
    strncpy(map,
            Settings::singleton()->getCvar("sv_nextmap")->getValue().c_str(),
            64);
  }
};

static CVar server("server", "false", CVARF_CONSOLE_ARGUMENT);
static CVar host("host", "false", CVARF_CONSOLE_ARGUMENT);

static HostParameters* hostParams;

struct WGamePrivate {
  float cameraPitch;
  float cameraYaw;
  putil::FpsController* controller;

  Worldspawn* worldspawn;
  UIState state;

  std::unique_ptr<SoundEmitter> mainMenuSound;
};

using namespace rdm;
WGame::WGame() : Game() {
  setIcon("rdm/icon.png");

  Input::singleton()->newAxis("ForwardBackward", SDLK_W, SDLK_S);
  Input::singleton()->newAxis("LeftRight", SDLK_A, SDLK_D);

  game = new WGamePrivate();
}

size_t WGame::getGameVersion() { return 0x00000001; }

WGame::~WGame() { delete game; }

WWAuthenticationProvider::WWAuthenticationProvider() {
  CVar* cv = rdm::Settings::singleton()->getCvar("baseurl", true);
  rdm::HttpManager::singleton()->setBaseUrl(cv->getValue());
}

void WWAuthenticationProvider::clientTokenReceived(
    network::Peer* peer, network::BitStream stream,
    rdm::HttpManager::Response& response) {}

void WWAuthenticationProvider::sendPeerInfo(network::Peer* peer,
                                            network::BitStream& stream) {
  rdm::HttpManager::Request rq;
  network::NetworkManager* manager = getManager();
  std::string ourUuid = uuid;
  std::string ourPublicToken = publicToken;
  /*rq.requestFinished = [this, peer,
                        stream](rdm::HttpManager::Response& response) {
    if (response.statusCode == 200) {
      json j = json::parse(response.getResponse());
      network::BitStream outstream;
      outstream.writeStream(stream);
      outstream.writeString(response.getResponse());
      getManager()->sendPacket(peer, outstream)
    } else {
      Log::printf(LOG_ERROR, "WWAuthenticationProvider error %s",
                  response.getResponse().c_str());
    }
    };*/
  json ct;
  ct["tokenAuthority"] = privateToken;
  auto rsp = rdm::HttpManager::singleton()
                 ->post("api/ww/v1/open_client_ticket", ct.dump(), rq)
                 .get();
  if (rsp.statusCode != 200) {
    Log::printf(LOG_ERROR, "Unable to open a client ticket %i, response: %s",
                rsp.statusCode, rsp.getResponse().c_str());
    stream.write<bool>(false);

    getManager()->sendPacket(peer, stream);
    return;
  }
  stream.write<bool>(true);
  json jrsp = json::parse(rsp.getResponse());
  stream.writeString((std::string)(jrsp["clientTicket"]));
  stream.writeString(publicToken);
  getManager()->sendPacket(peer, stream);
}

std::future<bool> WWAuthenticationProvider::verifyPeerInfo(
    network::Peer* peer, network::BitStream stream) {
  std::promise<bool> promise;

  bool verified = stream.read<bool>();
  if (!verified) {
    if (peer->address.host == 0x7f000001) {
      Log::printf(LOG_DEBUG,
                  "Allowed local client to connect, because they're cool! :D");
      promise.set_value(true);
      return promise.get_future();
    } else {
      Log::printf(LOG_DEBUG,
                  "Denied client, because they said they couldn't get a ticket "
                  "from the server");
      promise.set_value(false);
      return promise.get_future();
    }
  }

  json ct;
  ct["tokenAuthority"] = privateToken;
  auto rsp = rdm::HttpManager::singleton()
                 ->post("api/ww/v1/verify_client_ticket", ct.dump())
                 .get();
  return promise.get_future();
}

void WWAuthenticationProvider::serverSetup() {
  rdm::HttpManager::Response mainrsp =
      rdm::HttpManager::singleton()->get("api/ww/v1/main").get();
  try {
    json maindt = json::parse(mainrsp.getResponse());
    if (!maindt["motd"].empty()) {
      Log::printf(LOG_INFO, "MOTD: %s", ((std::string)maindt["motd"]).c_str());
    }

    json data;
    data["username"] =
        rdm::Settings::singleton()->getCvar("ww_global_username")->getValue();
    data["pubkey"] =
        getManager()->getGame()->getSecurityManager()->getPublicKey();

    rdm::HttpManager::Response authenticate1 =
        rdm::HttpManager::singleton()
            ->post("api/ww/v1/challenge", data.dump())
            .get();
  } catch (std::exception& e) {
    Log::printf(LOG_ERROR, "Failed setup %s", e.what());
  }
}

void WWAuthenticationProvider::serverDestroy() {}

void WGame::addEntityConstructors(network::NetworkManager* manager) {
  manager->setPassword("RDMEXRDMEXRDMEX");
  manager->registerConstructor<Worldspawn>("Worldspawn");
  manager->registerConstructor<WPlayer>("WPlayer");
  manager->registerConstructor<WeaponSniper>("WeaponSniper");
  manager->registerConstructor<WeaponMagnum>("WeaponMagnum");
  manager->setPlayerType("WPlayer");
  manager->setAuthenticationProvider(new WWAuthenticationProvider());
}

static CVar ip("ip", "", CVARF_CONSOLE_ARGUMENT);
static CVar port("port", "7938", CVARF_CONSOLE_ARGUMENT);

void WGame::initializeClient() {
  startGameState(GameStateConstructor<WWGameState>);

  addEntityConstructors(getWorld()->getNetworkManager());

  getGfxEngine()->getMaterialCache()->addDataFile(
      "rdm/materials/materials.json");

  // gfxEngine->setForcedAspect(4.0 / 3.0);

  world->setTitle("RDM");
  world->getPhysicsWorld()->getWorld()->setGravity(btVector3(0, 0, -406.67));

  if (host.getBool()) {
    startServer();
    world->getNetworkManager()->connect("127.0.0.1", port.getInt());
  }

  std::scoped_lock lock(world->worldLock);
  world->stepped.listen([this] {
    game->worldspawn =
        (Worldspawn*)world->getNetworkManager()->findEntityByType("Worldspawn");
  });

  Input::singleton()->keyDownSignals[SDLK_TAB].listen([] {
    Input::singleton()->setMouseLocked(!Input::singleton()->getMouseLocked());
    Log::printf(LOG_DEBUG, "mouseLocked = %s",
                Input::singleton()->getMouseLocked() ? "true" : "false");
  });

  gfxEngine->initialized.listen([this] {
    // game->file->initGfx(gfxEngine.get());
    // gfxEngine->addEntity<MapEntity>(game->file);
  });

  game->mainMenuSound.reset(getSoundManager()->newEmitter());
  /*game->mainMenuSound->play(getSoundManager()
                                ->getSoundCache()
                                ->get("dat5/main_menu.ogg", Sound::Stream)
                                .value());*/

  game->state = MainMenu;
  gfxEngine->renderStepped.listen([this] {
    network::Peer::Type peerType =
        world->getNetworkManager()->getLocalPeer().type;

    if (peerType == network::Peer::ConnectedPlayer) {
      if (game->worldspawn) {
        if (game->worldspawn->getFile())
          game->worldspawn->getFile()->updatePosition(
              gfxEngine->getCamera().getPosition());
      } else {
      }
    } else {
      Input::singleton()->setMouseLocked(false);
    }
  });

  if (!ip.getValue().empty()) {
    world->getNetworkManager()->connect(ip.getValue(), port.getInt());
  }
}

void WGame::initializeServer() {
  addEntityConstructors(worldServer->getNetworkManager());
  worldServer->getNetworkManager()->start(hostParams->port);
  worldServer->getPhysicsWorld()->getWorld()->setGravity(
      btVector3(0, 0, -106.67));

  Worldspawn* wspawn =
      (Worldspawn*)worldServer->getNetworkManager()->instantiate("Worldspawn");
  wspawn->setNextMap(hostParams->map);
}

void WGame::initialize() {
  hostParams = new HostParameters;
  hostParams->port = port.getInt();

  WorldConstructorSettings& settings = getWorldConstructorSettings();
  settings.network = true;
  settings.physics = true;

  if (server.getBool()) {
    startServer();
  } else {
    startClient();
  }
}
}  // namespace ww
