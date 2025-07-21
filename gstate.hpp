#pragma once
#include "game.hpp"
#include "http.hpp"
#include "state.hpp"
namespace ww {
struct CharacterData {
  int initialClass;
  int sign;
};

class WWGameState : public rdm::GameState {
  std::future<rdm::HttpManager::Response> indexResponse;
  std::future<rdm::HttpManager::Response> authChallengeResponse;
  std::future<rdm::HttpManager::Response> authChallenge2Response;
  std::future<rdm::HttpManager::Response> characterResponse;
  std::future<rdm::HttpManager::Response> uploadKeyUuid;
  std::future<rdm::HttpManager::Response>* currentResponse;

  std::unique_ptr<rdm::SoundEmitter> musicEmitter;
  bool needsAuthentication;
  float beginCreateTime;

  std::string ourUuid;
  std::string keyUuid;
  std::string privateToken;
  std::string publicToken;

 public:
  WWGameState(rdm::Game* game);

  enum SetupStage {
    GetIndex,
    WaitMotd,
    Authenticate,
    WaitAuthenticateInfo,
    Authenticate2,
    WaitKeyInfo,
    Authenticate3,
    GetPlayerInfo,
    CreatePlayer,
    Done,
    Failure
  };

  bool getNeedsAuthentication() { return needsAuthentication; }
  void setStage(SetupStage stage) { currentStage = stage; };
  virtual void renderMainMenu(rdm::gfx::Engine* engine);
  virtual void renderWaiting(rdm::gfx::Engine* engine);
  virtual void figureOutWhatToDo();
  virtual void tickWaiting();

  std::string getOurUuid() { return ourUuid; }

 private:
  SetupStage currentStage;
};
}  // namespace ww
