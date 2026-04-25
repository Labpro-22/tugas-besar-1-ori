#ifndef GAMESCREEN_HPP
#define GAMESCREEN_HPP

#include "views/gui/Screen.hpp"
#include "views/gui/components/Button.hpp"
#include "raylib.h"
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
    Button btnBuyProperty, btnAuction, btnBuildHouse, btnMortgage, btnRedeem;
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

    // ── Game messages ─────────────────────────────────────────────────────
    std::string lastEvent;

    // ── Generic popup ─────────────────────────────────────────────────────
    bool        showPopup;
    std::string popupTitle;
    std::string popupMsg;

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

    // For LASSO / DEMOLITION selection popup
    bool        showSelPopup = false;  // selection-list popup
    std::string selPopupTitle;
    std::vector<std::string> selOptions;
    std::vector<Button>      btnSelOptions;
    int         selPopupResult = -1;   // -1 = pending, >=0 = chosen idx

    // Two-step DEMOLITION: first pick player, then pick property
    int         demolitionPlayerPick = -1;

    // ── PPH choice popup ──────────────────────────────────────────────────
    bool pphPending = false;
    int  pphFlatAmt = 0;
    int  pphPctAmt  = 0;
    Button btnPphFlat;
    Button btnPphPct;

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
    bool canBuyCurrentTile()     const;
    bool checkFestival(Player& p);
    Color getColorGroupColor(const std::string& group) const;
    bool checkPPH(Player& p);   // true if PPH popup triggered

    // Auction
    void startAuction();
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
    void openSelPopup(const std::string& title, const std::vector<std::string>& opts);
    void updateSelPopup();
    void drawSelPopup();

    // Draw sub-sections
    void drawPlayTab(float cardX, float cardScale);
    void drawAssetsTab(float cardX, float cardScale);
    void drawPlayersTab(float cardX, float cardScale);
    void drawLogTab(float cardX, float cardScale);
    void drawTileOverlay();
    void drawPopup();
    void drawGameOverOverlay();

public:
    GameScreen(GameLoop* loop);
    ~GameScreen();
    void loadAssets()   override;
    void unloadAssets() override;
    void update(float dt) override;
    void draw()         override;
};

#endif
