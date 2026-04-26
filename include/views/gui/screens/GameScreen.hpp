#ifndef GAMESCREEN_HPP
#define GAMESCREEN_HPP

#include "views/gui/Screen.hpp"
#include "views/gui/components/Button.hpp"
#include "raylib.h"
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

class GameLoop;
class GameState;
class Player;
class PropertyTile;
class LandingProcessor;
class BankruptcyProcessor;
class RentCollector;
class CardProcessor;
class BotController;

class GameScreen : public Screen {
private:
    // ── Textures ──────────────────────────────────────────────────────────
    Texture2D bgTexture;
    Texture2D blurredBgTexture;
    Texture2D boardTexture;
    Texture2D playerIconsB[4];   // with background (UI panels)
    Texture2D playerIconsNb[4];  // no background (board overlay)
    Texture2D cardPanel;
    Texture2D diceTextures[6];
    Texture2D houseTexture;
    Texture2D hotelTexture;
    Texture2D pamTexture;
    Texture2D plnTexture;
    Texture2D trainTexture;
    Texture2D aktaTexture;
    Texture2D chanceCardTex;
    Texture2D communityCardTex;
    Texture2D lelangBackTexture;

    // ── Static tile display data ──────────────────────────────────────────
    struct TileInfo {
        std::string name, code, type, colorGroup;
        int buyPrice = 0, mortgageValue = 0, houseCost = 0, hotelCost = 0;
        int rent[6] = {};
    };
    TileInfo tileData[40];
    int selectedTileIdx;

    // ── Core buttons ──────────────────────────────────────────────────────
    Button btnPlay, btnAssets, btnPlayers, btnLog;
    Button btnRollDice, btnEndTurn;
    Button btnBuildHouse, btnMortgage, btnRedeem;
    Button btnSaveGame;
    Button btnPopupOk;

    // ── Set-dice (manual override) ────────────────────────────────────────
    Button btnSetDice;
    Button btnD1Plus, btnD1Minus;
    Button btnD2Plus, btnD2Minus;
    Button btnConfirmDice;
    Button btnCloseDice;
    bool setDiceMode = false;
    bool forceDice   = false;
    int  setDice1    = 1;
    int  setDice2    = 1;

    int activeTab;

    // ── Dice animation ────────────────────────────────────────────────────
    int   dice1, dice2;
    bool  diceRolling;
    float diceRollTimer, diceRollInterval;
    int   diceRollFrame;

    // ── Turn state ────────────────────────────────────────────────────────
    bool hasRolled;
    bool turnEnded;
    bool isDouble;
    int  consecDoubles;

    // ── Layout ────────────────────────────────────────────────────────────
    float globalScale;
    float boardX, boardY, boardScale;
    float cornerRatioW, cornerRatioH;
    float tileCenters[40][2];
    Rectangle tileBounds[40];

    // ── Backend ───────────────────────────────────────────────────────────
    GameLoop* gameLoop;
    GameState* gs;
    std::unique_ptr<LandingProcessor>    landing;
    std::unique_ptr<BankruptcyProcessor> bankruptcy;
    std::unique_ptr<RentCollector>       rentCollector;
    std::unique_ptr<CardProcessor>       cardProc;
    std::unique_ptr<BotController>       botCtrl;

    // ── Event log (shown in info box) ────────────────────────────────────
    std::deque<std::string> eventLog;  // newest at front
    static constexpr int EVENT_LOG_MAX = 20;
    int txLogCursor = 0;
    float logScroll = 0.0f;
    void pushLog(const std::string& msg);

    // ── Generic popup ─────────────────────────────────────────────────────
    bool        showPopup;
    std::string popupTitle;
    std::string popupMsg;

    // ── Card draw popup (Kesempatan / Dana Umum) ──────────────────────────
    bool        showCardPopup  = false;
    bool        cardPopupIsChance = true;
    std::string cardPopupDesc;
    Button      btnCardPopupOk;
    void drawCardPopup();

    // ── Auction state ─────────────────────────────────────────────────────
    bool                 auctionMode     = false;
    PropertyTile*        auctionProp     = nullptr;
    std::vector<Player*> auctionOrder;          // bidders in turn order
    int                  auctionIdx      = 0;   // current bidder in auctionOrder
    int                  auctionHighBid  = 0;
    Player*              auctionHighBidder = nullptr;
    std::vector<bool>    auctionPassed;
    char                 auctionInputBuf[12] = {};
    int                  auctionInputLen = 0;
    Button               btnAuctionBid;
    Button               btnAuctionPass;

    // ── Skill-card interaction ─────────────────────────────────────────────
    enum class CardMode { NONE, TELEPORT_SELECT, LASSO_SELECT, DEMOLITION_SELECT };
    CardMode    cardMode       = CardMode::NONE;
    int         pendingCardIdx = -1;   // card index being used
    std::string cardSelectMsg;         // hint text shown while selecting
    Button      btnSkillCards[4];      // per card in current player's hand

    // ── Skill-card confirmation popup ──────────────────────────────────────
    bool        skillCardConfirmPending = false;
    int         skillCardConfirmIdx     = -1;
    Button      btnSkillConfirmUse;
    Button      btnSkillConfirmCancel;

    // ── Drop-card popup (hand > 3, must discard one) ──────────────────────
    bool               dropCardPending    = false;
    bool               dropCardConfirm    = false;   // confirmation step
    int                dropCardConfirmIdx = -1;
    std::vector<Button> btnDropCards;
    Button             btnDropYes;
    Button             btnDropNo;
    void drawDropCardPopup();
    void checkDropCard();

    // For LASSO / DEMOLITION / GADAI / TEBUS selection popup
    enum class SelAction { NONE, LASSO, DEMOLITION_PLAYER, DEMOLITION_PROP, GADAI, TEBUS };
    bool        showSelPopup = false;
    std::string selPopupTitle;
    std::vector<std::string>  selOptions;
    std::vector<Button>       btnSelOptions;
    int         selPopupResult = -1;   // -1 = pending, >=0 = chosen idx
    SelAction   selAction = SelAction::NONE;

    // Cached property lists for gadai/tebus selection
    std::vector<PropertyTile*> gadaiProps;
    std::vector<PropertyTile*> tebusProps;

    // Two-step DEMOLITION: first pick player, then pick property
    int         demolitionPlayerPick = -1;

    // ── PPH choice popup ──────────────────────────────────────────────────
    bool pphPending = false;
    int  pphFlatAmt = 0;
    int  pphPctAmt  = 0;
    Button btnPphFlat;
    Button btnPphPct;
    // PPH debt: amount chosen but couldn't afford — pay when debtMode resolves
    int         pphDebtAmt  = 0;
    std::string pphDebtDesc;

    // ── Build popup ───────────────────────────────────────────────────────
    bool buildMode = false;
    int  buildStep = 0; // 0 = select color group, 1 = select property
    std::map<std::string, std::vector<PropertyTile*>> buildGroupProps;
    std::vector<std::string> buildGroupNames;
    std::vector<Button> btnBuildGroups;
    std::vector<Button> btnBuildProps;
    Button btnBuildCancel;
    Button btnBuildConfirm;
    std::string buildSelectedGroup;
    PropertyTile* buildSelectedProp = nullptr;

    // ── Festival popup ────────────────────────────────────────────────────
    bool festivalPending = false;
    std::vector<PropertyTile*> festivalProps;
    std::vector<Button> btnFestivalProps;
    Button btnFestivalCancel;
    float festivalScroll = 0.0f;

    // ── Property landing popup (BUY / AUCTION) ────────────────────────────
    bool propertyLandingPending = false;
    int  propertyLandingTileIdx = -1;
    bool propertyLandingDouble  = false;
    Button btnPropBuy;
    Button btnPropAuction;

    // ── Save popup ────────────────────────────────────────────────────────
    bool saveMode = false;
    char saveInputBuf[64] = "savegame";
    int  saveInputLen = 8;
    Button btnSaveConfirm;
    Button btnSaveCancel;

    // ── Jail choice popup (start of jailed player's turn) ────────────────
    bool   jailChoicePending = false;
    Button btnJailPay;
    Button btnJailRoll;
    void   checkJailChoice();   // call after turn advances
    void   drawJailChoice();

    // ── Debt recovery mode (can't afford rent/tax) ───────────────────────
    bool    debtMode     = false;   // player can't afford landing → must gadai/lelang
    bool    debtLanding  = false;   // applyLanding still pending after debt resolved
    int     debtAmount   = 0;       // how much is owed
    Button  btnDebtGadai;
    Button  btnDebtLelang;
    Button  btnDebtForce;           // force-proceed even if still broke (bankruptcy)

    // ── Game over ─────────────────────────────────────────────────────────
    bool gameOverOverlay;

    // ── Helpers ───────────────────────────────────────────────────────────
    void computeLayout();
    void initProcessors();
    void drainAndStoreEvents();
    void executeRollResult();
    void handleEndTurn();
    bool currentPlayerIsBot()    const;
    bool currentPlayerIsJailed() const;
    bool checkFestival(Player& p);
    Color getColorGroupColor(const std::string& group) const;
    bool checkPPH(Player& p);   // true if PPH popup triggered
    // Apply landing for skill-card moves (teleport, move card) — handles buy/auction popup
    void resolveCardLanding(Player& p, int tileIdx);

    // Auction
    void startAuction(PropertyTile* prop = nullptr);  // prop=nullptr → use current tile
    void startNextBankruptAuction();
    void updateAuction();
    void drawAuctionUI(float cardX, float cardScale);

    // Skill cards
    void useCardImmediate(int cardIdx);
    void useCardWithTarget(int cardIdx, CardMode mode);
    void finishTeleport(int tileIdx);
    void finishLasso(int playerIdx);
    void finishDemolition(int playerIdx, int propIdx);
    void drawSkillCardSection(float cardX, float cardScale, float startY);

    // Selection popup
    void openSelPopup(const std::string& title, const std::vector<std::string>& opts,
                      SelAction action = SelAction::NONE);
    void openGadaiPopup();
    void openTebusPopup();
    void updateSelPopup();
    void drawSelPopup();

    // Draw sub-sections
    void drawPlayTab(float cardX, float cardScale);
    void drawDebtMode(float cardX, float cardScale);
    void drawAssetsTab(float cardX, float cardScale);
    void drawPlayersTab(float cardX, float cardScale);
    void drawLogTab(float cardX, float cardScale);
    void drawTileOverlay();
    void drawPopup();
    void drawGameOverOverlay();

public:
    GameScreen(GameLoop* loop, const std::string& initMsg = "");
    ~GameScreen();
    void loadAssets()   override;
    void unloadAssets() override;
    void update(float dt) override;
    void draw()         override;
};

#endif
