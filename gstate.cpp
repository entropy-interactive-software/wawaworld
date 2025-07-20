#include "gstate.hpp"

#include <format>
#include <json.hpp>

#include "LinearMath/btQuaternion.h"
#include "game.hpp"
#include "gfx/gui/ngui_elements.hpp"
#include "gfx/gui/ngui_window.hpp"
#include "settings.hpp"
#include "state.hpp"
#include "wplayer.hpp"

using json = nlohmann::json;

namespace ww {
class CharacterCreatorWindow : public rdm::gfx::gui::NGuiWindow {
  rdm::resource::Model* playerModel;
  std::unique_ptr<rdm::resource::Model::Animator> playerAnimator;
  std::unique_ptr<rdm::gfx::Viewport> playerView;

 public:
  CharacterCreatorWindow(rdm::gfx::gui::NGuiManager* manager,
                         rdm::gfx::Engine* engine)
      : rdm::gfx::gui::NGuiWindow(manager, engine) {
    playerModel = getGame()->getResourceManager()->load<rdm::resource::Model>(
        PLAYERMODEL);
    playerAnimator.reset(new rdm::resource::Model::Animator());

    rdm::gfx::ViewportGfxSettings settings;
    settings.resolution = glm::ivec2(300, 400);
    playerView.reset(new rdm::gfx::Viewport(engine, settings));

    rdm::gfx::LightingManager& lm = playerView->getLightingManager();
    auto sun = lm.getSun();
    sun.ambient = glm::vec3(0.149, 0.173, 0.216);
    sun.diffuse = glm::vec3(1) - sun.ambient;
    sun.direction = glm::vec3(-1, 1, -0.1);
    lm.setSun(sun);

    rdm::gfx::Camera& cam = playerView->getCamera();
    cam.setTarget(glm::vec3(0, 0, 0.25));
    cam.setFOV(80.f);
    cam.setPosition(glm::vec3(-2, 4, 1.25));

    setLayout(new rdm::gfx::gui::NGuiHorizontalLayout());

    rdm::gfx::gui::NGuiPanel* panelLeft = new rdm::gfx::gui::NGuiPanel(manager);
    rdm::gfx::gui::TextLabel* label0 = new rdm::gfx::gui::TextLabel(manager);
    label0->setText(
        "ピエール・エリオット・トルドー"
        "（Pierre Elliott Trudeau、1919年10月18日 - 2000年9月28日）"
        "は、カナダの政治家、第20・22代首相。姓はトリュドーとも表記される。"
        "本名はジョセフ・フィリップ・ピエール・イヴ・エリオット・トルドー"
        "（Joseph Philippe Pierre Yves Elliott Trudeau）。\n\n"
        "By clicking Create you will agree to everything that will happen from "
        "now on.");
    label0->setAutoWrap(true);
    label0->setTextMaxWidth(400);
    panelLeft->addElement(label0);
    rdm::gfx::gui::Button* button0 = new rdm::gfx::gui::Button(manager);
    button0->setText("Create");
    panelLeft->addElement(button0);
    addElement(panelLeft);

    rdm::gfx::gui::Image* image0 = new rdm::gfx::gui::Image(manager);
    image0->setTexture(playerView->get());
    image0->setSize(settings.resolution);
    addElement(image0);

    setDraggable(false);
    setClosable(false);
    setCenter(true);

    setTitle("Character Creation");
  }

  virtual void frame() {
    playerAnimator->initBuffer(getEngine()->getDevice());
    if (!playerAnimator->animation)
      playerAnimator->animation = playerModel->getAnimation("idle_animation");
    void* _ = getEngine()->setViewport(playerView.get());
    getEngine()->getDevice()->clear(0.f, 0.f, 0.f, 0.f);
    getEngine()->getDevice()->clearDepth();
    playerModel->updateAnimator(getEngine(), playerAnimator.get());
    playerModel->render(getEngine()->getDevice(), playerAnimator.get());
    getEngine()->finishViewport(_);
  }
};
NGUI_INSTANTIATOR(CharacterCreatorWindow);

class CommunicationErrorWindow : public rdm::gfx::gui::NGuiWindow {
  rdm::gfx::gui::TextLabel* errorLabel;
  rdm::gfx::gui::Button* okButton;

 public:
  CommunicationErrorWindow(rdm::gfx::gui::NGuiManager* manager,
                           rdm::gfx::Engine* engine)
      : rdm::gfx::gui::NGuiWindow(manager, engine) {
    setTitle("Communication error");

    errorLabel = new rdm::gfx::gui::TextLabel(manager);
    errorLabel->setText("Hi");
    addElement(errorLabel);

    rdm::gfx::gui::TextLabel* apology = new rdm::gfx::gui::TextLabel(manager);
    apology->setText(
        "We are sorry for the inconvenience.\nThe game will now close");
    addElement(apology);

    okButton = new rdm::gfx::gui::Button(manager);
    okButton->setText("OK");
    okButton->setPressed([this] { close(); });
    addElement(okButton);

    setDraggable(false);
    setClosable(false);
    setCenter(true);
  }

  void setErrorMsg(std::string msg) { errorLabel->setText(msg); }

  virtual void closing() {
    getGame()->getGameState()->setState(rdm::GameState::Quit);
  }
};

NGUI_INSTANTIATOR(CommunicationErrorWindow);

class MotdWindow : public rdm::gfx::gui::NGuiWindow {
  rdm::gfx::gui::TextLabel* motdLabel;

 public:
  MotdWindow(rdm::gfx::gui::NGuiManager* manager, rdm::gfx::Engine* engine)
      : rdm::gfx::gui::NGuiWindow(manager, engine) {
    setTitle("Message of the day");
    motdLabel = new rdm::gfx::gui::TextLabel(manager);
    motdLabel->setTextMaxWidth(300);
    motdLabel->setAutoWrap(true);
    addElement(motdLabel);

    rdm::gfx::gui::Button* okButton = new rdm::gfx::gui::Button(manager);
    okButton->setText("OK");
    okButton->setPressed([this] { close(); });
    addElement(okButton);
  }

  void setMotdText(std::string t) { motdLabel->setText(t); }

  virtual void closing() {
    WWGameState* wg = dynamic_cast<WWGameState*>(getGame()->getGameState());
    wg->setStage(wg->getNeedsAuthentication()
                     ? WWGameState::Authenticate
                     : WWGameState::Done);  // currently in WaitMotd stage so
  }
};
NGUI_INSTANTIATOR(MotdWindow);

static rdm::CVar ww_username("ww_global_username", "", CVARF_SAVE);

class PrepareUserWindow : public rdm::gfx::gui::NGuiWindow {
  rdm::gfx::gui::TextInput* username;
  rdm::gfx::gui::TextLabel* additionalInfo;

 public:
  PrepareUserWindow(rdm::gfx::gui::NGuiManager* manager,
                    rdm::gfx::Engine* engine)
      : rdm::gfx::gui::NGuiWindow(manager, engine) {
    setTitle("Enter user information");

    setClosable(false);
    setDraggable(false);
    setCenter(true);

    rdm::gfx::gui::TextLabel* label0 = new rdm::gfx::gui::TextLabel(manager);
    label0->setText("Username:");
    addElement(label0);

    username = new rdm::gfx::gui::TextInput(manager);
    username->setEmptyText("TheLegend27");
    username->setLine(ww_username.getValue());
    addElement(username);

    additionalInfo = new rdm::gfx::gui::TextLabel(manager);
    additionalInfo->setText(" ");
    addElement(additionalInfo);

    rdm::gfx::gui::Button* okButton = new rdm::gfx::gui::Button(manager);
    okButton->setText("OK");
    okButton->setPressed([this, label0] {
      if (username->getLine().empty()) {
        label0->setText("Username: SET THIS!!!");
      } else
        close();
    });
    addElement(okButton);
  }

  void setMsg(std::string msg) { additionalInfo->setText(msg); }

  virtual void closing() {
    ww_username.setValue(username->getLine());
    WWGameState* wg = dynamic_cast<WWGameState*>(getGame()->getGameState());
    wg->setStage(WWGameState::Authenticate);
  }
};

NGUI_INSTANTIATOR(PrepareUserWindow);

WWGameState::WWGameState(rdm::Game* game) : rdm::GameState(game) {
  // stateMusic[MainMenu] = "dat5/mus/main_menu.ogg";
  currentStage = GetIndex;
  musicEmitter.reset(game->getSoundManager()->newEmitter());
}

void WWGameState::renderMainMenu(rdm::gfx::Engine* engine) {
  rdm::gfx::Camera& cam = engine->getCamera();
}

void WWGameState::renderWaiting(rdm::gfx::Engine* engine) {}

void WWGameState::tickWaiting() {
  if (!getGame()->getGfxEngine()->getGuiManager()) return;
  CommunicationErrorWindow* win = getGame()
                                      ->getGfxEngine()
                                      ->getGuiManager()
                                      ->getGui<CommunicationErrorWindow>();
  switch (currentStage) {
    case GetIndex:
      if (!indexResponse.valid()) return;
      {
        rdm::HttpManager::Response rsp = indexResponse.get();
        switch (rsp.statusCode) {
          default: {
            win->setErrorMsg(
                std::format("Error communicating with server. "
                            "Error details: {}\n{}",
                            rsp.statusCode, rsp.getResponse()));
            win->open();
            break;
          }
          case 200:  // YAY
          {
            json p = json::parse(rsp.getResponse());
            needsAuthentication = p["useAuthentication"];
            if (!p["motd"].is_null()) {
              MotdWindow* motdWin = getGame()
                                        ->getGfxEngine()
                                        ->getGuiManager()
                                        ->getGui<MotdWindow>();
              motdWin->setMotdText(p["motd"]);
              motdWin->open();
              currentStage = WaitMotd;
            } else {
              currentStage = needsAuthentication ? Authenticate : Done;
            }
          } break;
        }
      }
      break;
    case Authenticate: {
      if (ww_username.getValue().empty()) {
        setStage(WaitAuthenticateInfo);
        PrepareUserWindow* pWin = getGame()
                                      ->getGfxEngine()
                                      ->getGuiManager()
                                      ->getGui<PrepareUserWindow>();
        pWin->open();
        break;
      }

      rdm::HttpManager::Request rq;
      json data;
      data["username"] = ww_username.getValue();
      data["pubkey"] = "";
      authChallengeResponse = rdm::HttpManager::singleton()->post(
          "api/ww/v1/challenge", data.dump(), rq);
      currentStage = Authenticate2;
    } break;
    case Authenticate2: {
      if (!authChallengeResponse.valid()) return;
      rdm::HttpManager::Response rs = authChallengeResponse.get();
      if (rs.headers["Content-Type"] != "application/json") {
        CommunicationErrorWindow* win =
            getGame()
                ->getGfxEngine()
                ->getGuiManager()
                ->getGui<CommunicationErrorWindow>();
        win->setErrorMsg(
            std::format("Error communicating with server. "
                        "Error details: {}\n{}",
                        rs.statusCode, rs.getResponse()));
        win->open();
        setStage(Failure);
        for (auto& [key, value] : rs.headers) {
          rdm::Log::printf(rdm::LOG_DEBUG, "%s=%s", key.c_str(), value.c_str());
        }
        break;
      }

      json js = json::parse(rs.getResponse());
      if (rs.statusCode == 200) {
        ourUuid = js["uuid"];

        rdm::HttpManager::Request rq;
        characterResponse = rdm::HttpManager::singleton()->get(
            std::format("/api/ww/v1/{}/chara", ourUuid.c_str()), rq);
        setStage(GetPlayerInfo);
      } else if (rs.statusCode == 400) {
        if (js["message"] == "unknown user") {
          setStage(WaitAuthenticateInfo);
          PrepareUserWindow* pWin = getGame()
                                        ->getGfxEngine()
                                        ->getGuiManager()
                                        ->getGui<PrepareUserWindow>();
          pWin->setMsg("The server could not find anyone with that username.");
          pWin->open();
        }
      } else {
        CommunicationErrorWindow* win =
            getGame()
                ->getGfxEngine()
                ->getGuiManager()
                ->getGui<CommunicationErrorWindow>();
        win->setErrorMsg(
            std::format("Error communicating with server. "
                        "Error details: {}\n{}",
                        rs.statusCode, rs.getResponse()));
        win->open();
        setStage(Failure);
      }
    } break;
    case GetPlayerInfo: {
      if (!characterResponse.valid()) break;
      rdm::HttpManager::Response rs = characterResponse.get();
      if (rs.statusCode == 200) {
        json j = json::parse(rs.getResponse());
        if (!j["character_created"]) {
          setStage(CreatePlayer);  // start making da player
        } else {
          setStage(Done);
        }
      } else {
        CommunicationErrorWindow* win =
            getGame()
                ->getGfxEngine()
                ->getGuiManager()
                ->getGui<CommunicationErrorWindow>();
        win->setErrorMsg(
            std::format("Error communicating with server. "
                        "Error details: {}\n{}",
                        rs.statusCode, rs.getResponse()));
        win->open();
        setStage(Failure);
      }
    } break;
    case CreatePlayer:
      if (!musicEmitter->isPlaying()) {
        musicEmitter->play(
            getGame()
                ->getSoundManager()
                ->getSoundCache()
                ->get("rdm/mus/create_player.mp3", rdm::Sound::Stream)
                .value());
        musicEmitter->setLooping(true);
        musicEmitter->setGain(1.25);
        beginCreateTime = getGame()->getGfxEngine()->getTime();
        CharacterCreatorWindow* cWin = getGame()
                                           ->getGfxEngine()
                                           ->getGuiManager()
                                           ->getGui<CharacterCreatorWindow>();
        cWin->open();
      }
      getGame()->getGfxEngine()->setClearColor(glm::mix(
          glm::vec3(0.3), glm::vec3(0),
          std::min(1.0f,
                   getGame()->getGfxEngine()->getTime() - beginCreateTime)));
      break;
    case Done:
      setState(MainMenu);
      break;
    default:
      break;
  }

  if (currentStage != CreatePlayer) {
    if (musicEmitter->isPlaying()) musicEmitter->stop();
  }
}

#ifndef NDEBUG
static rdm::CVar baseurl("baseurl", "http://127.0.0.1:9898/", CVARF_SAVE);
#else
static rdm::CVar baseurl("baseurl", "https://endoh.ca/", CVARF_HIDDEN);
#endif

void WWGameState::figureOutWhatToDo() {
  rdm::HttpManager::Request rq;
  rdm::HttpManager::singleton()->setBaseUrl(baseurl.getValue());
  indexResponse = rdm::HttpManager::singleton()->get("api/ww/v1/main", rq);
  getGame()->getGfxEngine()->setClearColor(glm::vec3(0.3));

  currentStage = GetIndex;
  setState(WaitForSomething);
}
}  // namespace ww
