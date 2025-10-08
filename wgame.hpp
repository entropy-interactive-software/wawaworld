#pragma once
#include "game.hpp"
#include "http.hpp"
#include "network/network.hpp"
#include "planetmap.hpp"

namespace ww {
using namespace rdm;

class WWAuthenticationProvider : public rdm::network::IAuthenticationProvider {
  std::string privateToken;
  std::string publicToken;
  std::string uuid;

  void clientTokenReceived(network::Peer* peer, network::BitStream stream,
                           rdm::HttpManager::Response& response);

 public:
  WWAuthenticationProvider();

  virtual void sendPeerInfo(network::Peer* peer, network::BitStream& stream);
  virtual std::future<bool> verifyPeerInfo(network::Peer* peer,
                                           network::BitStream stream);
  virtual void serverSetup();
  virtual void serverDestroy();

  void setTokens(std::string publicToken, std::string privateToken,
                 std::string uuid) {
    this->publicToken = publicToken;
    this->privateToken = privateToken;
    this->uuid = uuid;
  }
};

struct WGamePrivate;
class WGame : public Game {
  WGamePrivate* game;
  PlanetMap* planet;

 public:
  WGame();
  ~WGame();

  void addEntityConstructors(network::NetworkManager* manager);

  virtual void initialize();
  virtual void initializeClient();
  virtual void initializeServer();

  virtual size_t getGameVersion();
};
};  // namespace ww
