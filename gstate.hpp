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
  friend class DbgProfileInfoWindow;

  std::future<rdm::HttpManager::Response> indexResponse;
  std::future<rdm::HttpManager::Response> authChallengeResponse;
  std::future<rdm::HttpManager::Response> authChallenge2Response;
  std::future<rdm::HttpManager::Response> characterResponse;
  std::future<rdm::HttpManager::Response> uploadKeyUuid;
  std::future<rdm::HttpManager::Response> uploadPlayer;
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
  virtual ~WWGameState();

  enum SetupStage {
    GetIndex,  // Sends web request to index so we know if the website even
               // works
    WaitMotd,  // Waits for motd response from website

    Authenticate,   // Authenticate first stage, tries to verify username &
                    // public key (if either fail, go to WaitKeyInfo or
                    // WaitAuthenticateinfo). Receives the challenge if valid,
                    // and we hash it in Authenticate2
    Authenticate2,  // Authenticate second stage, hashes the challenge received
                    // from Authenticate.
    Authenticate3,  // We check if the server allowed the authentication, and
                    // then check if the authenticated user has a character. If
                    // we do, the initialization stage is finished. Otherwise,
                    // we enter the create player menu and stuff happens there

    WaitAuthenticateInfo,  // Shows the prepare user info menu
    WaitKeyInfo,  // Here we send a request to the server to upload our user
                  // key, then make prepare user info show the qr code to upload
                  // the public key we have generated from SecurityManager

    GetPlayerInfo,
    CreatePlayer,
    UploadPlayer,
    WaitPlayerResponse,
    CreatedPlayer,  // play a fun sound

    Done,
    Failure  // If anything goes wrong we are here
  };

  bool getNeedsAuthentication() { return needsAuthentication; }
  void setStage(SetupStage stage) { currentStage = stage; };
  SetupStage getStage() { return currentStage; };
  virtual void renderMainMenu(rdm::gfx::Engine* engine);
  virtual void renderWaiting(rdm::gfx::Engine* engine);
  virtual void figureOutWhatToDo();
  virtual void tickWaiting();

  std::string getOurUuid() { return ourUuid; }

 private:
  SetupStage currentStage;
};
}  // namespace ww
