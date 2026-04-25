#include "views/gui/screens/GameScreen.hpp"

#include "core/GameLoop.hpp"
#include "core/GameState.hpp"
#include "core/LandingProcessor.hpp"
#include "core/BankruptcyProcessor.hpp"
#include "core/RentCollector.hpp"
#include "core/RentManager.hpp"
#include "core/CardProcessor.hpp"
#include "core/BotController.hpp"
#include "core/BuyManager.hpp"
#include "core/PropertyManager.hpp"
#include "core/JailManager.hpp"
#include "core/TaxManager.hpp"
#include "models/player/Bot.hpp"
#include "models/player/Player.hpp"
#include "models/tiles/PropertyTile.hpp"
#include "models/tiles/TaxTile.hpp"
#include "models/tiles/FestivalTile.hpp"
#include "core/SkillCardManager.hpp"
#include "models/card/SpecialPowerCard.hpp"
#include "utils/exceptions/SaveLoadException.hpp"
#include "utils/exceptions/UnablePayBuildException.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

// ─── layout constants ─────────────────────────────────────────────────────────
static const float CORNER_RATIO_W = 2.0f / 14.0f;
static const float CORNER_RATIO_H = 2.0f / 14.0f;
static const float BOARD_INSET_W  = 0.002f;
static const float BOARD_INSET_H  = 0.002f;

static void getGridRC(int i, int& row, int& col) {
    if      (i == 0)       { row = 10; col = 10; }
    else if (i <= 9)       { row = 10; col = 10 - i; }
    else if (i == 10)      { row = 10; col = 0; }
    else if (i <= 19)      { row = 20 - i; col = 0; }
    else if (i == 20)      { row = 0;  col = 0; }
    else if (i <= 29)      { row = 0;  col = i - 20; }
    else if (i == 30)      { row = 0;  col = 10; }
    else                   { row = i - 30; col = 10; }
}

// ─── constructor / destructor ─────────────────────────────────────────────────
GameScreen::GameScreen(GameLoop* loop)
    : bgTexture{}, blurredBgTexture{}, boardTexture{}, cardPanel{},
      aktaTexture{}, selectedTileIdx(-1),
      activeTab(0),
      dice1(1), dice2(1), diceRolling(false),
      diceRollTimer(0), diceRollInterval(0.08f), diceRollFrame(0),
      hasRolled(false), turnEnded(false), isDouble(false), consecDoubles(0),
      globalScale(1.0f), boardX(0), boardY(0), boardScale(1.0f),
      cornerRatioW(CORNER_RATIO_W), cornerRatioH(CORNER_RATIO_H),
      gameLoop(loop), gs(loop ? loop->getState() : nullptr),
      lastEvent(""), showPopup(false), gameOverOverlay(false)
{
    for (int i = 0; i < 4; i++) playerIconsB[i]  = {};
    for (int i = 0; i < 4; i++) playerIconsNb[i] = {};
    for (int i = 0; i < 6; i++) diceTextures[i] = {};
    for (int i = 0; i < 40; i++) { tileCenters[i][0] = 0; tileCenters[i][1] = 0; }

    if (gs) initProcessors();
}

GameScreen::~GameScreen() = default;

// ─── initProcessors ───────────────────────────────────────────────────────────
void GameScreen::initProcessors() {
    landing      = std::make_unique<LandingProcessor>(*gs);
    bankruptcy   = std::make_unique<BankruptcyProcessor>(*gs, *landing);
    rentCollector= std::make_unique<RentCollector>(*gs, *bankruptcy);
    gs->rentCollector = rentCollector.get();
    landing->setRentCollector(rentCollector.get());
    landing->setBankruptcyProcessor(bankruptcy.get());
    cardProc     = std::make_unique<CardProcessor>(*gs, *bankruptcy);
    botCtrl      = std::make_unique<BotController>(*gs, *landing, *bankruptcy,
                                                    *cardProc, *rentCollector);
}

// ─── drainAndStoreEvents ──────────────────────────────────────────────────────
void GameScreen::drainAndStoreEvents() {
    if (!gs) return;
    auto evts = gs->events.drainEvents();
    if (!evts.empty()) lastEvent = evts.back();
    // auto-detect card events and show popup
    for (const auto& e : evts) {
        if (e.find("Kesempatan") != std::string::npos ||
            e.find("Dana Umum") != std::string::npos ||
            e.find("KARTU") != std::string::npos) {
            showPopup = true;
            popupTitle = "KARTU";
            popupMsg = e;
        }
    }
}

// ─── helper predicates ────────────────────────────────────────────────────────
bool GameScreen::currentPlayerIsBot() const {
    if (!gs) return false;
    return dynamic_cast<Bot*>(gs->currentTurnPlayer()) != nullptr;
}

bool GameScreen::currentPlayerIsJailed() const {
    if (!gs) return false;
    Player* p = gs->currentTurnPlayer();
    return p && p->getStatus() == "JAIL";
}

bool GameScreen::canBuyCurrentTile() const {
    if (!gs || !hasRolled) return false;
    Player* p = gs->currentTurnPlayer();
    if (!p) return false;
    int idx = p->getCurrTile();
    if (idx < 0 || idx >= (int)gs->tiles.size()) return false;
    auto* prop = dynamic_cast<PropertyTile*>(gs->tiles[idx]);
    return prop && prop->getTileOwner() == nullptr;
}

bool GameScreen::checkPPH(Player& p) {
    int tileIdx = p.getCurrTile();
    auto* taxTile = dynamic_cast<TaxTile*>(gs->tiles[tileIdx]);
    if (taxTile && gs->tiles[tileIdx]->getTileType() == "TAX_PPH" && !currentPlayerIsBot()) {
        pphPending = true;
        pphFlatAmt = TaxManager::calculatePPHFlat(gs->config);
        pphPctAmt  = TaxManager::calculatePPHPct(p, gs->config);
        return true;
    }
    return false;
}

bool GameScreen::checkFestival(Player& p) {
    if (!gs) return false;
    int tileIdx = p.getCurrTile();
    auto* festTile = dynamic_cast<FestivalTile*>(gs->tiles[tileIdx]);
    if (!festTile || gs->tiles[tileIdx]->getTileType() != "FESTIVAL") return false;
    if (currentPlayerIsBot()) return false;

    festivalProps.clear();
    btnFestivalProps.clear();
    for (auto* prop : p.getOwnedProperties()) {
        if (prop && !prop->isMortgage()) {
            festivalProps.push_back(prop);
        }
    }
    if (festivalProps.empty()) {
        // no eligible properties — just show a quick popup
        showPopup = true;
        popupTitle = "FESTIVAL";
        popupMsg   = "Anda mendarat di Festival!\nTapi tidak punya properti\nyang bisa dipilih.";
        return false;
    }
    festivalPending = true;
    return true;
}

Color GameScreen::getColorGroupColor(const std::string& group) const {
    if (group == "Brown")       return {101, 67, 33, 255};
    if (group == "Light Blue")  return {173, 216, 230, 255};
    if (group == "Pink")        return {255, 105, 180, 255};
    if (group == "Orange")      return {255, 165, 0, 255};
    if (group == "Red")         return {220, 20, 60, 255};
    if (group == "Yellow")      return {255, 215, 0, 255};
    if (group == "Green")       return {34, 139, 34, 255};
    if (group == "Dark Blue")   return {0, 0, 139, 255};
    return {128, 128, 128, 255};
}

// ─── executeRollResult ────────────────────────────────────────────────────────
// called once dice animation finishes — apply roll to backend
void GameScreen::executeRollResult() {
    if (!gs) return;
    Player* p = gs->currentTurnPlayer();
    if (!p) return;

    auto finalizeRoll = [&]() {
        drainAndStoreEvents();
        hasRolled = true;
        gameLoop->checkWinGui();
        if (gs->game_over) gameOverOverlay = true;
    };

    if (currentPlayerIsJailed()) {
        int jailTile = p->getCurrTile();
        int curJailTurns = (gs->jail_turns.count(p)) ? gs->jail_turns[p] : 0;

        if (curJailTurns >= 3) {
            // forced fine + move
            bankruptcy->tryForceJailFine(*p);
            landing->rollAndMove(*p, true, dice1, dice2);
            landing->applyGoSalary(*p, jailTile);
            if (checkPPH(*p)) return;
            landing->applyLanding(*p);
            isDouble = false;
        } else {
            auto jr = JailManager::rollInJail(*p, gs->dice,
                                               curJailTurns, gs->board.getTileCount(),
                                               true, dice1, dice2);
            if (jr.escaped) {
                gs->jail_turns.erase(p);
                p->setStatus("ACTIVE");
                landing->applyGoSalary(*p, jailTile);
                if (checkPPH(*p)) return;
                landing->applyLanding(*p);
                isDouble = false;
            } else {
                gs->jail_turns[p] = jr.newJailTurns;
                lastEvent = p->getUsername() + " gagal keluar penjara ("
                            + std::to_string(jr.newJailTurns) + "/3)";
                isDouble = false;
                drainAndStoreEvents();
                hasRolled = true;
                handleEndTurn();
                return;
            }
        }
    } else {
        bool dbl = landing->rollAndMove(*p, true, dice1, dice2);
        landing->applyGoSalary(*p, p->getCurrTile() - dice1 - dice2); // old tile approx
        if (checkPPH(*p)) return;
        landing->applyLanding(*p);
        isDouble = dbl;
        if (dbl) {
            consecDoubles++;
            if (consecDoubles >= 3) {
                // 3 consecutive doubles → jail
                landing->sendToJail(*p);
                isDouble = false;
                consecDoubles = 0;
            }
        }
    }

    finalizeRoll();
    if (gs && !gs->game_over) checkFestival(*p);
}

// ─── handleEndTurn ───────────────────────────────────────────────────────────
void GameScreen::handleEndTurn() {
    if (!gs) return;
    if (isDouble && !gameOverOverlay) {
        // bonus roll: reset dice flags only, keep consecDoubles
        hasRolled  = false;
        isDouble   = false;
        turnEnded  = false;
        lastEvent  = "Dadu ganda! Giliran tambahan.";
        return;
    }
    consecDoubles = 0;
    gameLoop->checkWinGui();
    if (!gs->game_over) {
        gameLoop->advanceTurn();
        hasRolled  = false;
        turnEnded  = false;
        isDouble   = false;
        lastEvent  = "";
    } else {
        gameOverOverlay = true;
    }
}

// ─── computeLayout ────────────────────────────────────────────────────────────
void GameScreen::computeLayout() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    globalScale = std::min(sw / 1280.0f, sh / 720.0f);

    float maxH = sh * 0.96f;
    float maxW = sw * 0.55f;
    boardScale = std::min(maxW / (float)boardTexture.width,
                          maxH / (float)boardTexture.height);
    boardX = sw * 0.02f;
    boardY = (sh - boardTexture.height * boardScale) / 2.0f;

    float bw  = boardTexture.width  * boardScale;
    float bh  = boardTexture.height * boardScale;
    float inX = bw * BOARD_INSET_W;
    float inY = bh * BOARD_INSET_H;
    float tX0 = boardX + inX;
    float tY0 = boardY + inY;
    float tW  = bw - 2.0f * inX;
    float tH  = bh - 2.0f * inY;

    float cornerSz  = tW * cornerRatioW;
    float sideSz    = (tW - 2.0f * cornerSz)  / 9.0f;
    float cornerSzh = tH * cornerRatioH;
    float sideSzh   = (tH - 2.0f * cornerSzh) / 9.0f;

    for (int i = 0; i < 40; i++) {
        int row, col;
        getGridRC(i, row, col);

        float cx = (col == 0)  ? tX0 + cornerSz / 2.0f
                 : (col == 10) ? tX0 + tW - cornerSz / 2.0f
                               : tX0 + cornerSz + (col - 1) * sideSz + sideSz / 2.0f;

        float cy = (row == 0)  ? tY0 + cornerSzh / 2.0f
                 : (row == 10) ? tY0 + tH - cornerSzh / 2.0f
                               : tY0 + cornerSzh + (row - 1) * sideSzh + sideSzh / 2.0f;

        tileCenters[i][0] = cx;
        tileCenters[i][1] = cy;

        bool isCorner = (row == 0 || row == 10) && (col == 0 || col == 10);
        bool isTopBot = (row == 0 || row == 10) && !isCorner;
        float tw = isCorner ? cornerSz  : (isTopBot ? sideSz   : cornerSz);
        float th = isCorner ? cornerSzh : (isTopBot ? cornerSzh: sideSzh);
        tileBounds[i] = { cx - tw / 2.0f, cy - th / 2.0f, tw, th };
    }

    // ── card panel layout ─────────────────────────────────────────────────
    float boardRight = boardX + bw;
    float availW     = sw - boardRight - 10.0f;
    float cardScale  = std::min(availW / (float)cardPanel.width,
                                (sh - 40.0f) / (float)cardPanel.height);
    if (cardScale < 0.1f) cardScale = 0.1f;
    float cardX = boardRight + (availW - cardPanel.width * cardScale) * 0.65f;
    float cardY = (sh - cardPanel.height * cardScale) / 2.0f;

    float panelCenterX = cardX + cardPanel.width * cardScale / 2.0f;
    float btnW         = cardPanel.width * cardScale * 0.22f;
    float btnH         = 35.0f * globalScale;
    float btnSpacing   = 4.0f * globalScale;
    float totalBtnW    = 4.0f * btnW + 3.0f * btnSpacing;
    float btnStartX    = panelCenterX - totalBtnW / 1.94f;
    float btnY         = cardY + cardPanel.height * cardScale * 0.265f;

    btnPlay.loadAsText("PLAY",    btnStartX,                                btnY, btnW, btnH);
    btnAssets.loadAsText("ASSETS",btnStartX +       (btnW + btnSpacing),    btnY, btnW, btnH);
    btnPlayers.loadAsText("PLAYERS",btnStartX + 2.0f*(btnW + btnSpacing),   btnY, btnW, btnH);
    btnLog.loadAsText("LOG",      btnStartX + 3.0f*(btnW + btnSpacing),     btnY, btnW, btnH);
    btnPlay.setActive(activeTab == 0);
    btnAssets.setActive(activeTab == 1);
    btnPlayers.setActive(activeTab == 2);
    btnLog.setActive(activeTab == 3);

    float iconSz    = cardPanel.width * cardScale * 0.15f;
    float iconYPos  = btnY - 198.0f * globalScale;
    float playerNameBottom = iconYPos + iconSz + 10.0f * globalScale;

    // dice position (must match draw() exactly)
    float diceSz      = cardPanel.width * cardScale * 0.14f;
    float diceGap     = 10.0f * globalScale;
    float diceY       = playerNameBottom + 21.0f * globalScale;
    float diceTotalW  = diceSz + diceSz + diceGap;
    float d1x         = cardX + (cardPanel.width * cardScale - diceTotalW) / 2.0f - 105.0f * globalScale;
    float d2x         = d1x + diceSz + diceGap;

    // ROLL / END / ATUR DADU — placed to the right of dice, centered in remaining space
    float btnGap      = 6.0f * globalScale;
    float rollBtnW    = cardPanel.width * cardScale * 0.30f ;
    float endBtnW     = cardPanel.width * cardScale * 0.16f ;
    float actionBtnH  = 45.0 * 0.85f * 0.80f;

    float diceRight   = d2x + diceSz;
    float remainingX  = diceRight + 7.0f * globalScale;
    float remainingW  = (cardX + cardPanel.width * cardScale) - remainingX - 4.0f * globalScale;
    float groupW      = rollBtnW + btnGap + endBtnW;
    // clamp: if group is too wide, squeeze it into remaining space
    if (groupW > remainingW) {
        float scaleDown = remainingW / groupW;
        rollBtnW *= scaleDown;
        endBtnW  *= scaleDown;
        groupW    = rollBtnW + btnGap + endBtnW ;
    }
    float rollBtnX    = remainingX + (remainingW - groupW) / 4.0f;
    float endBtnX     = rollBtnX + rollBtnW + btnGap;
    float actionBtnY  = diceY + (diceSz - actionBtnH) / 2.0f + 23.0f * globalScale;

    btnRollDice.loadAsText("ROLL", rollBtnX, actionBtnY, rollBtnW, actionBtnH,
                           {244,206,43,255},{250,220,70,255},{80,40,35,255});
    btnEndTurn.loadAsText("END", endBtnX, actionBtnY, endBtnW, actionBtnH,
                          {255,235,202,255},{255,240,215,255},{80,40,35,255});

    // ── ATUR DADU button — placed below ROLL/END ──────────────────────────
    float setDiceBtnY  = actionBtnY + actionBtnH + 5.0f * globalScale;
    float setDiceBtnW  = groupW;
    float setDiceBtnH  = actionBtnH * 0.75f;
    btnSetDice.loadAsText("ATUR DADU", rollBtnX, setDiceBtnY, setDiceBtnW, setDiceBtnH,
                          {80,60,50,255},{110,80,70,255},{220,200,180,255});

    // counter buttons positioned inside popup — rebuilt in draw() via loadAsText each frame

    float boxPad  = 12.0f * globalScale;
    float abBoxX  = cardX + boxPad + 20.0f;
    float abBoxY  = btnY + btnH + boxPad + 10.0f;
    float abBoxW  = cardPanel.width * cardScale - 6.0f * boxPad;
    float labelH  = 14.0f * globalScale;
    float abtnY   = abBoxY + 100.0f * globalScale + 10.0f * globalScale + labelH + 8.0f * globalScale;
    float abtnColGap = 8.0f * globalScale;
    float abtnRowGap = 6.0f * globalScale;
    float abtnW   = (abBoxW - abtnColGap) / 2.0f;
    float abtnH   = 30.0f * globalScale;
    Color abBg    = {255,235,202,255};
    Color abHover = {255,220,180,255};
    Color abFg    = {80,40,35,255};
    float col2X   = abBoxX + abtnW + abtnColGap;

    btnBuyProperty.loadAsText("BUY PROPERTY", abBoxX, abtnY,                                    abtnW, abtnH, abBg, abHover, abFg);
    btnAuction.loadAsText(    "AUCTION",       col2X,  abtnY,                                    abtnW, abtnH, abBg, abHover, abFg);
    btnBuildHouse.loadAsText( "BUILD HOUSE",  abBoxX, abtnY+(abtnH+abtnRowGap),                  abtnW, abtnH, abBg, abHover, abFg);
    btnMortgage.loadAsText(   "MORTGAGE",      col2X,  abtnY+(abtnH+abtnRowGap),                  abtnW, abtnH, abBg, abHover, abFg);
    btnRedeem.loadAsText(     "TEBUS",        abBoxX, abtnY+2.0f*(abtnH+abtnRowGap),              abtnW, abtnH, abBg, abHover, abFg);

    // auction bid / pass buttons (positioned over the action-buttons area)
    float aucBtnH = abtnH;
    float aucBtnW = (abBoxW - abtnColGap) / 2.0f;
    btnAuctionBid.loadAsText("BID",  abBoxX,        abtnY, aucBtnW, aucBtnH, {70,160,90,255},{90,180,110,255},WHITE);
    btnAuctionPass.loadAsText("PASS",col2X,          abtnY, aucBtnW, aucBtnH, {200,60,60,255},{220,80,80,255},WHITE);

    // save game — small button anchored top-right of the panel
    float saveW = 60.0f * globalScale, saveH = 22.0f * globalScale;
    float saveX = cardX + cardPanel.width * cardScale - saveW - 35.0f * globalScale;
    float saveY = cardY + 20.0f * globalScale;
    btnSaveGame.loadAsText("SAVE", saveX, saveY, saveW, saveH,
                           {80,40,35,255},{110,60,55,255},WHITE);

    // popup OK button
    float popW = 380.0f * globalScale, popH = 200.0f * globalScale;
    float popX = (sw - popW) / 2.0f, popY = (sh - popH) / 2.0f;
    float okW = 100.0f * globalScale, okH = 34.0f * globalScale;
    btnPopupOk.loadAsText("OK", popX + (popW - okW) / 2.0f,
                          popY + popH - okH - 20.0f * globalScale,
                          okW, okH, {148,73,68,255},{180,90,85,255},WHITE);
}

// ─── loadAssets ───────────────────────────────────────────────────────────────
void GameScreen::loadAssets() {
    bgTexture     = LoadTexture("assets/page_background.png");
    Image bgImg   = LoadImage("assets/page_background.png");
    ImageBlurGaussian(&bgImg, 10);
    blurredBgTexture = LoadTextureFromImage(bgImg);
    UnloadImage(bgImg);

    boardTexture  = LoadTexture("assets/board.png");
    cardPanel     = LoadTexture("assets/menu/menu_back.png");

    // with-background icons for UI/header/players tab
    playerIconsB[0] = LoadTexture("assets/assets1/gru_b.png");
    playerIconsB[1] = LoadTexture("assets/assets1/minion_b.png");
    playerIconsB[2] = LoadTexture("assets/assets1/purple_b.png");
    playerIconsB[3] = LoadTexture("assets/assets1/banana_b.png");
    // no-background icons for board
    playerIconsNb[0] = LoadTexture("assets/playericons/gru_nb.png");
    playerIconsNb[1] = LoadTexture("assets/playericons/minion_nb.png");
    playerIconsNb[2] = LoadTexture("assets/playericons/purple_nb.png");
    playerIconsNb[3] = LoadTexture("assets/playericons/banana_nb.png");
    diceTextures[0] = LoadTexture("assets/assets1/dice-1-export 1.png");
    diceTextures[1] = LoadTexture("assets/assets1/dice-2-export 1.png");
    diceTextures[2] = LoadTexture("assets/assets1/dice-3-export 1.png");
    diceTextures[3] = LoadTexture("assets/assets1/dice-4-export 1.png");
    diceTextures[4] = LoadTexture("assets/assets1/dice-5-export 1.png");
    diceTextures[5] = LoadTexture("assets/assets1/dice-6-export 1.png");

    houseTexture = LoadTexture("assets/assets1/house 1.png");
    hotelTexture = LoadTexture("assets/assets1/hotel 1.png");
    pamTexture   = LoadTexture("assets/assets1/pam 1.png");
    plnTexture   = LoadTexture("assets/assets1/pln 1.png");
    trainTexture = LoadTexture("assets/assets1/train 1.png");
    aktaTexture  = LoadTexture("assets/Akta_background.png");

    // load static tile data from config
    std::string line;
    {
        std::ifstream f("config/board.txt");
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            int pos; std::string code, name, type;
            ss >> pos >> code >> name >> type;
            if (pos < 1 || pos > 40) continue;
            for (char& c : name) if (c == '_') c = ' ';
            int idx = pos - 1;
            tileData[idx].name = name;
            tileData[idx].code = code;
            tileData[idx].type = type;
        }
    }
    {
        std::ifstream f("config/property.txt");
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            int pos; std::string code, name, type, color;
            ss >> pos >> code >> name >> type >> color;
            if (pos < 1 || pos > 40) continue;
            for (char& c : color) if (c == '_') c = ' ';
            int idx = pos - 1;
            tileData[idx].colorGroup = color;
            if (type == "STREET") {
                ss >> tileData[idx].buyPrice >> tileData[idx].mortgageValue
                   >> tileData[idx].houseCost >> tileData[idx].hotelCost;
                for (int r = 0; r < 6; r++) ss >> tileData[idx].rent[r];
            } else {
                int dummy; ss >> dummy >> tileData[idx].mortgageValue;
            }
        }
    }

    computeLayout();
}

// ─── unloadAssets ─────────────────────────────────────────────────────────────
void GameScreen::unloadAssets() {
    UnloadTexture(bgTexture);
    UnloadTexture(blurredBgTexture);
    UnloadTexture(boardTexture);
    UnloadTexture(cardPanel);
    for (int i = 0; i < 4; i++) UnloadTexture(playerIconsB[i]);
    for (int i = 0; i < 4; i++) UnloadTexture(playerIconsNb[i]);
    for (int i = 0; i < 6; i++) UnloadTexture(diceTextures[i]);
    UnloadTexture(houseTexture);
    UnloadTexture(hotelTexture);
    UnloadTexture(pamTexture);
    UnloadTexture(plnTexture);
    UnloadTexture(trainTexture);
    UnloadTexture(aktaTexture);
    btnPlay.unload(); btnAssets.unload(); btnPlayers.unload(); btnLog.unload();
    btnRollDice.unload(); btnEndTurn.unload();
    btnBuyProperty.unload(); btnAuction.unload();
    btnBuildHouse.unload(); btnMortgage.unload(); btnRedeem.unload();
    btnAuctionBid.unload(); btnAuctionPass.unload();
    btnSaveGame.unload();
    btnSetDice.unload();
    btnD1Plus.unload(); btnD1Minus.unload();
    btnD2Plus.unload(); btnD2Minus.unload();
    btnConfirmDice.unload(); btnCloseDice.unload();
    btnPopupOk.unload();
    for (auto& b : btnSkillCards) b.unload();
}

// ─── update ───────────────────────────────────────────────────────────────────
void GameScreen::update(float dt) {
    // blocking overlays: no other interaction allowed
    bool anyOverlay = showPopup || showSelPopup || gameOverOverlay || setDiceMode
                      || auctionMode || cardMode != CardMode::NONE || pphPending
                      || buildMode || festivalPending;

    // tab switching (only when no overlay is blocking)
    if (!anyOverlay) {
        if (btnPlay.isClicked())    activeTab = 0;
        if (btnAssets.isClicked())  activeTab = 1;
        if (btnPlayers.isClicked()) activeTab = 2;
        if (btnLog.isClicked())     activeTab = 3;
    }
    btnPlay.setActive(activeTab == 0);
    btnAssets.setActive(activeTab == 1);
    btnPlayers.setActive(activeTab == 2);
    btnLog.setActive(activeTab == 3);

    // ── update disabled states every frame ────────────────────────────────
    {
        bool blocked = showPopup || gameOverOverlay || !gs || auctionMode
                       || showSelPopup || cardMode != CardMode::NONE || pphPending
                       || buildMode || festivalPending;
        bool isBot   = gs && currentPlayerIsBot();
        bool jailed  = gs && currentPlayerIsJailed();

        // ROLL: only when it's human's turn, hasn't rolled yet, dice not animating
        btnRollDice.setDisabled(blocked || isBot || hasRolled || diceRolling);

        // END: only after rolling (and dice done animating)
        btnEndTurn.setDisabled(blocked || isBot || !hasRolled || diceRolling);

        // ATUR DADU: only before rolling
        btnSetDice.setDisabled(blocked || isBot || hasRolled || diceRolling);

        // action buttons: only after rolling, only for human, only relevant ones
        bool afterRoll = !blocked && !isBot && hasRolled && !diceRolling;

        if (jailed) {
            // In jail: BUY repurposed as "Bayar Denda" — always usable after roll context
            btnBuyProperty.setDisabled(!afterRoll);
            btnAuction.setDisabled(true);
            btnBuildHouse.setDisabled(true);
            btnMortgage.setDisabled(true);
            btnRedeem.setDisabled(true);
        } else {
            btnBuyProperty.setDisabled(!afterRoll || !canBuyCurrentTile());
            btnAuction.setDisabled(!afterRoll);           // always shows "Coming Soon"
            btnBuildHouse.setDisabled(!afterRoll);
            btnMortgage.setDisabled(!afterRoll);
            // TEBUS: check if any owned property is mortgaged
            bool hasMortgaged = false;
            if (gs) {
                Player* cur = gs->currentTurnPlayer();
                if (cur) {
                    for (auto* prop : cur->getOwnedProperties()) {
                        if (prop && prop->isMortgage()) { hasMortgaged = true; break; }
                    }
                }
            }
            btnRedeem.setDisabled(!afterRoll || !hasMortgaged);
        }

        // Dynamic colors for ROLL / END: active = yellow, disabled = cream
        if (btnRollDice.isDisabled()) {
            btnRollDice.setColors({255,235,202,255}, {255,240,215,255}, {80,40,35,255});
        } else {
            btnRollDice.setColors({244,206,43,255}, {250,220,70,255}, {80,40,35,255});
        }
        if (btnEndTurn.isDisabled()) {
            btnEndTurn.setColors({255,235,202,255}, {255,240,215,255}, {80,40,35,255});
        } else {
            btnEndTurn.setColors({244,206,43,255}, {250,220,70,255}, {80,40,35,255});
        }
    }

    // popup blocks everything else
    if (showPopup) {
        if (btnPopupOk.isClicked()) showPopup = false;
        return;
    }

    // game over overlay: only allow going home
    if (gameOverOverlay) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
            nextScreen     = AppScreen::HOME;
            shouldChangeScreen = true;
        }
        return;
    }

    // ESC: cancel card mode / deselect tile / go home
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (cardMode != CardMode::NONE) { cardMode = CardMode::NONE; pendingCardIdx = -1; return; }
        if (selectedTileIdx >= 0)       { selectedTileIdx = -1; return; }
        nextScreen = AppScreen::HOME; shouldChangeScreen = true; return;
    }

    // ── tile click: TELEPORT target OR normal selection ───────────────────
    if (!setDiceMode && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = GetMousePosition();
        int hitTile = -1;
        for (int i = 0; i < 40; i++) {
            if (CheckCollisionPointRec(mouse, tileBounds[i])) { hitTile = i; break; }
        }
        if (hitTile >= 0) {
            if (cardMode == CardMode::TELEPORT_SELECT) {
                finishTeleport(hitTile);
            } else if (selectedTileIdx >= 0) {
                selectedTileIdx = -1;
            } else {
                selectedTileIdx = hitTile;
            }
        } else if (selectedTileIdx >= 0 && cardMode == CardMode::NONE) {
            selectedTileIdx = -1;
        }
    }

    if (!gs) return;

    // ── save game ─────────────────────────────────────────────────────────
    if (btnSaveGame.isClicked()) {
        try {
            gameLoop->saveToFile("save/quicksave.json");
            lastEvent = "Game tersimpan ke save/quicksave.json";
        } catch (...) {
            showPopup = true; popupTitle = "SAVE GAGAL";
            popupMsg  = "Tidak bisa menyimpan.\nCek folder save/ ada.";
        }
    }

    // ── ATUR DADU popup ───────────────────────────────────────────────────
    if (btnSetDice.isClicked()) setDiceMode = true;

    if (setDiceMode) {
        // recompute popup button positions so isClicked() matches draw()
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
        float popW = 300.0f * globalScale;
        float popH = 190.0f * globalScale;
        float popX = (sw2 - popW) / 2.0f;
        float popY = (sh2 - popH) / 2.0f;
        float pmW  = 32.0f * globalScale;
        float numW = 40.0f * globalScale;
        float cH2  = 34.0f * globalScale;
        float dGap = 24.0f * globalScale;
        float totalW = 2.0f*(pmW + numW + pmW) + dGap;
        float cX2  = popX + (popW - totalW) / 2.0f;
        float cY2  = popY + 60.0f * globalScale;
        float okW  = 90.0f * globalScale, okH = 30.0f * globalScale;
        float xSz  = 22.0f * globalScale;
        btnConfirmDice.loadAsText("OK", popX + (popW - okW)/2.0f,
                                  cY2 + cH2 + 12.0f*globalScale, okW, okH,
                                  {70,140,80,255},{90,170,100,255},WHITE);
        btnCloseDice.loadAsText("X", popX + popW - xSz - 6.0f*globalScale,
                                popY + 6.0f*globalScale, xSz, xSz,
                                {120,50,45,255},{160,70,65,255},WHITE);
        btnD1Minus.loadAsText("-", cX2, cY2, pmW, cH2, {80,50,40,255},{110,70,60,255},WHITE);
        btnD1Plus.loadAsText("+", cX2+pmW+numW, cY2, pmW, cH2, {80,50,40,255},{110,70,60,255},WHITE);
        btnD2Minus.loadAsText("-", cX2 + 2*pmW + numW + dGap, cY2, pmW, cH2, {80,50,40,255},{110,70,60,255},WHITE);
        btnD2Plus.loadAsText("+", cX2 + 2*pmW + numW + dGap + pmW + numW, cY2, pmW, cH2, {80,50,40,255},{110,70,60,255},WHITE);

        if (btnCloseDice.isClicked())   { setDiceMode = false; return; }
        if (btnD1Minus.isClicked()) setDice1 = std::max(1, setDice1 - 1);
        if (btnD1Plus.isClicked())  setDice1 = std::min(6, setDice1 + 1);
        if (btnD2Minus.isClicked()) setDice2 = std::max(1, setDice2 - 1);
        if (btnD2Plus.isClicked())  setDice2 = std::min(6, setDice2 + 1);
        if (btnConfirmDice.isClicked()) {
            forceDice   = true;
            setDiceMode = false;
            dice1 = setDice1;
            dice2 = setDice2;
        }
        return;
    }

    // ── PPH popup ─────────────────────────────────────────────────────────
    if (pphPending) {
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
        float popW = 360.0f * globalScale;
        float popH = 220.0f * globalScale;
        float popX = (sw2 - popW) / 2.0f;
        float popY = (sh2 - popH) / 2.0f;

        float btnW = (popW - 30.0f * globalScale) / 2.0f;
        float btnH = 40.0f * globalScale;
        float btnY = popY + popH - btnH - 20.0f * globalScale;
        btnPphFlat.loadAsText(TextFormat("Flat M%d", pphFlatAmt),
                              popX + 10.0f * globalScale, btnY, btnW, btnH,
                              {244,206,43,255},{250,220,70,255},{80,40,35,255});
        char pctBuf[64];
        std::snprintf(pctBuf, sizeof(pctBuf), "%d%% (M%d)", gs->config.getPphPercentage(), pphPctAmt);
        btnPphPct.loadAsText(pctBuf,
                             popX + 20.0f * globalScale + btnW, btnY, btnW, btnH,
                             {244,206,43,255},{250,220,70,255},{80,40,35,255});

        Player* p = gs->currentTurnPlayer();
        if (btnPphFlat.isClicked()) {
            int amt = pphFlatAmt;
            if (p->getBalance() >= amt) {
                *p += -amt;
                gs->addLog(*p, "BAYAR_PAJAK", "PPH FLAT M" + std::to_string(amt));
                gs->events.pushEvent(p->getUsername() + " membayar PPH flat M" + std::to_string(amt));
            } else {
                lastEvent = "Saldo tidak cukup untuk bayar PPH!";
            }
            pphPending = false;
            hasRolled = true;
            drainAndStoreEvents();
            gameLoop->checkWinGui();
            if (gs->game_over) gameOverOverlay = true;
        } else if (btnPphPct.isClicked()) {
            int amt = pphPctAmt;
            if (p->getBalance() >= amt) {
                *p += -amt;
                gs->addLog(*p, "BAYAR_PAJAK", "PPH PCT M" + std::to_string(amt));
                gs->events.pushEvent(p->getUsername() + " membayar PPH " + std::to_string(gs->config.getPphPercentage()) + "% = M" + std::to_string(amt));
            } else {
                lastEvent = "Saldo tidak cukup untuk bayar PPH!";
            }
            pphPending = false;
            hasRolled = true;
            drainAndStoreEvents();
            gameLoop->checkWinGui();
            if (gs->game_over) gameOverOverlay = true;
        }
        return;
    }

    // ── Festival popup ────────────────────────────────────────────────────
    if (festivalPending) {
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
        float popW = 460.0f * globalScale;
        float popH = 300.0f * globalScale;
        float popX = (sw2 - popW) / 2.0f;
        float popY = (sh2 - popH) / 2.0f;

        float btnW2 = popW - 40.0f * globalScale;
        float rowH = 30.0f * globalScale;
        float cancelW = 80.0f * globalScale;
        float cancelH = 28.0f * globalScale;

        btnFestivalCancel.loadAsText("BATAL", popX + popW - cancelW - 10.0f * globalScale,
                                     popY + popH - cancelH - 10.0f * globalScale,
                                     cancelW, cancelH,
                                     {120,50,45,255},{160,70,65,255},WHITE);
        if (btnFestivalCancel.isClicked()) {
            festivalPending = false;
            festivalProps.clear();
            btnFestivalProps.clear();
            return;
        }

        // create property buttons if not yet
        if (btnFestivalProps.empty() && !festivalProps.empty()) {
            for (int i = 0; i < (int)festivalProps.size(); i++) {
                auto* prop = festivalProps[i];
                char buf[128];
                int rent = RentManager::computeRent(*prop, gs->dice.getTotal(), gs->config, gs->tiles);
                std::snprintf(buf, sizeof(buf), "%s (%s) Sewa: M%d",
                              prop->getTileName().c_str(), prop->getTileCode().c_str(), rent);
                Button b;
                b.loadAsText(buf, popX + 20.0f * globalScale,
                             popY + 50.0f * globalScale + i * (rowH + 4.0f),
                             btnW2, rowH,
                             {244,206,43,255},{250,220,70,255},{80,40,35,255});
                btnFestivalProps.push_back(b);
            }
        }
        for (int i = 0; i < (int)btnFestivalProps.size(); i++) {
            if (btnFestivalProps[i].isClicked()) {
                auto* prop = festivalProps[i];
                cardProc->cmdFestival(*gs->currentTurnPlayer(), prop->getTileCode());
                gs->addLog(*gs->currentTurnPlayer(), "FESTIVAL", prop->getTileCode());
                drainAndStoreEvents();
                festivalPending = false;
                festivalProps.clear();
                btnFestivalProps.clear();
                break;
            }
        }
        return;
    }

    // ── build popup ───────────────────────────────────────────────────────
    if (buildMode) {
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
        float popW = 420.0f * globalScale;
        float popH = 320.0f * globalScale;
        float popX = (sw2 - popW) / 2.0f;
        float popY = (sh2 - popH) / 2.0f;
        float rowH = 32.0f * globalScale;
        float btnW2 = popW - 40.0f * globalScale;
        float cancelW = 80.0f * globalScale;
        float cancelH = 28.0f * globalScale;

        btnBuildCancel.loadAsText("BATAL", popX + popW - cancelW - 10.0f * globalScale,
                                  popY + popH - cancelH - 10.0f * globalScale,
                                  cancelW, cancelH,
                                  {120,50,45,255},{160,70,65,255},WHITE);
        if (btnBuildCancel.isClicked()) {
            buildMode = false; buildStep = 0; buildSelectedProp = nullptr;
            return;
        }

        if (buildStep == 0) {
            // create group buttons if not yet
            if (btnBuildGroups.empty() && !buildGroupNames.empty()) {
                for (int i = 0; i < (int)buildGroupNames.size(); i++) {
                    Button b;
                    b.loadAsText(buildGroupNames[i].c_str(),
                                 popX + 20.0f * globalScale,
                                 popY + 50.0f * globalScale + i * (rowH + 4.0f),
                                 btnW2, rowH,
                                 {244,206,43,255},{250,220,70,255},{80,40,35,255});
                    btnBuildGroups.push_back(b);
                }
            }
            for (int i = 0; i < (int)btnBuildGroups.size(); i++) {
                if (btnBuildGroups[i].isClicked()) {
                    buildSelectedGroup = buildGroupNames[i];
                    buildStep = 1;
                    btnBuildProps.clear();
                    auto& props = buildGroupProps[buildSelectedGroup];
                    for (int j = 0; j < (int)props.size(); j++) {
                        auto* prop = props[j];
                        int lvl = prop->getLevel();
                        char buf[128];
                        int cost = (lvl >= 4) ? prop->getHotelCost() : prop->getHouseCost();
                        std::snprintf(buf, sizeof(buf), "%s (%s) Lvl:%d  M%d",
                                      prop->getTileName().c_str(), prop->getTileCode().c_str(), lvl, cost);
                        Button b;
                        b.loadAsText(buf,
                                     popX + 20.0f * globalScale,
                                     popY + 50.0f * globalScale + j * (rowH + 4.0f),
                                     btnW2, rowH,
                                     {244,206,43,255},{250,220,70,255},{80,40,35,255});
                        btnBuildProps.push_back(b);
                    }
                    break;
                }
            }
        } else if (buildStep == 1) {
            for (int i = 0; i < (int)btnBuildProps.size(); i++) {
                if (btnBuildProps[i].isClicked()) {
                    buildSelectedProp = buildGroupProps[buildSelectedGroup][i];
                    int lvl = buildSelectedProp->getLevel();
                    if (lvl >= 4) {
                        buildStep = 2; // confirmation
                    } else {
                        // build directly
                        try {
                            PropertyManager::build(*gs->currentTurnPlayer(), *buildSelectedProp);
                            gs->addLog(*gs->currentTurnPlayer(), "BANGUN", buildSelectedProp->getTileCode());
                            drainAndStoreEvents();
                        } catch (const UnablePayBuildException&) {
                            showPopup = true; popupTitle = "BUILD"; popupMsg = "Saldo tidak cukup!";
                        }
                        buildMode = false; buildStep = 0; buildSelectedProp = nullptr;
                    }
                    break;
                }
            }
        } else if (buildStep == 2) {
            // hotel confirmation
            float confW = 180.0f * globalScale, confH = 36.0f * globalScale;
            float confX = popX + (popW - confW) / 2.0f;
            float confY = popY + popH - confH - 50.0f * globalScale;
            btnBuildConfirm.loadAsText("UPGRADE HOTEL", confX, confY, confW, confH,
                                       {244,206,43,255},{250,220,70,255},{80,40,35,255});
            if (btnBuildConfirm.isClicked()) {
                try {
                    PropertyManager::build(*gs->currentTurnPlayer(), *buildSelectedProp);
                    gs->addLog(*gs->currentTurnPlayer(), "BANGUN", buildSelectedProp->getTileCode());
                    drainAndStoreEvents();
                } catch (const UnablePayBuildException&) {
                    showPopup = true; popupTitle = "BUILD"; popupMsg = "Saldo tidak cukup!";
                }
                buildMode = false; buildStep = 0; buildSelectedProp = nullptr;
            }
        }
        return;
    }

    // ── selection popup (LASSO / DEMOLITION) ─────────────────────────────
    if (showSelPopup) { updateSelPopup(); return; }

    // ── auto-play bot turn ────────────────────────────────────────────────
    if (currentPlayerIsBot() && !hasRolled) {
        Player* p = gs->currentTurnPlayer();
        botCtrl->executeTurn(*p);
        drainAndStoreEvents();
        gameLoop->checkWinGui();
        if (gs->game_over) { gameOverOverlay = true; return; }
        handleEndTurn();
        return;
    }

    // ── dice animation ────────────────────────────────────────────────────
    if (diceRolling) {
        diceRollTimer += dt;
        if (diceRollTimer >= diceRollInterval) {
            diceRollTimer = 0;
            diceRollFrame++;
            if (forceDice) {
                dice1 = setDice1;  // show forced values during animation
                dice2 = setDice2;
            } else {
                dice1 = GetRandomValue(1, 6);
                dice2 = GetRandomValue(1, 6);
            }
            int maxFrames = forceDice ? 4 : 12;  // fast animation for forced dice
            if (diceRollFrame >= maxFrames) {
                diceRolling   = false;
                diceRollFrame = 0;
                if (forceDice) {
                    forceDice = false;  // consume the forced roll
                }
                executeRollResult();
            }
        }
        return;
    }

    // ── auction mode ──────────────────────────────────────────────────────
    if (auctionMode) { updateAuction(); return; }

    // ── roll dice button ──────────────────────────────────────────────────
    if (btnRollDice.isClicked() && !hasRolled) {
        diceRolling      = true;
        diceRollTimer    = 0;
        diceRollFrame    = 0;
        diceRollInterval = 0.06f;
    }

    // ── action buttons (PLAY tab) ─────────────────────────────────────────
    if (hasRolled && activeTab == 0) {
        Player* p = gs->currentTurnPlayer();

        if (currentPlayerIsJailed()) {
            if (btnBuyProperty.isClicked()) {
                bankruptcy->cmdBayarDenda(*p);
                drainAndStoreEvents();
            }
        } else {
            if (btnBuyProperty.isClicked() && canBuyCurrentTile()) {
                BuyManager::buy(*p, *gs->tiles[p->getCurrTile()]);
                drainAndStoreEvents();
            }

            if (btnAuction.isClicked()) startAuction();

            if (btnBuildHouse.isClicked()) {
                auto options = PropertyManager::getBuildableOptions(*p, gs->board);
                if (options.empty()) {
                    showPopup = true; popupTitle = "BUILD";
                    popupMsg  = "Tidak ada properti yang\nbisa dibangun saat ini.";
                } else {
                    buildMode = true;
                    buildStep = 0;
                    buildGroupProps.clear();
                    buildGroupNames.clear();
                    btnBuildGroups.clear();
                    btnBuildProps.clear();
                    buildSelectedProp = nullptr;
                    for (auto& [grp, opts] : options) {
                        buildGroupNames.push_back(grp);
                        buildGroupProps[grp].clear();
                        for (auto& opt : opts) {
                            buildGroupProps[grp].push_back(opt.prop);
                        }
                    }
                }
            }

            if (btnMortgage.isClicked()) {
                int idx = (selectedTileIdx >= 0) ? selectedTileIdx : p->getCurrTile();
                auto* prop = (idx >= 0 && idx < (int)gs->tiles.size())
                             ? dynamic_cast<PropertyTile*>(gs->tiles[idx]) : nullptr;
                if (prop && prop->getTileOwner() == p) {
                    if (!PropertyManager::mortgage(*p, *prop)) {
                        showPopup = true; popupTitle = "MORTGAGE";
                        popupMsg  = "Tidak bisa menggadai\ntile ini.";
                    } else drainAndStoreEvents();
                }
            }

            if (btnRedeem.isClicked()) {
                int idx = (selectedTileIdx >= 0) ? selectedTileIdx : p->getCurrTile();
                auto* prop = (idx >= 0 && idx < (int)gs->tiles.size())
                             ? dynamic_cast<PropertyTile*>(gs->tiles[idx]) : nullptr;
                if (prop && prop->getTileOwner() == p && prop->isMortgage()) {
                    bankruptcy->cmdTebus(*p, prop->getTileCode());
                    drainAndStoreEvents();
                } else {
                    showPopup = true; popupTitle = "TEBUS";
                    popupMsg  = "Pilih properti milik Anda\nyang sedang digadaikan.";
                }
            }
        }

        // ── skill card buttons ────────────────────────────────────────────
        if (!p->isSkillUsed()) {
            int handSz = p->getHandSize();
            for (int ci = 0; ci < handSz && ci < 4; ci++) {
                if (btnSkillCards[ci].isClicked()) {
                    auto* card = p->getHandCard(ci);
                    if (!card) continue;
                    std::string t = card->getCardType();
                    if (t == "TELEPORT")    useCardWithTarget(ci, CardMode::TELEPORT_SELECT);
                    else if (t == "LASSO")  useCardWithTarget(ci, CardMode::LASSO_SELECT);
                    else if (t == "DEMOLITION") useCardWithTarget(ci, CardMode::DEMOLITION_SELECT);
                    else                    useCardImmediate(ci);
                }
            }
        }
    }

    // ── end turn ──────────────────────────────────────────────────────────
    if (btnEndTurn.isClicked() && hasRolled && !diceRolling)
        handleEndTurn();

    if (IsWindowResized()) computeLayout();
}

// ─── draw ─────────────────────────────────────────────────────────────────────
void GameScreen::draw() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // background
    float bgScale = std::max(sw / (float)blurredBgTexture.width,
                             sh / (float)blurredBgTexture.height);
    float bgX = (sw - blurredBgTexture.width * bgScale) / 2.0f;
    float bgY = (sh - blurredBgTexture.height * bgScale) / 2.0f;
    DrawTextureEx(blurredBgTexture, {bgX, bgY}, 0.0f, bgScale, WHITE);

    // board
    DrawTextureEx(boardTexture, {boardX, boardY}, 0.0f, boardScale, WHITE);

    // ── player icons on board ─────────────────────────────────────────────
    int numPlayers = gs ? (int)gs->players.size() : 0;
    // draw no-background icons on board — forced to uniform size
    float targetIconSize = 32.0f * globalScale;
    for (int i = 0; i < numPlayers; i++) {
        Player* p = gs->players[i];
        if (p->getStatus() == "BANKRUPT") continue;

        int tileIdx = p->getCurrTile();
        if (tileIdx < 0 || tileIdx >= 40) continue;
        float cx = tileCenters[tileIdx][0];
        float cy = tileCenters[tileIdx][1];

        // force each icon to the same target size regardless of source dimensions
        float scX = targetIconSize / (float)playerIconsNb[i % 4].width;
        float scY = targetIconSize / (float)playerIconsNb[i % 4].height;
        float sc  = std::min(scX, scY);
        float iw  = playerIconsNb[i % 4].width  * sc;
        float ih  = playerIconsNb[i % 4].height * sc;

        // custom offset per player to avoid overlap
        float offX = 0.0f, offY = -0.2f * ih;
        if      (i == 0) { offX = -0.8f * iw; }          // gru: more to the left
        else if (i == 1) { offX = -5.0f; }                // minion: center
        else if (i == 2) { offX = +0.1f * iw; }          // purple: right
        else if (i == 3) { offX = +0.7f * iw; offY = +5.0f; } // banana: beside purple, slightly lower

        float dx = cx - iw / 2.0f + offX;
        float dy = cy - ih / 2.0f + offY + 8.0f * globalScale;
        if (i == 3) {
            // banana: rotate vertical (90 deg) with center pivot
            DrawTexturePro(playerIconsNb[3],
                           {0, 0, (float)playerIconsNb[3].width, (float)playerIconsNb[3].height},
                           {dx, dy, iw, ih},
                           {iw / 2.0f, ih / 2.0f},
                            -90.0f, WHITE);
        } else {
            DrawTextureEx(playerIconsNb[i % 4], {dx, dy}, 0.0f, sc, WHITE);
        }
    }

    // ── card panel ────────────────────────────────────────────────────────
    float boardRight = boardX + boardTexture.width * boardScale;
    float availW     = sw - boardRight - 10.0f;
    float cardScale  = std::min(availW / (float)cardPanel.width,
                                (sh - 20.0f) / (float)cardPanel.height);
    if (cardScale < 0.1f) cardScale = 0.1f;
    float cardX = boardRight + (availW - cardPanel.width * cardScale) * 0.65f;
    float cardY = (sh - cardPanel.height * cardScale) / 2.0f;
    DrawTextureEx(cardPanel, {cardX, cardY}, 0.0f, cardScale, WHITE);

    btnPlay.draw(); btnAssets.draw(); btnPlayers.draw(); btnLog.draw();

    // ── tab content ───────────────────────────────────────────────────────
    if      (activeTab == 0) drawPlayTab(cardX, cardScale);
    else if (activeTab == 1) drawAssetsTab(cardX, cardScale);
    else if (activeTab == 2) drawPlayersTab(cardX, cardScale);
    else if (activeTab == 3) drawLogTab(cardX, cardScale);

    // ── current player header (always visible) ────────────────────────────
    if (gs) {
        int activeIdx = gs->active_player_id;
        if (activeIdx >= 0 && activeIdx < numPlayers) {
            Player* cur = gs->players[activeIdx];
            float iconSz      = cardPanel.width * cardScale * 0.15f;
            float iconStartX  = cardX + (cardPanel.width * cardScale - iconSz) - 306.0f;
            float iconY       = btnPlay.getY() + btnPlay.getHeight() - 198.0f * globalScale;
            float iconScaleVal = iconSz / (float)playerIconsB[activeIdx % 4].width;
            DrawRectangleRec({iconStartX - 3, iconY - 3, iconSz + 6, iconSz + 6},
                             {255,235,202,255});
            DrawRectangleLinesEx({iconStartX - 3, iconY - 2, iconSz + 6, iconSz + 6},
                                 2, {148,73,68,255});
            DrawTextureEx(playerIconsB[activeIdx % 4], {iconStartX, iconY}, 0.0f,
                          iconScaleVal, WHITE);

            // nama + uang di bawah icon
            float nameX  = iconStartX + iconSz + 10.0f * globalScale;
            float nameY  = btnPlay.getY() + btnPlay.getHeight() - 194.0f * globalScale;
            int   nameSz = (int)(35 * globalScale);
            DrawText(cur->getUsername().c_str(), (int)nameX, (int)nameY, nameSz, BLACK);

            // balance below name
            char balBuf[32];
            std::snprintf(balBuf, sizeof(balBuf), "M%d", cur->getBalance());
            int balSz = (int)(18 * globalScale);
            DrawText(balBuf, (int)nameX, (int)(nameY + nameSz + 2), balSz, {80,40,35,255});

            // jail indicator
            if (cur->getStatus() == "JAIL") {
                int jSz = (int)(13 * globalScale);
                DrawText("[PENJARA]", (int)nameX, (int)(nameY + nameSz + balSz + 4), jSz,
                         {200,50,50,255});
            }
        }
    }

    // ── dice ──────────────────────────────────────────────────────────────
    {
        float diceSz = cardPanel.width * cardScale * 0.14f;
        float diceGap = 10.0f * globalScale;
        float iconSz = cardPanel.width * cardScale * 0.15f;
        float iconY  = btnPlay.getY() + btnPlay.getHeight() - 198.0f * globalScale;
        float playerNameBottom = iconY + iconSz + 10.0f * globalScale;
        float diceY  = playerNameBottom + 21.0f * globalScale;
        float totalW = diceSz + diceSz + diceGap;
        float d1x    = cardX + (cardPanel.width * cardScale - totalW) / 2.0f - 105.0f * globalScale;
        float d2x    = d1x + diceSz + diceGap;
        int d1i = std::max(0, std::min(5, dice1 - 1));
        int d2i = std::max(0, std::min(5, dice2 - 1));
        DrawTextureEx(diceTextures[d1i], {d1x, diceY}, 0.0f,
                      diceSz / (float)diceTextures[d1i].width, WHITE);
        DrawTextureEx(diceTextures[d2i], {d2x, diceY}, 0.0f,
                      diceSz / (float)diceTextures[d2i].width, WHITE);
    }

    btnRollDice.draw();
    Color rollBorder = btnRollDice.isDisabled() ? Color{210,210,210,180} : Color{80,40,35,255};
    DrawRectangleLinesEx({btnRollDice.getX(), btnRollDice.getY(),
                          btnRollDice.getWidth(), btnRollDice.getHeight()}, 2, rollBorder);
    btnEndTurn.draw();
    Color endBorder = btnEndTurn.isDisabled() ? Color{210,210,210,180} : Color{80,40,35,255};
    DrawRectangleLinesEx({btnEndTurn.getX(), btnEndTurn.getY(),
                          btnEndTurn.getWidth(), btnEndTurn.getHeight()}, 2, endBorder);

    // save button (always visible)
    btnSaveGame.draw();

    // ATUR DADU button
    btnSetDice.setActive(setDiceMode || forceDice);
    btnSetDice.draw();



    // ── ATUR DADU popup ───────────────────────────────────────────────────
    if (setDiceMode) {
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
        DrawRectangle(0, 0, sw2, sh2, {0,0,0,150});

        float popW = 300.0f * globalScale;
        float popH = 190.0f * globalScale;
        float popX = (sw2 - popW) / 2.0f;
        float popY = (sh2 - popH) / 2.0f;

        DrawRectangleRounded({popX, popY, popW, popH}, 0.15f, 8, {40,25,20,240});
        DrawRectangleLinesEx({popX, popY, popW, popH}, 2.0f, {220,180,100,255});

        int titleSz = (int)(18 * globalScale);
        const char* title = "ATUR DADU";
        int tw2 = MeasureText(title, titleSz);
        DrawText(title, (int)(popX + (popW - tw2)/2.0f),
                 (int)(popY + 14.0f*globalScale), titleSz, {220,180,100,255});

        // die counter row
        float pmW    = 32.0f * globalScale;
        float numW   = 40.0f * globalScale;
        float cH2    = 34.0f * globalScale;
        float dGap   = 24.0f * globalScale;
        float totalW = 2.0f*(pmW + numW + pmW) + dGap;
        float cX2    = popX + (popW - totalW) / 2.0f;
        float cY2    = popY + 60.0f * globalScale;

        int numSz = (int)(cH2 * 0.55f);
        int lblSz = (int)(12 * globalScale);

        // D1
        DrawText("Dadu 1", (int)(cX2 + (pmW + numW + pmW - MeasureText("Dadu 1", lblSz))/2.0f),
                 (int)(cY2 - lblSz - 4), lblSz, {200,170,130,255});
        btnD1Minus.loadAsText("-", cX2, cY2, pmW, cH2, {80,50,40,255},{110,70,60,255},WHITE);
        btnD1Minus.draw();
        DrawRectangleRec({cX2+pmW, cY2, numW, cH2}, {255,243,210,255});
        DrawRectangleLinesEx({cX2+pmW, cY2, numW, cH2}, 1.5f, {150,100,70,255});
        char b1[4]; std::snprintf(b1, sizeof(b1), "%d", setDice1);
        int bw1 = MeasureText(b1, numSz);
        DrawText(b1, (int)(cX2+pmW + (numW-bw1)/2.0f), (int)(cY2+(cH2-numSz)/2.0f), numSz, BLACK);
        btnD1Plus.loadAsText("+", cX2+pmW+numW, cY2, pmW, cH2, {80,50,40,255},{110,70,60,255},WHITE);
        btnD1Plus.draw();

        // D2
        float d2X = cX2 + 2*pmW + numW + dGap;
        DrawText("Dadu 2", (int)(d2X + (pmW + numW + pmW - MeasureText("Dadu 2", lblSz))/2.0f),
                 (int)(cY2 - lblSz - 4), lblSz, {200,170,130,255});
        btnD2Minus.loadAsText("-", d2X, cY2, pmW, cH2, {80,50,40,255},{110,70,60,255},WHITE);
        btnD2Minus.draw();
        DrawRectangleRec({d2X+pmW, cY2, numW, cH2}, {255,243,210,255});
        DrawRectangleLinesEx({d2X+pmW, cY2, numW, cH2}, 1.5f, {150,100,70,255});
        char b2[4]; std::snprintf(b2, sizeof(b2), "%d", setDice2);
        int bw2 = MeasureText(b2, numSz);
        DrawText(b2, (int)(d2X+pmW + (numW-bw2)/2.0f), (int)(cY2+(cH2-numSz)/2.0f), numSz, BLACK);
        btnD2Plus.loadAsText("+", d2X+pmW+numW, cY2, pmW, cH2, {80,50,40,255},{110,70,60,255},WHITE);
        btnD2Plus.draw();

        // confirm + hint
        float okW = 90.0f * globalScale, okH = 30.0f * globalScale;
        btnConfirmDice.loadAsText("OK", popX + (popW - okW)/2.0f,
                                  cY2 + cH2 + 12.0f*globalScale, okW, okH,
                                  {70,140,80,255},{90,170,100,255},WHITE);
        btnConfirmDice.draw();

        // X close button — top-right of popup
        float xSz = 22.0f * globalScale;
        btnCloseDice.loadAsText("X", popX + popW - xSz - 6.0f*globalScale,
                                popY + 6.0f*globalScale, xSz, xSz,
                                {120,50,45,255},{160,70,65,255},WHITE);
        btnCloseDice.draw();
    }

    // TELEPORT hint overlay on board
    if (cardMode == CardMode::TELEPORT_SELECT) {
        float bw2 = boardTexture.width * boardScale;
        float bh2 = boardTexture.height * boardScale;
        DrawRectangle((int)boardX, (int)boardY, (int)bw2, (int)bh2, {0,0,200,40});
        int hSz = (int)(14 * globalScale);
        const char* h = "Klik tile tujuan teleport (ESC batal)";
        int hw2 = MeasureText(h, hSz);
        DrawText(h, (int)(boardX + (bw2 - hw2) / 2.0f),
                 (int)(boardY + bh2 - 24.0f * globalScale), hSz, WHITE);
    }

    // tile overlay
    if (selectedTileIdx >= 0 && cardMode == CardMode::NONE) drawTileOverlay();

    // popup
    if (showPopup) drawPopup();

    // selection popup (LASSO / DEMOLITION)
    if (showSelPopup) drawSelPopup();

    // PPH popup
    if (pphPending) {
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
        DrawRectangle(0, 0, sw2, sh2, {0,0,0,180});

        float popW = 360.0f * globalScale;
        float popH = 220.0f * globalScale;
        float popX = (sw2 - popW) / 2.0f;
        float popY = (sh2 - popH) / 2.0f;

        DrawRectangleRounded({popX, popY, popW, popH}, 0.12f, 8, {255,243,210,255});
        DrawRectangleRoundedLines({popX, popY, popW, popH}, 0.12f, 8, {80,40,35,255});

        int titleSz = (int)(20 * globalScale);
        const char* title = "PAJAK PENGHASILAN";
        int tw2 = MeasureText(title, titleSz);
        DrawText(title, (int)(popX + (popW - tw2)/2.0f), (int)(popY + 18.0f*globalScale), titleSz, {148,73,68,255});

        int subSz = (int)(14 * globalScale);
        const char* msg = "Pilih cara pembayaran:";
        int mw = MeasureText(msg, subSz);
        DrawText(msg, (int)(popX + (popW - mw)/2.0f), (int)(popY + 55.0f*globalScale), subSz, BLACK);

        btnPphFlat.draw();
        btnPphPct.draw();
    }

    // build popup
    if (buildMode) {
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
        DrawRectangle(0, 0, sw2, sh2, {0,0,0,180});

        float popW = 460.0f * globalScale;
        float popH = 360.0f * globalScale;
        float popX = (sw2 - popW) / 2.0f;
        float popY = (sh2 - popH) / 2.0f;

        DrawRectangleRounded({popX, popY, popW, popH}, 0.12f, 8, {255,243,210,255});
        DrawRectangleRoundedLines({popX, popY, popW, popH}, 0.12f, 8, {80,40,35,255});

        int titleSz = (int)(20 * globalScale);
        int tw2 = MeasureText("BANGUN", titleSz);
        DrawText("BANGUN", (int)(popX + (popW - tw2)/2.0f), (int)(popY + 16.0f*globalScale), titleSz, {148,73,68,255});

        int subSz = (int)(13 * globalScale);
        if (buildStep == 0) {
            const char* msg = "Pilih Color Group:";
            DrawText(msg, (int)(popX + 20.0f*globalScale), (int)(popY + 46.0f*globalScale), subSz, BLACK);
            for (auto& b : btnBuildGroups) b.draw();
        } else if (buildStep == 1) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "Pilih properti [%s]:", buildSelectedGroup.c_str());
            DrawText(buf, (int)(popX + 20.0f*globalScale), (int)(popY + 46.0f*globalScale), subSz, BLACK);
            for (auto& b : btnBuildProps) b.draw();
        } else if (buildStep == 2) {
            const char* msg = "Konfirmasi upgrade ke Hotel?";
            int mw = MeasureText(msg, subSz);
            DrawText(msg, (int)(popX + (popW - mw)/2.0f), (int)(popY + 100.0f*globalScale), subSz, BLACK);
            if (buildSelectedProp) {
                char costBuf[64];
                std::snprintf(costBuf, sizeof(costBuf), "Biaya: M%d", buildSelectedProp->getHotelCost());
                int cw = MeasureText(costBuf, subSz);
                DrawText(costBuf, (int)(popX + (popW - cw)/2.0f), (int)(popY + 130.0f*globalScale), subSz, {80,40,35,255});
            }
            btnBuildConfirm.draw();
        }
        btnBuildCancel.draw();
    }

    // festival popup
    if (festivalPending) {
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
        DrawRectangle(0, 0, sw2, sh2, {0,0,0,180});

        float popW = 460.0f * globalScale;
        float popH = 300.0f * globalScale;
        float popX = (sw2 - popW) / 2.0f;
        float popY = (sh2 - popH) / 2.0f;

        DrawRectangleRounded({popX, popY, popW, popH}, 0.12f, 8, {255,243,210,255});
        DrawRectangleRoundedLines({popX, popY, popW, popH}, 0.12f, 8, {80,40,35,255});

        int titleSz = (int)(20 * globalScale);
        int tw2 = MeasureText("FESTIVAL", titleSz);
        DrawText("FESTIVAL", (int)(popX + (popW - tw2)/2.0f), (int)(popY + 16.0f*globalScale), titleSz, {148,73,68,255});

        int subSz = (int)(13 * globalScale);
        const char* msg = "Pilih properti untuk festival:";
        DrawText(msg, (int)(popX + 20.0f*globalScale), (int)(popY + 46.0f*globalScale), subSz, BLACK);
        for (auto& b : btnFestivalProps) b.draw();
        btnFestivalCancel.draw();
    }

    // game over
    if (gameOverOverlay) drawGameOverOverlay();
}

// ─── drawPlayTab ──────────────────────────────────────────────────────────────
void GameScreen::drawPlayTab(float cardX, float cardScale) {
    float boxPad = 12.0f * globalScale;
    float boxX   = cardX + boxPad + 20.0f;
    float boxY   = btnPlay.getY() + btnPlay.getHeight() + boxPad + 10.0f;
    float boxW   = cardPanel.width * cardScale - 6.0f * boxPad;
    float boxH   = 100.0f * globalScale;

    DrawRectangleRec({boxX, boxY, boxW, boxH}, {255,243,210,255});
    DrawRectangleLinesEx({boxX, boxY, boxW, boxH}, 2.5f, {50,25,20,255});

    int titleSz = (int)(15 * globalScale);
    int subSz   = (int)(12 * globalScale);

    float textX = boxX + 8*globalScale;
    float textY = boxY + 8*globalScale;
    if (diceRolling) {
        DrawText("ROLLING...", (int)textX, (int)textY, titleSz, BLACK);
    } else if (hasRolled) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%d + %d = %d", dice1, dice2, dice1 + dice2);
        DrawText("DICE RESULT", (int)textX, (int)textY, titleSz, BLACK);
        textY += titleSz + 4*globalScale;
        DrawText(buf, (int)textX, (int)textY, subSz, {80,40,35,255});
        textY += subSz + 4*globalScale;

        // multi-line lastEvent with simple word-wrap
        if (!lastEvent.empty()) {
            int maxLineW = (int)(boxW - 16*globalScale);
            std::string remaining = lastEvent;
            while (!remaining.empty() && textY < boxY + boxH - subSz) {
                // binary search for max chars that fit
                int low = 1, high = (int)remaining.size(), best = 0;
                while (low <= high) {
                    int mid = (low + high) / 2;
                    if (MeasureText(remaining.substr(0, mid).c_str(), subSz) <= maxLineW) {
                        best = mid; low = mid + 1;
                    } else {
                        high = mid - 1;
                    }
                }
                // break at space if possible
                int breakAt = best;
                if (breakAt < (int)remaining.size()) {
                    size_t spacePos = remaining.rfind(' ', breakAt);
                    if (spacePos != std::string::npos && spacePos > 0) breakAt = (int)spacePos;
                }
                std::string line = remaining.substr(0, breakAt);
                DrawText(line.c_str(), (int)textX, (int)textY, subSz, {80,40,35,255});
                textY += subSz + 2*globalScale;
                // advance remaining
                while (breakAt < (int)remaining.size() && remaining[breakAt] == ' ') breakAt++;
                remaining = remaining.substr(breakAt);
            }
        }
    } else {
        const char* hint = currentPlayerIsJailed()
            ? "Di penjara! Lempar dadu atau bayar denda."
            : "Lempar dadu untuk bermain!";
        DrawText(hint, (int)textX, (int)(boxY + 16*globalScale),
                 subSz, {120,80,75,255});
    }

    float actionsLabelY = boxY + boxH + 10.0f * globalScale;
    int labelSz = (int)(13 * globalScale);

    if (auctionMode) {
        drawAuctionUI(cardX, cardScale);
        return;
    }

    if (currentPlayerIsJailed() && hasRolled) {
        DrawText("AKSI PENJARA", (int)boxX, (int)actionsLabelY, labelSz, {148,73,68,255});
        Color border = btnBuyProperty.isDisabled() ? Color{170,170,170,180} : Color{80,40,35,255};
        btnBuyProperty.draw();
        DrawRectangleLinesEx({btnBuyProperty.getX(), btnBuyProperty.getY(),
                              btnBuyProperty.getWidth(), btnBuyProperty.getHeight()}, 1.5f, border);
    } else {
        DrawText("ACTIONS", (int)boxX, (int)actionsLabelY, labelSz, {148,73,68,255});
        auto drawBtn = [&](Button& btn) {
            btn.draw();
            Color border = btn.isDisabled() ? Color{170,170,170,180} : Color{80,40,35,255};
            DrawRectangleLinesEx({btn.getX(), btn.getY(), btn.getWidth(), btn.getHeight()},
                                 1.5f, border);
        };
        drawBtn(btnBuyProperty);
        drawBtn(btnAuction);
        drawBtn(btnBuildHouse);
        drawBtn(btnMortgage);
        drawBtn(btnRedeem);
    }

    // skill cards section — below action buttons
    float cardSectionY = btnRedeem.getY() + btnRedeem.getHeight() + 10.0f * globalScale;
    drawSkillCardSection(cardX, cardScale, cardSectionY);
}

// ─── drawAssetsTab ────────────────────────────────────────────────────────────
void GameScreen::drawAssetsTab(float cardX, float cardScale) {
    if (!gs) return;
    Player* p = gs->currentTurnPlayer();
    if (!p) return;

    float pad   = 12.0f * globalScale;
    float listX = cardX + pad + 20.0f;
    float listY = btnPlay.getY() + btnPlay.getHeight() + pad + 10.0f;
    float listW = cardPanel.width * cardScale - 6.0f * pad;
    float rowH  = 24.0f * globalScale;
    int   rowSz = (int)(13 * globalScale);

    DrawText("Properti Dimiliki:", (int)listX, (int)listY, rowSz + 2, {80,40,35,255});
    listY += rowH + 4.0f;

    auto props = p->getOwnedProperties();
    if (props.empty()) {
        DrawText("Belum ada properti.", (int)listX, (int)listY, rowSz, {120,80,75,255});
    } else {
        float maxY = listY + (cardPanel.height * cardScale) * 0.52f;
        for (auto* prop : props) {
            if (listY > maxY) break;
            std::string tp = prop->getTileType();
            std::string code = prop->getTileCode();

            // find tileData index by code
            int tidx = -1;
            for (int k = 0; k < 40; k++) if (tileData[k].code == code) { tidx = k; break; }

            // color strip / icon on the left
            float stripW = 18.0f * globalScale;
            float stripH = rowH - 4.0f * globalScale;
            float stripX = listX;
            float stripY = listY + 2.0f * globalScale;

            if (tp == "STREET" && tidx >= 0) {
                Color c = getColorGroupColor(tileData[tidx].colorGroup);
                DrawRectangleRec({stripX, stripY, stripW, stripH}, c);
            } else if (tp == "RAILROAD") {
                float sc = stripH / (float)trainTexture.height;
                DrawTextureEx(trainTexture, {stripX, stripY}, 0.0f, sc, WHITE);
            } else if (tp == "UTILITY") {
                // PLN or PAM - check name
                bool isPln = (prop->getTileName().find("PLN") != std::string::npos) ||
                             (prop->getTileName().find("Listrik") != std::string::npos);
                Texture2D& utex = isPln ? plnTexture : pamTexture;
                float sc = stripH / (float)utex.height;
                DrawTextureEx(utex, {stripX, stripY}, 0.0f, sc, WHITE);
            } else {
                DrawRectangleRec({stripX, stripY, stripW, stripH}, {180,180,180,255});
            }
            DrawRectangleLinesEx({stripX, stripY, stripW, stripH}, 1, {80,40,35,255});

            // text
            float textX = stripX + stripW + 6.0f * globalScale;
            std::string info = prop->getTileCode() + " " + prop->getTileName();
            if (prop->isMortgage()) info += " [GADAI]";
            int lvl = prop->getLevel();
            if (lvl > 0 && lvl < 5) {
                char b[16]; std::snprintf(b, sizeof(b), " [%dR]", lvl);
                info += b;
            } else if (lvl == 5) {
                info += " [H]";
            }
            DrawText(info.c_str(), (int)textX, (int)listY, rowSz, BLACK);
            listY += rowH;
        }
    }
}

// ─── drawPlayersTab ───────────────────────────────────────────────────────────
void GameScreen::drawPlayersTab(float cardX, float cardScale) {
    if (!gs) return;
    static const Color playerCardColors[6] = {
        {200,60,60,255},{70,120,200,255},{70,160,90,255},
        {220,140,50,255},{140,80,180,255},{50,170,170,255},
    };

    float pad     = 12.0f * globalScale;
    float listX   = cardX + pad + 20.0f;
    float listY   = btnPlay.getY() + btnPlay.getHeight() + pad + 10.0f;
    float listW   = cardPanel.width * cardScale - 6.0f * pad;
    float cardH   = 58.0f * globalScale;
    float cardGap = 5.0f * globalScale;
    float iconBox = cardH - 8.0f * globalScale;
    int   nameSz  = (int)(15 * globalScale);
    int   statSz  = (int)(13 * globalScale);
    float statIconH = (float)(statSz + 4) * globalScale;

    int count = (int)gs->players.size();
    for (int i = 0; i < count; i++) {
        Player* p = gs->players[i];
        float cy  = listY + i * (cardH + cardGap);

        bool isCurrent = (i == gs->active_player_id);
        Color bg = isCurrent ? Color{255,243,160,255} : Color{255,243,210,255};
        DrawRectangleRec({listX, cy, listW, cardH}, bg);
        DrawRectangleLinesEx({listX, cy, listW, cardH}, 2.0f, {50,25,20,255});

        // icon box
        float ibX = listX + 4.0f * globalScale;
        float ibY = cy + (cardH - iconBox) / 2.0f;
        DrawRectangleRec({ibX, ibY, iconBox, iconBox}, playerCardColors[i % 4]);
        DrawTextureEx(playerIconsB[i % 4], {ibX, ibY}, 0.0f,
                      iconBox / (float)playerIconsB[i % 4].width, WHITE);

        float textX = ibX + iconBox + 8.0f * globalScale;
        float nameY = cy + 12.0f * globalScale;

        std::string displayName = p->getUsername();
        if (p->getStatus() == "BANKRUPT") displayName += " [BANGKRUT]";
        DrawText(displayName.c_str(), (int)textX, (int)nameY, nameSz, BLACK);
        if (isCurrent) {
            int nw    = MeasureText(displayName.c_str(), nameSz);
            int turnSz = (int)(10 * globalScale);
            DrawText("- giliran", (int)(textX + nw + 8*globalScale),
                     (int)(nameY + 1*globalScale), turnSz, {148,73,68,255});
        }

        // stats row: balance, houses, hotels
        float statsY = nameY + nameSz + 4.0f * globalScale;
        float cx2    = textX;

        auto drawStat = [&](Texture2D& tex, int cnt) {
            char buf[8]; std::snprintf(buf, sizeof(buf), "%d", cnt);
            float sc = statIconH / (float)tex.height;
            DrawTextureEx(tex, {cx2, statsY - 1.0f*globalScale}, 0.0f, sc, WHITE);
            cx2 += tex.width * sc + 2.0f * globalScale;
            DrawText(buf, (int)cx2, (int)statsY, statSz, BLACK);
            cx2 += MeasureText(buf, statSz) + 8.0f * globalScale;
        };

        char moneyBuf[32];
        std::snprintf(moneyBuf, sizeof(moneyBuf), "M%d", p->getBalance());
        DrawText(moneyBuf, (int)cx2, (int)statsY, statSz, BLACK);
        cx2 += MeasureText(moneyBuf, statSz) + 8.0f * globalScale;

        // count houses/hotels/railroad/utility from owned properties
        int houses = 0, hotels = 0, railroads = 0, utilities = 0;
        for (auto* prop : p->getOwnedProperties()) {
            int lvl = prop->getLevel();
            std::string tp = prop->getTileType();
            if (tp == "RAILROAD")      railroads++;
            else if (tp == "UTILITY")  utilities++;
            else if (lvl == 5)         hotels++;
            else if (lvl > 0)          houses += lvl;
        }
        drawStat(houseTexture,  houses);
        drawStat(hotelTexture,  hotels);
        drawStat(pamTexture,    utilities);
        drawStat(trainTexture,  railroads);
    }
}

// ─── drawLogTab ───────────────────────────────────────────────────────────────
void GameScreen::drawLogTab(float cardX, float cardScale) {
    if (!gs) return;
    float pad   = 12.0f * globalScale;
    float listX = cardX + pad + 20.0f;
    float listY = btnPlay.getY() + btnPlay.getHeight() + pad + 10.0f;
    (void)(cardPanel.width * cardScale - 6.0f * pad); // listW reserved for future use
    int   rowSz = (int)(11 * globalScale);
    float rowH  = (float)(rowSz + 4) * globalScale;

    DrawText("Log Transaksi:", (int)listX, (int)listY, rowSz + 2, {80,40,35,255});
    listY += rowH + 4.0f;

    auto& log = gs->transaction_log;
    float maxY = listY + (cardPanel.height * cardScale) * 0.52f;
    int start  = (int)log.size() > 16 ? (int)log.size() - 16 : 0;
    for (int i = (int)log.size() - 1; i >= start; i--) {
        if (listY > maxY) break;
        const auto& entry = log[i];
        char buf[128];
        std::snprintf(buf, sizeof(buf), "T%d [%s] %s",
                      entry.getTurn(),
                      entry.getUsername().c_str(),
                      entry.getActionType().c_str());
        DrawText(buf, (int)listX, (int)listY, rowSz, BLACK);
        listY += rowH;
    }
    if (log.empty())
        DrawText("Belum ada log.", (int)listX, (int)listY, rowSz, {120,80,75,255});
}

// ─── drawTileOverlay ─────────────────────────────────────────────────────────
void GameScreen::drawTileOverlay() {
    float bw = boardTexture.width * boardScale;
    float bh = boardTexture.height * boardScale;
    DrawRectangle((int)boardX, (int)boardY, (int)bw, (int)bh, {0,0,0,120});

    float aktaScale = bw * 0.42f / (float)aktaTexture.width;
    float aktaW = aktaTexture.width  * aktaScale;
    float aktaH = aktaTexture.height * aktaScale;
    float aktaX = boardX + (bw - aktaW) / 2.0f;
    float aktaY = boardY + (bh - aktaH) / 2.0f;
    DrawTextureEx(aktaTexture, {aktaX, aktaY}, 0.0f, aktaScale, WHITE);

    const TileInfo& info = tileData[selectedTileIdx];

    int nameSz = (int)(aktaH * 0.055f);
    int nameW  = MeasureText(info.name.c_str(), nameSz);
    DrawText(info.name.c_str(), (int)(aktaX + (aktaW - nameW) / 2.0f),
             (int)(aktaY + aktaH * 0.04f), nameSz, BLACK);

    int typeSz = (int)(aktaH * 0.030f);
    int typeW  = MeasureText(info.type.c_str(), typeSz);
    DrawText(info.type.c_str(), (int)(aktaX + (aktaW - typeW) / 2.0f),
             (int)(aktaY + aktaH * 0.13f), typeSz, {148,73,68,255});

    // ownership info from live game state
    if (gs && selectedTileIdx >= 0 && selectedTileIdx < (int)gs->tiles.size()) {
        auto* prop = dynamic_cast<PropertyTile*>(gs->tiles[selectedTileIdx]);
        if (prop && prop->getTileOwner()) {
            std::string ownerStr = "Milik: " + prop->getTileOwner()->getUsername();
            int owSz = (int)(aktaH * 0.028f);
            int owW  = MeasureText(ownerStr.c_str(), owSz);
            DrawText(ownerStr.c_str(), (int)(aktaX + (aktaW - owW) / 2.0f),
                     (int)(aktaY + aktaH * 0.195f), owSz, {70,120,200,255});
        }
    }

    float lx = aktaX + aktaW * 0.08f;
    float rx = aktaX + aktaW * 0.92f;
    float ry = aktaY + aktaH * 0.25f;
    float rH = aktaH * 0.052f;
    int   rSz = (int)(aktaH * 0.038f);
    Color lC = {80,50,40,255}, vC = {30,20,15,255}, sC = {148,73,68,255};

    char buf[64];
    auto drawRow = [&](const char* label, const char* value) {
        DrawText(label, (int)lx, (int)ry, rSz, lC);
        int vw = MeasureText(value, rSz);
        DrawText(value, (int)(rx - vw), (int)ry, rSz, vC);
        ry += rH;
    };
    auto drawDivider = [&]() {
        ry += rH * 0.1f;
        DrawLineEx({lx, ry}, {rx, ry}, 0.8f, {160,100,80,120});
        ry += rH * 0.4f;
    };
    auto drawSection = [&](const char* title) {
        DrawText(title, (int)lx, (int)ry, (int)(aktaH * 0.033f), sC);
        ry += rH * 0.75f;
    };

    if (info.type == "STREET") {
        std::snprintf(buf, sizeof(buf), "Rp %d", info.buyPrice);
        drawRow("Harga Beli",  buf);
        std::snprintf(buf, sizeof(buf), "Rp %d", info.mortgageValue);
        drawRow("Nilai Gadai", buf);
        drawDivider(); drawSection("SEWA");
        const char* rentLabels[6] = {"Lahan","1 Rumah","2 Rumah","3 Rumah","4 Rumah","Hotel"};
        for (int r = 0; r < 6; r++) {
            std::snprintf(buf, sizeof(buf), "Rp %d", info.rent[r]);
            drawRow(rentLabels[r], buf);
        }
        drawDivider(); drawSection("BIAYA PEMBANGUNAN");
        std::snprintf(buf, sizeof(buf), "Rp %d", info.houseCost);
        drawRow("Rumah", buf);
        std::snprintf(buf, sizeof(buf), "Rp %d", info.hotelCost);
        drawRow("Hotel", buf);
    } else if (info.type == "RAILROAD") {
        std::snprintf(buf, sizeof(buf), "Rp %d", info.mortgageValue);
        drawRow("Nilai Gadai", buf);
        drawRow("Cara Beli", "Gratis (mendarat)");
        drawDivider(); drawSection("SEWA");
        const int railRent[4] = {25,50,100,200};
        const char* railLabels[4] = {"1 Stasiun","2 Stasiun","3 Stasiun","4 Stasiun"};
        for (int r = 0; r < 4; r++) {
            std::snprintf(buf, sizeof(buf), "Rp %d", railRent[r]);
            drawRow(railLabels[r], buf);
        }
    } else if (info.type == "UTILITY") {
        std::snprintf(buf, sizeof(buf), "Rp %d", info.mortgageValue);
        drawRow("Nilai Gadai", buf);
        drawRow("Cara Beli", "Gratis (mendarat)");
        drawDivider(); drawSection("SEWA");
        drawRow("1 Utilitas", "Dadu x 4");
        drawRow("2 Utilitas", "Dadu x 10");
    } else {
        drawRow("Tipe", info.type.c_str());
    }

    int hintSz = (int)(aktaH * 0.028f);
    const char* hint = "Klik mana saja untuk tutup";
    int hintW = MeasureText(hint, hintSz);
    DrawText(hint, (int)(aktaX + (aktaW - hintW) / 2.0f),
             (int)(aktaY + aktaH * 0.90f), hintSz, {160,120,100,160});
}

// ─── drawPopup ────────────────────────────────────────────────────────────────
void GameScreen::drawPopup() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, {0,0,0,160});

    float popW = 380.0f * globalScale, popH = 200.0f * globalScale;
    float popX = (sw - popW) / 2.0f, popY = (sh - popH) / 2.0f;

    DrawRectangleRounded({popX, popY, popW, popH}, 0.15f, 8, {255,243,210,255});
    DrawRectangleRoundedLines({popX, popY, popW, popH}, 0.15f, 8, {80,40,35,255});

    int titleSz = (int)(20 * globalScale);
    int tw = MeasureText(popupTitle.c_str(), titleSz);
    DrawText(popupTitle.c_str(), (int)(popX + (popW - tw) / 2.0f),
             (int)(popY + 20.0f * globalScale), titleSz, {80,40,35,255});

    // draw message with newline support
    int msgSz = (int)(14 * globalScale);
    float msgY = popY + 20.0f * globalScale + titleSz + 12.0f * globalScale;
    std::string msg = popupMsg;
    size_t pos = 0;
    while (true) {
        size_t nl = msg.find('\n', pos);
        std::string line = (nl == std::string::npos) ? msg.substr(pos) : msg.substr(pos, nl - pos);
        int lw = MeasureText(line.c_str(), msgSz);
        DrawText(line.c_str(), (int)(popX + (popW - lw) / 2.0f), (int)msgY, msgSz, BLACK);
        msgY += msgSz + 6;
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }

    btnPopupOk.draw();
}

// ─── drawGameOverOverlay ──────────────────────────────────────────────────────
void GameScreen::drawGameOverOverlay() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, {0,0,0,200});

    float boxW = 500.0f * globalScale, boxH = 260.0f * globalScale;
    float boxX = (sw - boxW) / 2.0f, boxY = (sh - boxH) / 2.0f;
    DrawRectangleRounded({boxX, boxY, boxW, boxH}, 0.15f, 8, {255,243,210,255});
    DrawRectangleRoundedLines({boxX, boxY, boxW, boxH}, 0.15f, 8, {80,40,35,255});

    int titleSz = (int)(28 * globalScale);
    const char* title = "GAME OVER";
    int tw = MeasureText(title, titleSz);
    DrawText(title, (int)(boxX + (boxW - tw) / 2.0f),
             (int)(boxY + 24.0f * globalScale), titleSz, {148,73,68,255});

    // find winner
    std::string winner = "—";
    if (gs) {
        if (gs->game_over_by_bankruptcy) {
            for (auto* p : gs->players)
                if (p->getStatus() != "BANKRUPT") { winner = p->getUsername(); break; }
        } else {
            // max turn: richest player wins
            int best = -1;
            for (auto* p : gs->players) {
                if (p->getStatus() != "BANKRUPT" && p->getNetWorth() > best) {
                    best = p->getNetWorth(); winner = p->getUsername();
                }
            }
        }
    }

    int msgSz = (int)(18 * globalScale);
    std::string winMsg = "Pemenang: " + winner;
    int mw = MeasureText(winMsg.c_str(), msgSz);
    DrawText(winMsg.c_str(), (int)(boxX + (boxW - mw) / 2.0f),
             (int)(boxY + 24.0f*globalScale + titleSz + 18.0f*globalScale), msgSz, BLACK);

    int hintSz = (int)(13 * globalScale);
    const char* hint = "Tekan ESC atau ENTER untuk kembali ke menu";
    int hw = MeasureText(hint, hintSz);
    DrawText(hint, (int)(boxX + (boxW - hw) / 2.0f),
             (int)(boxY + boxH - 40.0f*globalScale), hintSz, {120,80,75,255});
}

// ══════════════════════════════════════════════════════════════════════════════
// AUCTION
// ══════════════════════════════════════════════════════════════════════════════
void GameScreen::startAuction() {
    if (!gs) return;
    Player* trigger = gs->currentTurnPlayer();
    int tileIdx = trigger->getCurrTile();
    auctionProp = dynamic_cast<PropertyTile*>(gs->tiles[tileIdx]);
    if (!auctionProp) return;  // not a property tile

    // build bidder order: start from player AFTER trigger in turn_order
    auctionOrder.clear();
    int sz = (int)gs->turn_order.size();
    int trigIdx = 0;
    for (int i = 0; i < sz; i++) {
        if (gs->turn_order[i] == trigger) { trigIdx = i; break; }
    }
    for (int i = 1; i <= sz; i++) {
        Player* pl = gs->turn_order[(trigIdx + i) % sz];
        if (pl->getStatus() != "BANKRUPT") auctionOrder.push_back(pl);
    }
    // trigger can also bid (added last)
    auctionOrder.push_back(trigger);

    auctionIdx = 0;
    auctionHighBid = 1;
    auctionHighBidder = nullptr;
    auctionPassed.assign(auctionOrder.size(), false);
    auctionInputLen = 0;
    auctionInputBuf[0] = '\0';
    auctionMode = true;
}

void GameScreen::updateAuction() {
    if (auctionOrder.empty()) { auctionMode = false; return; }

    // text input for bid
    int key = GetCharPressed();
    while (key > 0) {
        if (key >= '0' && key <= '9' && auctionInputLen < 10) {
            auctionInputBuf[auctionInputLen++] = (char)key;
            auctionInputBuf[auctionInputLen] = '\0';
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && auctionInputLen > 0)
        auctionInputBuf[--auctionInputLen] = '\0';

    Player* bidder = auctionOrder[auctionIdx];

    if (btnAuctionBid.isClicked() && auctionInputLen > 0) {
        int bid = std::stoi(std::string(auctionInputBuf));
        if (bid > auctionHighBid && bid <= bidder->getBalance()) {
            auctionHighBid = bid;
            auctionHighBidder = bidder;
            auctionInputLen = 0; auctionInputBuf[0] = '\0';
            // advance to next non-passed bidder
            int next = (auctionIdx + 1) % (int)auctionOrder.size();
            while (auctionPassed[next] && next != auctionIdx)
                next = (next + 1) % (int)auctionOrder.size();
            auctionIdx = next;
        } else {
            lastEvent = "Tawaran harus > " + std::to_string(auctionHighBid) + " dan <= saldo.";
        }
    }

    if (btnAuctionPass.isClicked()) {
        auctionPassed[auctionIdx] = true;
        auctionInputLen = 0; auctionInputBuf[0] = '\0';
        int next = (auctionIdx + 1) % (int)auctionOrder.size();
        while (auctionPassed[next] && next != auctionIdx)
            next = (next + 1) % (int)auctionOrder.size();
        auctionIdx = next;
    }

    // count remaining (non-passed) bidders
    int remaining = 0;
    for (bool p : auctionPassed) if (!p) remaining++;

    if (remaining <= 1) {
        // auction ends
        if (auctionHighBidder) {
            auctionHighBidder->operator+=(-auctionHighBid);
            auctionHighBidder->addOwnedProperty(auctionProp);
            gs->recomputeMonopolyForGroup(auctionProp->getColorGroup());
            lastEvent = auctionHighBidder->getUsername() + " menang lelang M"
                        + std::to_string(auctionHighBid);
            gs->addLog(*auctionHighBidder, "LELANG", auctionProp->getTileCode()
                       + " M" + std::to_string(auctionHighBid));
        } else {
            lastEvent = "Tidak ada pemenang lelang.";
        }
        auctionMode = false;
        auctionProp = nullptr;
    }
}

void GameScreen::drawAuctionUI(float /*cardX*/, float /*cardScale*/) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, {0,0,0,180});

    float pad  = 12.0f * globalScale;
    float boxW = 420.0f * globalScale;
    float boxH = 320.0f * globalScale;
    float boxX = (sw - boxW) / 2.0f;
    float boxY = (sh - boxH) / 2.0f;

    DrawRectangleRounded({boxX, boxY, boxW, boxH}, 0.12f, 8, {255,243,210,255});
    DrawRectangleRoundedLines({boxX, boxY, boxW, boxH}, 0.12f, 8, {80,40,35,255});

    int  titleSz = (int)(18 * globalScale);
    int  subSz   = (int)(14 * globalScale);
    float rowH   = (float)(subSz + 6) * globalScale;
    float cy = boxY + 20.0f * globalScale;
    float cx = boxX + 20.0f * globalScale;

    DrawText("LELANG", (int)cx, (int)cy, titleSz, {148,73,68,255});
    cy += titleSz + 10;
    if (auctionProp) {
        std::string pname = auctionProp->getTileName();
        DrawText(pname.c_str(), (int)cx, (int)cy, titleSz - 2, BLACK);
        cy += titleSz + 6;
    }

    char buf[64];
    if (auctionHighBidder) {
        std::snprintf(buf, sizeof(buf), "Tertinggi: M%d oleh %s",
                      auctionHighBid, auctionHighBidder->getUsername().c_str());
    } else {
        std::snprintf(buf, sizeof(buf), "Harga awal: M%d", auctionHighBid);
    }
    DrawText(buf, (int)cx, (int)cy, subSz, {80,40,35,255});
    cy += rowH;

    // show each bidder status
    for (int i = 0; i < (int)auctionOrder.size(); i++) {
        Color c = auctionPassed[i] ? Color{180,180,180,255} :
                  (i == auctionIdx ? Color{70,160,90,255} : Color{40,40,40,255});
        std::string s = (i == auctionIdx ? "> " : "  ")
                       + auctionOrder[i]->getUsername()
                       + (auctionPassed[i] ? " [PASS]" : "");
        DrawText(s.c_str(), (int)cx, (int)cy, subSz, c);
        cy += rowH;
    }
    cy += 8.0f * globalScale;

    // bid input
    DrawText("Tawaran:", (int)cx, (int)cy, subSz, BLACK);
    float inX = cx + 90.0f * globalScale;
    float inW = boxW * 0.4f, inH = 26.0f * globalScale;
    DrawRectangleRec({inX, cy - 2, inW, inH}, {255,243,210,255});
    DrawRectangleLinesEx({inX, cy - 2, inW, inH}, 1.5f, {80,40,35,255});
    std::string inp(auctionInputBuf);
    if ((int)GetTime() % 2 == 0) inp += "|";
    DrawText(inp.c_str(), (int)(inX + 4), (int)cy, subSz, BLACK);
    cy += inH + 10.0f * globalScale;

    // reposition buttons to center
    float bidW = 90.0f * globalScale, bidH = 32.0f * globalScale;
    float gap  = 12.0f * globalScale;
    float totalW = bidW + gap + bidW;
    float btnX = boxX + (boxW - totalW) / 2.0f;
    btnAuctionBid.loadAsText("BID", btnX, cy, bidW, bidH,
                             {244,206,43,255},{250,220,70,255},{80,40,35,255});
    btnAuctionPass.loadAsText("PASS", btnX + bidW + gap, cy, bidW, bidH,
                              {255,235,202,255},{255,240,215,255},{80,40,35,255});
    btnAuctionBid.draw();
    btnAuctionPass.draw();
}

// ══════════════════════════════════════════════════════════════════════════════
// SKILL CARDS
// ══════════════════════════════════════════════════════════════════════════════
void GameScreen::useCardImmediate(int cardIdx) {
    Player* p = gs->currentTurnPlayer();
    cardProc->cmdGunakanKemampuan(*p, cardIdx);
    // if card was MOVE, player position changed — apply landing
    drainAndStoreEvents();
    // after MOVE card, apply landing on new tile
    auto* card = p->getHandCard(cardIdx); // might be null if already removed
    // use via cardProc already removed it and set skillUsed, just apply landing if needed
    // (cmdGunakanKemampuan for MOVE calls applyGeneric which does action — move player)
    // apply landing always safe (might be no-op for non-movement cards)
    landing->applyLanding(*p);
    drainAndStoreEvents();
    gameLoop->checkWinGui();
    if (gs->game_over) gameOverOverlay = true;
    (void)card;
}

void GameScreen::useCardWithTarget(int cardIdx, CardMode mode) {
    Player* p = gs->currentTurnPlayer();
    if (mode == CardMode::TELEPORT_SELECT) {
        cardMode        = mode;
        pendingCardIdx  = cardIdx;
        cardSelectMsg   = "Klik tile tujuan teleport";
    } else if (mode == CardMode::LASSO_SELECT) {
        // build options list
        std::vector<std::string> opts;
        for (auto* pl : gs->players) {
            if (pl != p && pl->getStatus() != "BANKRUPT")
                opts.push_back(pl->getUsername());
        }
        if (opts.empty()) { lastEvent = "Tidak ada pemain lain."; return; }
        pendingCardIdx = cardIdx;
        cardMode       = mode;
        openSelPopup("LASSO — Pilih Pemain", opts);
    } else if (mode == CardMode::DEMOLITION_SELECT) {
        // step 1: pick opponent who has buildings
        std::vector<std::string> opts;
        for (auto* pl : gs->players) {
            if (pl == p || pl->getStatus() == "BANKRUPT") continue;
            for (auto* prop : pl->getOwnedProperties())
                if (prop->getLevel() > 0) { opts.push_back(pl->getUsername()); break; }
        }
        if (opts.empty()) { lastEvent = "Tidak ada bangunan lawan."; return; }
        pendingCardIdx       = cardIdx;
        cardMode             = mode;
        demolitionPlayerPick = -1;
        openSelPopup("DEMOLITION — Pilih Pemain", opts);
    }
}

void GameScreen::finishTeleport(int tileIdx) {
    Player* p = gs->currentTurnPlayer();
    p->removeHandCard(pendingCardIdx);
    SkillCardManager::applyTeleport(*p, tileIdx, gs->board.getTileCount());
    p->setSkillUsed(true);
    gs->addLog(*p, "GUNAKAN_KEMAMPUAN", "TeleportCard -> " + gs->tiles[tileIdx]->getTileCode());
    landing->applyLanding(*p);
    drainAndStoreEvents();
    cardMode = CardMode::NONE; pendingCardIdx = -1;
    gameLoop->checkWinGui();
    if (gs->game_over) gameOverOverlay = true;
}

void GameScreen::finishLasso(int playerIdx) {
    Player* p = gs->currentTurnPlayer();
    // find the target player by index among non-bankrupt, non-self
    std::vector<Player*> others;
    for (auto* pl : gs->players)
        if (pl != p && pl->getStatus() != "BANKRUPT") others.push_back(pl);
    if (playerIdx < 0 || playerIdx >= (int)others.size()) {
        cardMode = CardMode::NONE; pendingCardIdx = -1; return;
    }
    Player* target = others[playerIdx];
    p->removeHandCard(pendingCardIdx);
    SkillCardManager::applyLasso(*p, *target, p->getCurrTile());
    p->setSkillUsed(true);
    gs->addLog(*p, "GUNAKAN_KEMAMPUAN", "LassoCard -> " + target->getUsername());
    lastEvent = target->getUsername() + " ditarik ke " + gs->tiles[p->getCurrTile()]->getTileName();
    drainAndStoreEvents();
    cardMode = CardMode::NONE; pendingCardIdx = -1;
}

void GameScreen::finishDemolition(int playerIdx, int propIdx) {
    Player* p = gs->currentTurnPlayer();
    // collect opponents with buildings
    std::vector<std::pair<Player*, std::vector<PropertyTile*>>> opps;
    for (auto* pl : gs->players) {
        if (pl == p || pl->getStatus() == "BANKRUPT") continue;
        std::vector<PropertyTile*> built;
        for (auto* prop : pl->getOwnedProperties())
            if (prop->getLevel() > 0) built.push_back(prop);
        if (!built.empty()) opps.push_back({pl, built});
    }
    if (playerIdx < 0 || playerIdx >= (int)opps.size()) {
        cardMode = CardMode::NONE; pendingCardIdx = -1; return;
    }
    auto& [target, built] = opps[playerIdx];
    if (propIdx < 0 || propIdx >= (int)built.size()) {
        cardMode = CardMode::NONE; pendingCardIdx = -1; return;
    }
    PropertyTile* tProp = built[propIdx];
    p->removeHandCard(pendingCardIdx);
    SkillCardManager::applyDemolition(*tProp);
    p->setSkillUsed(true);
    gs->addLog(*p, "GUNAKAN_KEMAMPUAN", "DemolitionCard -> " + tProp->getTileCode());
    lastEvent = "Bangunan di " + tProp->getTileCode() + " dihancurkan!";
    drainAndStoreEvents();
    cardMode = CardMode::NONE; pendingCardIdx = -1;
    demolitionPlayerPick = -1;
}

void GameScreen::drawSkillCardSection(float cardX, float cardScale, float startY) {
    if (!gs) return;
    Player* p = gs->currentTurnPlayer();
    if (!p) return;
    int handSz = p->getHandSize();
    if (handSz == 0) return;

    float pad   = 12.0f * globalScale;
    float sectX = cardX + pad + 20.0f;
    float sectW = cardPanel.width * cardScale - 6.0f * pad;
    float sectY = startY;
    int   lblSz = (int)(12 * globalScale);

    bool used = p->isSkillUsed();
    Color lblColor = used ? Color{170,170,170,180} : Color{148,73,68,255};
    DrawText("KARTU KEMAMPUAN", (int)sectX, (int)sectY, lblSz, lblColor);
    sectY += lblSz + 6.0f * globalScale;

    float btnH  = 24.0f * globalScale;
    float btnGap= 4.0f  * globalScale;

    for (int i = 0; i < handSz && i < 4; i++) {
        auto* card = p->getHandCard(i);
        if (!card) continue;
        std::string label = card->getCardType() + ": " + card->describe();
        if (label.size() > 28) label = label.substr(0, 28) + "..";
        Color bg    = used ? Color{210,210,210,255} : Color{255,235,202,255};
        Color hover = used ? Color{210,210,210,255} : Color{255,220,180,255};
        Color fg    = used ? Color{170,170,170,255} : Color{80,40,35,255};
        btnSkillCards[i].loadAsText(label, sectX, sectY, sectW, btnH, bg, hover, fg);
        btnSkillCards[i].setDisabled(used || !hasRolled);
        btnSkillCards[i].draw();
        Color border = (used || !hasRolled) ? Color{170,170,170,180} : Color{80,40,35,255};
        DrawRectangleLinesEx({sectX, sectY, sectW, btnH}, 1.0f, border);
        sectY += btnH + btnGap;
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// SELECTION POPUP (LASSO / DEMOLITION)
// ══════════════════════════════════════════════════════════════════════════════
void GameScreen::openSelPopup(const std::string& title, const std::vector<std::string>& opts) {
    selPopupTitle  = title;
    selOptions     = opts;
    selPopupResult = -1;
    showSelPopup   = true;

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float popW = 340.0f * globalScale, popH = (80.0f + opts.size() * 32.0f) * globalScale;
    float popX = (sw - popW) / 2.0f, popY = (sh - popH) / 2.0f;
    float optH = 28.0f * globalScale;
    float optY = popY + 50.0f * globalScale;

    btnSelOptions.clear();
    for (int i = 0; i < (int)opts.size(); i++) {
        Button b;
        b.loadAsText(opts[i], popX + 20.0f * globalScale, optY + i * (optH + 4.0f * globalScale),
                     popW - 40.0f * globalScale, optH,
                     {255,235,202,255},{255,220,180,255},{80,40,35,255});
        btnSelOptions.push_back(b);
    }
}

void GameScreen::updateSelPopup() {
    for (int i = 0; i < (int)btnSelOptions.size(); i++) {
        if (btnSelOptions[i].isClicked()) {
            selPopupResult = i;
            showSelPopup = false;
            break;
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        showSelPopup = false; selPopupResult = -1;
        cardMode = CardMode::NONE; pendingCardIdx = -1; demolitionPlayerPick = -1;
        return;
    }
    if (!showSelPopup && selPopupResult >= 0) {
        if (cardMode == CardMode::LASSO_SELECT) {
            finishLasso(selPopupResult);
        } else if (cardMode == CardMode::DEMOLITION_SELECT) {
            if (demolitionPlayerPick < 0) {
                // step 1 done — pick player; now build property list for that player
                demolitionPlayerPick = selPopupResult;
                Player* p = gs->currentTurnPlayer();
                // find that opponent's properties with buildings
                std::vector<Player*> opps;
                for (auto* pl : gs->players) {
                    if (pl == p || pl->getStatus() == "BANKRUPT") continue;
                    for (auto* prop : pl->getOwnedProperties())
                        if (prop->getLevel() > 0) { opps.push_back(pl); break; }
                }
                if (demolitionPlayerPick < (int)opps.size()) {
                    Player* target = opps[demolitionPlayerPick];
                    std::vector<std::string> propOpts;
                    for (auto* prop : target->getOwnedProperties())
                        if (prop->getLevel() > 0)
                            propOpts.push_back(prop->getTileCode() + " " + prop->getTileName()
                                               + " (lvl " + std::to_string(prop->getLevel()) + ")");
                    openSelPopup("DEMOLITION — Pilih Bangunan", propOpts);
                }
            } else {
                finishDemolition(demolitionPlayerPick, selPopupResult);
            }
        }
    }
}

void GameScreen::drawSelPopup() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, {0,0,0,150});

    float popW = 340.0f * globalScale;
    float popH = (80.0f + selOptions.size() * 32.0f) * globalScale;
    float popX = (sw - popW) / 2.0f, popY = (sh - popH) / 2.0f;

    DrawRectangleRounded({popX, popY, popW, popH}, 0.12f, 8, {255,243,210,255});
    DrawRectangleRoundedLines({popX, popY, popW, popH}, 0.12f, 8, {80,40,35,255});

    int titleSz = (int)(16 * globalScale);
    int tw = MeasureText(selPopupTitle.c_str(), titleSz);
    DrawText(selPopupTitle.c_str(),
             (int)(popX + (popW - tw) / 2.0f), (int)(popY + 14.0f * globalScale),
             titleSz, {80,40,35,255});

    int hintSz = (int)(10 * globalScale);
    const char* hint = "ESC untuk batal";
    DrawText(hint, (int)(popX + 16.0f*globalScale), (int)(popY + popH - 16.0f*globalScale),
             hintSz, {160,120,100,160});

    for (auto& b : btnSelOptions) b.draw();
}
