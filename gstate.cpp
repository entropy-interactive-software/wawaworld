#include "gstate.hpp"

#include <qrencode.h>

#include <format>
#include <json.hpp>

#include "LinearMath/btQuaternion.h"
#include "game.hpp"
#include "gfx/gui/ngui_elements.hpp"
#include "gfx/gui/ngui_window.hpp"
#include "settings.hpp"
#include "state.hpp"
#include "wplayer.hpp"

#ifndef NDEBUG
static rdm::CVar baseurl("baseurl", "http://127.0.0.1:8000/", CVARF_SAVE);
#else
static rdm::CVar baseurl("baseurl", "https://endoh.ca/", CVARF_HIDDEN);
#endif

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

    rdm::gfx::gui::NGuiPanel* buttonRow0 =
        new rdm::gfx::gui::NGuiPanel(manager);
    buttonRow0->setLayout(new rdm::gfx::gui::NGuiHorizontalLayout());

    rdm::gfx::gui::ImageButton* ibutton0 =
        new rdm::gfx::gui::ImageButton(manager);
    ibutton0->setOverTexture(
        getGame()->getResourceManager()->load<rdm::resource::Texture>(
            "rdm/gui/normal_guy.png"));
    buttonRow0->addElement(ibutton0);

    rdm::gfx::gui::ImageButton* ibutton1 =
        new rdm::gfx::gui::ImageButton(manager);
    ibutton1->setOverTexture(
        getGame()->getResourceManager()->load<rdm::resource::Texture>(
            "rdm/gui/magicka.png"));
    buttonRow0->addElement(ibutton1);

    rdm::gfx::gui::ImageButton* ibutton2 =
        new rdm::gfx::gui::ImageButton(manager);
    ibutton2->setOverTexture(
        getGame()->getResourceManager()->load<rdm::resource::Texture>(
            "rdm/gui/gunns.png"));
    buttonRow0->addElement(ibutton2);

    rdm::gfx::gui::ImageButton* ibutton3 =
        new rdm::gfx::gui::ImageButton(manager);
    ibutton3->setOverTexture(
        getGame()->getResourceManager()->load<rdm::resource::Texture>(
            "rdm/gui/muscle.png"));
    buttonRow0->addElement(ibutton3);

    panelLeft->addElement(buttonRow0);

    rdm::gfx::gui::NGuiPanel* buttonRow1 =
        new rdm::gfx::gui::NGuiPanel(manager);
    buttonRow1->setLayout(new rdm::gfx::gui::NGuiHorizontalLayout());
    for (int i = 0; i < 9; i++) {
      rdm::gfx::gui::ImageButton* signbutton =
          new rdm::gfx::gui::ImageButton(manager);
      signbutton->setOverTexture(
          getGame()->getResourceManager()->load<rdm::resource::Texture>(
              std::format("rdm/gui/sign{}.png", i).c_str()));
      buttonRow1->addElement(signbutton);
    }
    panelLeft->addElement(buttonRow1);

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
    errorLabel->setTextMaxWidth(500);
    errorLabel->setAutoWrap(true);
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

class DbgProfileInfoWindow : public rdm::gfx::gui::NGuiWindow {
  rdm::gfx::gui::TextLabel* allTheStuff;

 public:
  DbgProfileInfoWindow(rdm::gfx::gui::NGuiManager* manager,
                       rdm::gfx::Engine* engine)
      : rdm::gfx::gui::NGuiWindow(manager, engine) {
    allTheStuff = new rdm::gfx::gui::TextLabel(manager);
    addElement(allTheStuff);
  }

  virtual void opening() {
    WWGameState* wg = dynamic_cast<WWGameState*>(getGame()->getGameState());
    allTheStuff->setText(std::format("UUID: {}\nUsername: {}", wg->getOurUuid(),
                                     ww_username.getValue()));
  }
};

NGUI_INSTANTIATOR(DbgProfileInfoWindow);

class PrepareUserWindow : public rdm::gfx::gui::NGuiWindow {
  rdm::gfx::gui::TextInput* username;
  rdm::gfx::gui::TextLabel* additionalInfo;
  std::unique_ptr<rdm::gfx::BaseTexture> qrCodeTexture;
  rdm::gfx::gui::Image* qrCodeImage;

 public:
  PrepareUserWindow(rdm::gfx::gui::NGuiManager* manager,
                    rdm::gfx::Engine* engine)
      : rdm::gfx::gui::NGuiWindow(manager, engine) {
    setTitle("Enter user information");

    qrCodeTexture = engine->getDevice()->createTexture();
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

    qrCodeImage = new rdm::gfx::gui::Image(manager);
    // qrCodeImage->setTexture(qrCodeTexture.get());
    qrCodeImage->setShowa(false);
    addElement(qrCodeImage);

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
  void showTheFuckingThing(std::string uuid) {
    std::string qrCodeUrl =
        baseurl.getValue() + "home/add_authorized_key?pubkey=" + uuid.c_str();
    QRcode* code =
        QRcode_encodeString(qrCodeUrl.c_str(), 0, QR_ECLEVEL_H, QR_MODE_8, 1);
    std::vector<char> imgBuf;  // need to convert to rgb
    for (int i = 0; i < code->width; i++) {
      for (int j = 0; j < code->width; j++) {
        int c = code->data[i + (j * code->width)];
        if (c & 1) {
          imgBuf.push_back(0x0);
          imgBuf.push_back(0x0);
          imgBuf.push_back(0x0);
        } else {
          imgBuf.push_back(0xff);
          imgBuf.push_back(0xff);
          imgBuf.push_back(0xff);
        }
      }
    }
    qrCodeTexture->upload2d(code->width, code->width, rdm::gfx::DtUnsignedByte,
                            rdm::gfx::BaseTexture::RGB, imgBuf.data());
    qrCodeTexture->setFiltering(rdm::gfx::BaseTexture::Nearest,
                                rdm::gfx::BaseTexture::Nearest);
    qrCodeImage->setSize(glm::vec2(code->width * 2.f));
    qrCodeImage->setTexture(qrCodeTexture.get());
    qrCodeImage->setShowa(true);
  }

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
              setStage(WaitMotd);
            } else {
              setStage(needsAuthentication ? Authenticate : Done);
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
      data["pubkey"] = getGame()->getSecurityManager()->getPublicKey();
      authChallengeResponse = rdm::HttpManager::singleton()->post(
          "api/ww/v1/challenge", data.dump(), rq);
      currentStage = Authenticate2;
    } break;
    case Authenticate2: {
      if (!authChallengeResponse.valid()) return;
      rdm::HttpManager::Response rs = authChallengeResponse.get();
      json js = json::parse(rs.getResponse());
      if (rs.statusCode == 200) {
        std::string challengeData = js["challenge"];

        rdm::SignedMessage msg = getGame()->getSecurityManager()->sign(
            challengeData.data(), challengeData.size());
        std::string challengeOutput;
        challengeOutput.resize(msg.data.size());
        memcpy(challengeOutput.data(), msg.data.data(), msg.data.size());

        rdm::HttpManager::Request rq;
        json data;
        data["challengeHashedSig"] = msg.sig;
        data["challengeHashedKey"] = msg.key;
        data["challengeHashedData"] = challengeOutput;
        data["uuid"] = js["uuid"];

        authChallenge2Response = rdm::HttpManager::singleton()->post(
            "api/ww/v1/challenge2", data.dump(), rq);
        setStage(Authenticate3);
      } else if (rs.statusCode == 400) {
        if (js["message"] == "unknown user") {
          setStage(WaitAuthenticateInfo);
          PrepareUserWindow* pWin = getGame()
                                        ->getGfxEngine()
                                        ->getGuiManager()
                                        ->getGui<PrepareUserWindow>();
          pWin->setMsg("The server could not find anyone with that username.");
          pWin->open();
        } else if (js["message"] == "unknown key") {
          setStage(WaitKeyInfo);
          rdm::HttpManager::Request rq;
          json j;
          j["pubkey"] = getGame()->getSecurityManager()->getPublicKey();
          uploadKeyUuid = rdm::HttpManager::singleton()->post(
              "/api/km/upload_key", j.dump(), rq);
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
    case WaitKeyInfo: {
      if (!uploadKeyUuid.valid()) return;
      rdm::HttpManager::Response rs = uploadKeyUuid.get();
      if (rs.statusCode == 200 || rs.statusCode == 201) {
        json j = json::parse(rs.getResponse());
        PrepareUserWindow* pWin = getGame()
                                      ->getGfxEngine()
                                      ->getGuiManager()
                                      ->getGui<PrepareUserWindow>();
        pWin->setMsg(
            "Please scan the QR code, and add the key in order to "
            "login.");
        getGame()->getGfxEngine()->renderStepped.addClosure([this, pWin, j] {
          pWin->showTheFuckingThing(j["id"]);
          pWin->open();
          setStage(WaitAuthenticateInfo);
        });
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
    case Authenticate3: {
      if (!authChallenge2Response.valid()) return;
      rdm::HttpManager::Response rs = authChallenge2Response.get();
      if (rs.statusCode != 200) {
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
      } else {
        json r = json::parse(rs.getResponse());
        json ud = r["data"];

        ourUuid = ud["user"];
        publicToken = ud["id"];
        privateToken = ud["authority"];

        rdm::HttpManager::Request rq;
        characterResponse = rdm::HttpManager::singleton()->get(
            std::format("/api/ww/v1/{}/chara", ourUuid.c_str()), rq);
        setStage(GetPlayerInfo);
      }
    } break;
    case GetPlayerInfo: {
      if (!characterResponse.valid()) break;
      rdm::HttpManager::Response rs = characterResponse.get();
      if (rs.statusCode == 200) {
        DbgProfileInfoWindow* win = getGame()
                                        ->getGfxEngine()
                                        ->getGuiManager()
                                        ->getGui<DbgProfileInfoWindow>();
        win->open();

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

void WWGameState::figureOutWhatToDo() {
  rdm::HttpManager::Request rq;
  rdm::HttpManager::singleton()->setBaseUrl(baseurl.getValue());
  indexResponse = rdm::HttpManager::singleton()->get("api/ww/v1/main", rq);
  getGame()->getGfxEngine()->setClearColor(glm::vec3(0.3));

  currentStage = GetIndex;
  setState(WaitForSomething);
}
}  // namespace ww
