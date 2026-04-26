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
#include "utils/exceptions/GeneralException.hpp"
#include "utils/exceptions/InsufficientMoneyException.hpp"
#include "utils/exceptions/UnablePayRentException.hpp"
#include "utils/exceptions/UnablePayPPHTaxException.hpp"
#include "utils/exceptions/UnablePayPBMTaxException.hpp"

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
GameScreen::GameScreen(GameLoop* loop, const std::string& initMsg)
    : bgTexture{}, blurredBgTexture{}, boardTexture{}, cardPanel{},
      aktaTexture{}, selectedTileIdx(-1),
      activeTab(0),
      dice1(1), dice2(1), diceRolling(false),
      diceRollTimer(0), diceRollInterval(0.08f), diceRollFrame(0),
      hasRolled(false), turnEnded(false), isDouble(false), consecDoubles(0),
      globalScale(1.0f), boardX(0), boardY(0), boardScale(1.0f),
      cornerRatioW(CORNER_RATIO_W), cornerRatioH(CORNER_RATIO_H),
      gameLoop(loop), gs(loop ? loop->getState() : nullptr),
      showPopup(!initMsg.empty()), gameOverOverlay(false)
{
    if (!initMsg.empty()) {
        popupTitle = "LOAD GAGAL";
        popupMsg   = initMsg;
    }
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

// ─── pushLog / drainAndStoreEvents ───────────────────────────────────────────
void GameScreen::pushLog(const std::string& msg) {
    if (msg.empty()) return;
    eventLog.push_front(msg);
    while ((int)eventLog.size() > EVENT_LOG_MAX)
        eventLog.pop_back();
}

void GameScreen::drainAndStoreEvents() {
    if (!gs) return;
    auto evts = gs->events.drainEvents();
    for (int i = (int)evts.size() - 1; i >= 0; i--)
        pushLog(evts[i]);

    for (int i = txLogCursor; i < (int)gs->transaction_log.size(); i++) {
        const auto& entry = gs->transaction_log[i];
        const auto& action = entry.getActionType();
        const auto& desc = entry.getDescription();

        if (action == "KARTU") {
            bool isChance = (desc.find("Kesempatan") != std::string::npos);
            std::string cardDesc = desc;
            auto colon = desc.find(": ");
            if (colon != std::string::npos) cardDesc = desc.substr(colon + 2);
            if (!showCardPopup) {
                showCardPopup     = true;
                cardPopupIsChance = isChance;
                cardPopupDesc     = cardDesc;
            }
            pushLog(isChance ? "Dapat kartu Kesempatan" : "Dapat kartu Dana Umum");
        } else if (action == "GAJI_GO") {
            pushLog("+ " + desc + " (gaji GO)");
        } else if (action == "SEWA") {
            pushLog("Bayar sewa: " + desc);
        } else if (action == "SEWA_BEBAS") {
            pushLog("Sewa gratis: " + desc);
        } else if (action == "BAYAR_PAJAK") {
            pushLog("Bayar pajak: " + desc);
        } else if (action == "OTOMATIS") {
            pushLog(desc);
        } else if (action == "DROP_KEMAMPUAN") {
            pushLog("Buang kartu kemampuan");
        } else if (action == "DRAW_KEMAMPUAN") {
            // desc format: "KIND - description"
            auto dash = desc.find(" - ");
            std::string kind = (dash != std::string::npos) ? desc.substr(0, dash) : desc;
            pushLog("Dapat kartu kemampuan: " + kind);
        } else if (action == "GUNAKAN_KEMAMPUAN") {
            // desc format: "TypeCard -> target" or "TypeCard - description"
            auto arr = desc.find(" -> ");
            std::string kind = (arr != std::string::npos) ? desc.substr(0, arr) : desc;
            pushLog("Pakai kartu: " + kind);
        } else if (action == "BANGKRUT") {
            const auto& uname = entry.getUsername();
            pushLog(uname + " BANGKRUT!");
        } else if (action == "GADAI") {
            // desc: "CODE MVALUE"
            auto sp = desc.find(' ');
            std::string code = (sp != std::string::npos) ? desc.substr(0, sp) : desc;
            std::string val  = (sp != std::string::npos) ? desc.substr(sp + 1) : "";
            // look up property name
            std::string propName = code;
            for (int ti = 0; ti < (int)gs->tiles.size(); ti++) {
                if (gs->tiles[ti]->getTileCode() == code) { propName = gs->tiles[ti]->getTileName(); break; }
            }
            pushLog("Gadai: " + propName + (val.empty() ? "" : " +" + val));
        } else if (action == "TEBUS") {
            auto sp = desc.find(' ');
            std::string code = (sp != std::string::npos) ? desc.substr(0, sp) : desc;
            std::string val  = (sp != std::string::npos) ? desc.substr(sp + 1) : "";
            std::string propName = code;
            for (int ti = 0; ti < (int)gs->tiles.size(); ti++) {
                if (gs->tiles[ti]->getTileCode() == code) { propName = gs->tiles[ti]->getTileName(); break; }
            }
            pushLog("Tebus: " + propName + (val.empty() ? "" : " -" + val));
        } else if (action == "BELI") {
            pushLog("Beli: " + desc);
        } else if (action == "BANGUN") {
            std::string propName = desc;
            for (int ti = 0; ti < (int)gs->tiles.size(); ti++) {
                if (gs->tiles[ti]->getTileCode() == desc) { propName = gs->tiles[ti]->getTileName(); break; }
            }
            pushLog("Bangun di: " + propName);
        }
    }
    txLogCursor = (int)gs->transaction_log.size();
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
    // Indonesian config names (underscores replaced with spaces in loadAssets)
    if (group == "COKLAT")      return {139, 90,  43,  255};
    if (group == "BIRU MUDA")   return {135, 206, 235, 255};
    if (group == "MERAH MUDA")  return {255, 105, 180, 255};
    if (group == "ORANGE")      return {255, 140,  0,  255};
    if (group == "MERAH")       return {220,  20,  60, 255};
    if (group == "KUNING")      return {255, 210,   0, 255};
    if (group == "HIJAU")       return { 34, 139,  34, 255};
    if (group == "BIRU TUA")    return {  0,   0, 180, 255};
    // English fallbacks (legacy)
    if (group == "Brown")       return {139,  90,  43, 255};
    if (group == "Light Blue")  return {135, 206, 235, 255};
    if (group == "Pink")        return {255, 105, 180, 255};
    if (group == "Red")         return {220,  20,  60, 255};
    if (group == "Yellow")      return {255, 210,   0, 255};
    if (group == "Green")       return { 34, 139,  34, 255};
    if (group == "Dark Blue")   return {  0,   0, 180, 255};
    return {160, 160, 160, 255};
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
            gs->jail_turns.erase(p);
            p->setStatus("ACTIVE");
            landing->rollAndMove(*p, true, dice1, dice2);
            // rollAndMove already called applyGoSalary internally
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
                // escaped with double → preserve bonus roll (standard Monopoly rule)
                isDouble = jr.isDouble;
                if (isDouble) consecDoubles++;
            } else {
                gs->jail_turns[p] = jr.newJailTurns;
                pushLog(p->getUsername() + " gagal keluar penjara ("
                            + std::to_string(jr.newJailTurns) + "/3)");
                isDouble = false;
                drainAndStoreEvents();
                hasRolled = true;
                handleEndTurn();
                return;
            }
        }
    } else {
        bool dbl = landing->rollAndMove(*p, true, dice1, dice2);

        // Dice result log
        char diceLog[32];
        std::snprintf(diceLog, sizeof(diceLog), "%d + %d = %d%s", dice1, dice2, dice1 + dice2, dbl ? " (Ganda!)" : "");
        pushLog(std::string("Dadu: ") + diceLog);
        gs->addLog(*p, "DADU", std::string(diceLog));

        // ── 3rd consecutive double → IMMEDIATE jail, no exceptions ───────
        if (dbl) {
            consecDoubles++;
            if (consecDoubles >= 3) {
                landing->sendToJail(*p);
                isDouble      = false;
                consecDoubles = 0;
                pushLog(p->getUsername() + " masuk penjara (3x double berturut-turut)!");
                finalizeRoll();
                handleEndTurn();
                return;
            }
        }

        int tileIdx = p->getCurrTile();
        Tile* landedTile = gs->tiles[tileIdx];
        std::string tileType = landedTile->getTileType();

        // Landing log
        if (tileType == "CHANCE") {
            pushLog("Anda mendarat di petak kartu Kesempatan.");
        } else if (tileType == "COMMUNITY") {
            pushLog("Anda mendarat di petak kartu Dana Umum.");
        } else if (tileType == "FESTIVAL") {
            pushLog("Anda mendarat di Festival!");
        } else {
            pushLog("Mendarat di [" + landedTile->getTileCode() + "] " + landedTile->getTileName());
        }

        auto* prop = dynamic_cast<PropertyTile*>(gs->tiles[tileIdx]);
        bool isBuyableProp = prop && prop->getTileOwner() == nullptr;
        // intercept unowned property landing for human players
        if (isBuyableProp && !currentPlayerIsBot()) {
            // Auto-give railroad/utility (no purchase needed)
            std::string tp = prop->getTileType();
            if (tp == "RAILROAD" || tp == "UTILITY") {
                p->addOwnedProperty(prop);
                gs->recomputeMonopolyForGroup(prop->getColorGroup());
                pushLog(prop->getTileName() + " (" + prop->getTileCode() + ") jadi milikmu!");
                isDouble = dbl;
                finalizeRoll();
                return;
            }
            // if player can't afford, skip popup and go straight to auction
            if (p->getBalance() < prop->getBuyPrice()) {
                startAuction();
                isDouble = dbl;
                finalizeRoll();
                return;
            }
            propertyLandingPending = true;
            propertyLandingTileIdx = tileIdx;
            propertyLandingDouble  = dbl;
            finalizeRoll();
            return;
        }

        if (checkPPH(*p)) {
            isDouble = dbl;
            // (consecDoubles already incremented above if dbl)
            return;
        }

        // ── applyLanding with exception-based debt recovery ───────────────
        try {
            landing->applyLanding(*p);
        } catch (const UnablePayRentException& e) {
            // Backend threw because player can't afford rent (after discount applied)
            std::string msg = e.what();
            // extract amount from message "Sewa M<N>"
            int rent = 0;
            auto pos = msg.find('M');
            if (pos != std::string::npos) rent = std::stoi(msg.substr(pos + 1));
            debtMode    = true;
            debtLanding = true;
            debtAmount  = rent > 0 ? rent : debtAmount;
            pushLog("Tidak sanggup bayar sewa M" + std::to_string(debtAmount));
            isDouble = dbl;
            finalizeRoll();
            return;
        } catch (const UnablePayPBMTaxException& e) {
            std::string msg = e.what();
            int amt = 0;
            auto pos = msg.find('M');
            if (pos != std::string::npos) amt = std::stoi(msg.substr(pos + 1));
            debtMode    = true;
            debtLanding = true;
            debtAmount  = amt > 0 ? amt : debtAmount;
            pushLog("Tidak sanggup bayar pajak PBM M" + std::to_string(debtAmount));
            isDouble = dbl;
            finalizeRoll();
            return;
        } catch (const UnablePayPPHTaxException&) {
            pushLog("Tidak sanggup bayar pajak PPH.");
            isDouble = dbl;
            finalizeRoll();
            return;
        } catch (const InsufficientMoneyException& e) {
            int needRaise = 0;
            std::string msg = e.what();
            auto pos = msg.find("CARD:");
            if (pos != std::string::npos) needRaise = std::stoi(msg.substr(pos + 5));
            debtMode    = true;
            debtLanding = false;
            debtAmount  = 0;
            pushLog("Tidak sanggup bayar kartu! Perlu naikkan M" + std::to_string(needRaise) + " via gadai.");
            isDouble = dbl;
            finalizeRoll();
            return;
        }

        // if player was sent to jail by landing (GO_JAIL tile / card) — end turn, no bonus
        if (p->getStatus() == "JAIL") {
            isDouble      = false;
            consecDoubles = 0;
        } else {
            isDouble = dbl;  // consecDoubles already updated at top of else block
        }
    }

    finalizeRoll();
    if (gs && !gs->game_over) checkFestival(*p);

    // auto-end turn if player just entered jail (no need to press END)
    if (gs && !gs->game_over && p->getStatus() == "JAIL") {
        handleEndTurn();
    }
}

// ─── handleEndTurn ───────────────────────────────────────────────────────────
void GameScreen::handleEndTurn() {
    if (!gs) return;
    if (isDouble && !gameOverOverlay) {
        // bonus roll: reset dice flags only, keep consecDoubles
        hasRolled  = false;
        isDouble   = false;
        turnEnded  = false;
        eventLog.clear();
        pushLog("Dadu ganda! Giliran tambahan.");
        return;
    }
    consecDoubles = 0;
    debtMode = false; debtLanding = false; debtAmount = 0;
    pphDebtAmt = 0; pphDebtDesc.clear();
    gameLoop->checkWinGui();
    if (!gs->game_over) {
        gameLoop->advanceTurn();
        hasRolled  = false;
        turnEnded  = false;
        isDouble   = false;
        eventLog.clear();

        // Resolve any deferred landings for the new current player
        Player* newPlayer = gs->currentTurnPlayer();
        if (newPlayer) {
            auto it = gs->deferredLandings.find(newPlayer);
            if (it != gs->deferredLandings.end()) {
                gs->deferredLandings.erase(it);
                if (newPlayer->getStatus() != "BANKRUPT") {
                    try {
                        landing->applyLanding(*newPlayer);
                    } catch (const UnablePayRentException& e) {
                        std::string msg = e.what();
                        int rent = 0;
                        auto pos = msg.find('M');
                        if (pos != std::string::npos) rent = std::stoi(msg.substr(pos + 1));
                        debtMode = true; debtLanding = true; debtAmount = rent;
                        pushLog(newPlayer->getUsername() + " tidak sanggup bayar sewa M" + std::to_string(rent) + " (efek LASSO)");
                    } catch (const UnablePayPBMTaxException& e) {
                        std::string msg = e.what();
                        int amt = 0; auto pos = msg.find('M');
                        if (pos != std::string::npos) amt = std::stoi(msg.substr(pos + 1));
                        debtMode = true; debtLanding = true; debtAmount = amt;
                        pushLog("Tidak sanggup bayar pajak PBM M" + std::to_string(amt) + " (efek LASSO)");
                    } catch (const UnablePayPPHTaxException&) {
                        pushLog("Tidak sanggup bayar pajak PPH (efek LASSO).");
                    } catch (const InsufficientMoneyException& e) {
                        int needRaise = 0;
                        std::string msg = e.what();
                        auto pos = msg.find("CARD:");
                        if (pos != std::string::npos) needRaise = std::stoi(msg.substr(pos + 5));
                        debtMode = true; debtLanding = false; debtAmount = 0;
                        pushLog("Tidak sanggup bayar kartu! Perlu naikkan M" + std::to_string(needRaise) + " via gadai.");
                    }
                    drainAndStoreEvents();
                    if (debtMode) {
                        checkJailChoice();
                        return;
                    }
                }
            }
        }

        // draw skill card for the new current player (human or bot)
        if (cardProc && newPlayer)
            cardProc->drawSkillCardAtTurnStart(*newPlayer);
        drainAndStoreEvents();
        checkDropCard();     // show discard popup if hand > 3 (human players)

        checkJailChoice();   // show popup if next player is in jail
    } else {
        gameOverOverlay = true;
    }
}

// ─── checkDropCard ────────────────────────────────────────────────────────────
void GameScreen::checkDropCard() {
    if (!gs) return;
    Player* p = gs->currentTurnPlayer();
    // only human players — bots auto-discard inside drawSkillCardAtTurnStart
    if (!p || dynamic_cast<Bot*>(p)) return;
    if (p->getHandSize() > 3) dropCardPending = true;
}

// ─── checkJailChoice ─────────────────────────────────────────────────────────
void GameScreen::checkJailChoice() {
    if (!gs) return;
    Player* p = gs->currentTurnPlayer();
    // only for human players in jail; defer if dropCard popup is pending
    if (p && p->getStatus() == "JAIL" && !dynamic_cast<Bot*>(p) && !dropCardPending)
        jailChoicePending = true;
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
    float abBoxX  = cardX + boxPad + 20.0f * globalScale;
    float abBoxY  = btnY + btnH + boxPad + 10.0f * globalScale;
    float abBoxW  = cardPanel.width * cardScale - 6.0f * boxPad;
    float labelH  = 14.0f * globalScale;
    float abtnY   = abBoxY + 120.0f * globalScale + 10.0f * globalScale + labelH + 8.0f * globalScale;
    float abtnColGap = 8.0f * globalScale;
    float abtnRowGap = 6.0f * globalScale;
    float abtnW   = (abBoxW - abtnColGap) / 2.0f;
    float abtnH   = 30.0f * globalScale;
    Color abBg    = {255,235,202,255};
    Color abHover = {255,220,180,255};
    Color abFg    = {80,40,35,255};
    float col2X   = abBoxX + abtnW + abtnColGap;

    // Layout: GADAI | TEBUS  (row 1),  BUILD HOUSE full-width (row 2)
    btnMortgage.loadAsText(  "GADAI",       abBoxX, abtnY,                    abtnW, abtnH, abBg, abHover, abFg);
    btnRedeem.loadAsText(    "TEBUS",       col2X,  abtnY,                    abtnW, abtnH, abBg, abHover, abFg);
    btnBuildHouse.loadAsText("BUILD HOUSE", abBoxX, abtnY+(abtnH+abtnRowGap), abBoxW, abtnH, abBg, abHover, abFg);

    // debt recovery buttons — positioned in same area as action buttons
    float debtW = (abBoxW - abtnColGap) / 2.0f;
    btnDebtGadai.loadAsText("GADAI",  abBoxX,  abtnY, debtW, abtnH,
                             {200,140,50,255},{220,160,70,255},WHITE);
    btnDebtLelang.loadAsText("LELANG", col2X,   abtnY, debtW, abtnH,
                              {70,120,200,255},{90,140,220,255},WHITE);
    float forceW = abBoxW;
    btnDebtForce.loadAsText("LANJUT (TERIMA PAILIT)", abBoxX,
                             abtnY + abtnH + abtnRowGap, forceW, abtnH,
                             {180,60,60,255},{200,80,80,255},WHITE);

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
    aktaTexture      = LoadTexture("assets/Akta_background.png");
    chanceCardTex    = LoadTexture("assets/cards/Chance_card.png");
    communityCardTex = LoadTexture("assets/cards/Community_Card.png");
    lelangBackTexture = LoadTexture("assets/Lelang_back.png");

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
    UnloadTexture(chanceCardTex);
    UnloadTexture(communityCardTex);
    UnloadTexture(lelangBackTexture);
    btnPlay.unload(); btnAssets.unload(); btnPlayers.unload(); btnLog.unload();
    btnRollDice.unload(); btnEndTurn.unload();
    btnBuildHouse.unload(); btnMortgage.unload(); btnRedeem.unload();
    btnDebtGadai.unload(); btnDebtLelang.unload(); btnDebtForce.unload();
    btnAuctionBid.unload(); btnAuctionPass.unload();
    btnSaveGame.unload();
    btnSetDice.unload();
    btnD1Plus.unload(); btnD1Minus.unload();
    btnD2Plus.unload(); btnD2Minus.unload();
    btnConfirmDice.unload(); btnCloseDice.unload();
    btnPopupOk.unload();
    btnCardPopupOk.unload();
    btnJailPay.unload(); btnJailRoll.unload();
    for (auto& b : btnSkillCards) b.unload();
}

// ─── update ───────────────────────────────────────────────────────────────────
void GameScreen::update(float dt) {
    // blocking overlays: no other interaction allowed
    // TELEPORT_SELECT intentionally excluded: it needs tile clicks on board
    bool cardBlocks = (cardMode == CardMode::LASSO_SELECT ||
                       cardMode == CardMode::DEMOLITION_SELECT);
    // debtMode intentionally excluded from anyOverlay so skill cards remain usable
    bool anyOverlay = showPopup || showCardPopup || showSelPopup || gameOverOverlay || setDiceMode
                      || auctionMode || cardBlocks || pphPending
                      || buildMode || festivalPending || propertyLandingPending
                      || saveMode || jailChoicePending || dropCardPending;

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
                       || buildMode || festivalPending || propertyLandingPending
                       || setDiceMode || saveMode || debtMode;
        bool isBot   = gs && currentPlayerIsBot();
        bool jailed  = gs && currentPlayerIsJailed();

        // ROLL: only when it's human's turn, hasn't rolled yet, dice not animating, not bankrupt
        bool isBankrupt = gs && gs->currentTurnPlayer() && gs->currentTurnPlayer()->getStatus() == "BANKRUPT";
        btnRollDice.setDisabled(blocked || isBot || hasRolled || diceRolling || isBankrupt);

        // END: only after rolling (and dice done animating)
        btnEndTurn.setDisabled(blocked || isBot || !hasRolled || diceRolling);

        // ATUR DADU: only before rolling
        btnSetDice.setDisabled(blocked || isBot || hasRolled || diceRolling);

        // SAVE: disable during overlays for consistency
        // Save only allowed at START of turn (before rolling), not mid-turn
        btnSaveGame.setDisabled(blocked || hasRolled || diceRolling);

        // action buttons: only after rolling, only for human, only relevant ones
        bool afterRoll = !blocked && !isBot && hasRolled && !diceRolling;

        // GADAI / TEBUS: available anytime during player's turn (before OR after roll)
        // In debtMode, still available (that's the whole point)
        bool myTurn = !blocked && !isBot && !diceRolling;
        bool hasGadaiable = false, hasMortgaged = false;
        if (gs) {
            Player* cur = gs->currentTurnPlayer();
            if (cur) {
                for (auto* prop : cur->getOwnedProperties()) {
                    if (prop->getTileOwner() == cur && !prop->isMortgage()) hasGadaiable = true;
                    if (prop->getTileOwner() == cur &&  prop->isMortgage()) hasMortgaged = true;
                }
            }
        }
        // debtMode: only GADAI allowed (TEBUS makes balance worse)
        if (debtMode) {
            btnMortgage.setDisabled(!hasGadaiable);
            btnRedeem.setDisabled(true);
        } else {
            btnMortgage.setDisabled(!myTurn || !hasGadaiable);
            btnRedeem.setDisabled(!myTurn || !hasMortgaged);
        }

        if (jailed) {
            btnBuildHouse.setDisabled(true);
        } else {
            btnBuildHouse.setDisabled(!afterRoll);
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

    // ── drop-card popup (hand > 3) ────────────────────────────────────────
    if (dropCardPending && gs) {
        Player* p = gs->currentTurnPlayer();
        if (!p || p->getHandSize() <= 3) { dropCardPending = false; dropCardConfirm = false; return; }

        int handSz = p->getHandSize();

        if (dropCardConfirm) {
            // YES → discard, NO → back to list
            if (btnDropYes.isClicked()) {
                if (dropCardConfirmIdx >= 0 && dropCardConfirmIdx < handSz) {
                    gs->addLog(*p, "DROP_KEMAMPUAN", std::to_string(dropCardConfirmIdx + 1));
                    p->removeHandCard(dropCardConfirmIdx);
                }
                dropCardConfirm = false; dropCardConfirmIdx = -1;
                dropCardPending = (p->getHandSize() > 3);
                btnDropCards.clear();
                // if dropCard done, check if jailChoice should now appear
                if (!dropCardPending) checkJailChoice();
                return;
            }
            if (btnDropNo.isClicked()) {
                dropCardConfirm = false; dropCardConfirmIdx = -1;
                return;
            }
            return;
        }

        // Card list: show type only, click → confirmation
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
        float popW  = 480.0f * globalScale;
        float btnH  = 44.0f  * globalScale;
        float btnGap = 8.0f  * globalScale;
        float pad   = 24.0f  * globalScale;
        float headerH = 60.0f * globalScale;
        float subInfoH = 30.0f * globalScale;
        float popH  = headerH + subInfoH + pad*0.5f + handSz*(btnH+btnGap) + pad;
        float popX  = (sw2 - popW) / 2.0f;
        float popY  = (sh2 - popH) / 2.0f;

        btnDropCards.resize(handSz);
        for (int i = 0; i < handSz; i++) {
            auto* c = p->getHandCard(i);
            std::string label = c ? c->getCardType() : "?";
            float btnY = popY + headerH + subInfoH + pad*0.5f + i*(btnH+btnGap);
            btnDropCards[i].loadAsText(label, popX+pad, btnY, popW-2*pad, btnH,
                {244,79,68,255},{255,110,100,255}, WHITE);
            if (btnDropCards[i].isClicked()) {
                dropCardConfirmIdx = i;
                dropCardConfirm    = true;
                return;
            }
        }
        return;
    }

    // ── jail choice popup ─────────────────────────────────────────────────
    if (jailChoicePending && gs) {
        Player* p = gs->currentTurnPlayer();
        if (!p || p->getStatus() != "JAIL") { jailChoicePending = false; return; }

        if (btnJailPay.isClicked()) {
            // pay fine → exit jail
            bankruptcy->cmdBayarDenda(*p);
            drainAndStoreEvents();
            jailChoicePending = false;
            // player now ACTIVE — can roll normally this turn
        } else if (btnJailRoll.isClicked()) {
            // skip popup → let player roll (JailManager handles it in executeRollResult)
            jailChoicePending = false;
        }
        return;
    }

    // card popup — dismiss with any click or ENTER
    if (showCardPopup) {
        if (btnCardPopupOk.isClicked() || IsKeyPressed(KEY_ENTER) ||
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            showCardPopup = false;
        }
        return;
    }

    // generic popup blocks everything else
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
        if (anyOverlay) return; // don't process ESC during popups
        if (cardMode != CardMode::NONE) { cardMode = CardMode::NONE; pendingCardIdx = -1; return; }
        if (selectedTileIdx >= 0)       { selectedTileIdx = -1; return; }
        nextScreen = AppScreen::HOME; shouldChangeScreen = true; return;
    }

    // ── tile click: TELEPORT target OR normal selection ───────────────────
    if (!anyOverlay && !setDiceMode && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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
        } else {
            // clicked outside board — cancel TELEPORT if active, else deselect tile
            if (cardMode == CardMode::TELEPORT_SELECT) {
                cardMode = CardMode::NONE; pendingCardIdx = -1;
            } else if (selectedTileIdx >= 0 && cardMode == CardMode::NONE) {
                selectedTileIdx = -1;
            }
        }
    }

    if (!gs) return;

    // ── save game ─────────────────────────────────────────────────────────
    if (!anyOverlay && btnSaveGame.isClicked()) {
        saveMode = true;
        saveInputLen = 0;
        saveInputBuf[0] = '\0';
    }

    // ── ATUR DADU popup ───────────────────────────────────────────────────
    if (!anyOverlay && btnSetDice.isClicked()) setDiceMode = true;

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
        auto payPPH = [&](int amt, const std::string& logDesc) {
            if (p->getBalance() >= amt) {
                *p += -amt;
                gs->addLog(*p, "BAYAR_PAJAK", logDesc);
                gs->events.pushEvent(p->getUsername() + " membayar " + logDesc);
                pphPending = false;
                hasRolled = true;
                drainAndStoreEvents();
                gameLoop->checkWinGui();
                if (gs->game_over) gameOverOverlay = true;
            } else {
                // Can't afford — enter debt mode, keep pphPending until resolved
                int maxLiq = 0;
                for (auto* prop : p->getOwnedProperties())
                    if (!prop->isMortgage()) maxLiq += prop->getMortgageValue();
                if (p->getBalance() + maxLiq < amt) {
                    // Can't even liquidate enough — force bankrupt
                    *p += -(amt);   // go negative so processBankruptcy fires
                    pphPending = false;
                    hasRolled = true;
                    drainAndStoreEvents();
                    gameLoop->checkWinGui();
                    if (gs->game_over) gameOverOverlay = true;
                } else {
                    debtMode    = true;
                    debtLanding = false;
                    debtAmount  = amt;
                    pphDebtAmt  = amt;
                    pphDebtDesc = logDesc;
                    pphPending  = false;
                    hasRolled   = true;
                    pushLog("Tidak sanggup bayar PPH M" + std::to_string(amt) + "! Gadai properti dulu.");
                }
            }
        };

        if (btnPphFlat.isClicked()) {
            payPPH(pphFlatAmt, "PPH FLAT M" + std::to_string(pphFlatAmt));
        } else if (btnPphPct.isClicked()) {
            payPPH(pphPctAmt, "PPH " + std::to_string(gs->config.getPphPercentage()) + "% M" + std::to_string(pphPctAmt));
        }
        return;
    }

    // ── Save popup ────────────────────────────────────────────────────────
    if (saveMode) {
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
        float popW = 420.0f * globalScale;
        float popH = 220.0f * globalScale;
        float popX = (sw2 - popW) / 2.0f;
        float popY = (sh2 - popH) / 2.0f;

        float btnW2 = (popW - 30.0f * globalScale) / 2.0f;
        float btnH2 = 36.0f * globalScale;
        float btnY2 = popY + popH - btnH2 - 20.0f * globalScale;

        btnSaveConfirm.loadAsText("SIMPAN", popX + 10.0f * globalScale, btnY2, btnW2, btnH2,
                                  {70,140,80,255},{90,170,100,255},WHITE);
        btnSaveCancel.loadAsText("BATAL", popX + 20.0f * globalScale + btnW2, btnY2, btnW2, btnH2,
                                 {120,50,45,255},{160,70,65,255},WHITE);

        // text input
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= 32 && key <= 126 && saveInputLen < 50) {
                saveInputBuf[saveInputLen++] = (char)key;
                saveInputBuf[saveInputLen] = '\0';
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && saveInputLen > 0)
            saveInputBuf[--saveInputLen] = '\0';

        if (btnSaveConfirm.isClicked() && saveInputLen > 0) {
            std::string fname = std::string("save/") + saveInputBuf;
            try {
                gameLoop->saveToFile(fname);
                pushLog("Game tersimpan ke " + fname);
            } catch (const std::exception& e) {
                showPopup = true; popupTitle = "SAVE GAGAL";
                popupMsg  = std::string("Tidak bisa menyimpan:\n") + e.what();
            }
            saveMode = false;
            saveInputLen = 0;
            saveInputBuf[0] = '\0';
            return;
        }
        if (btnSaveCancel.isClicked()) {
            saveMode = false;
            saveInputLen = 0;
            saveInputBuf[0] = '\0';
            return;
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

        float listStartY = popY + 70.0f * globalScale;
        float listEndY   = popY + popH - cancelH - 16.0f * globalScale;
        float visibleH   = listEndY - listStartY;

        btnFestivalCancel.loadAsText("BATAL", popX + popW - cancelW - 10.0f * globalScale,
                                     popY + popH - cancelH - 10.0f * globalScale,
                                     cancelW, cancelH,
                                     {120,50,45,255},{160,70,65,255},WHITE);
        if (btnFestivalCancel.isClicked()) {
            festivalPending = false;
            festivalProps.clear();
            btnFestivalProps.clear();
            festivalScroll = 0.0f;
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
                b.loadAsText(buf, 0.0f, 0.0f, btnW2, rowH,
                             {148,73,68,255},{170,85,78,255},WHITE);
                btnFestivalProps.push_back(b);
            }
        }

        // scroll handling
        float contentH = (float)btnFestivalProps.size() * (rowH + 4.0f);
        if (contentH > visibleH) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f) {
                festivalScroll -= wheel * 60.0f * globalScale;
                festivalScroll = std::max(0.0f, std::min(festivalScroll, contentH - visibleH));
            }
        } else {
            festivalScroll = 0.0f;
        }

        // update positions & visibility
        for (int i = 0; i < (int)btnFestivalProps.size(); i++) {
            float itemY = listStartY + i * (rowH + 4.0f) - festivalScroll;
            btnFestivalProps[i].setPosition(popX + 20.0f * globalScale, itemY);
            bool visible = (itemY + rowH > listStartY) && (itemY < listEndY);
            btnFestivalProps[i].setDisabled(!visible);
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
                festivalScroll = 0.0f;
                break;
            }
        }
        return;
    }

    // ── Property landing popup (BUY / AUCTION) ────────────────────────────
    if (propertyLandingPending) {
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();

        float gap = 20.0f * globalScale;
        float aktaScale = 0.42f * globalScale;        // same scale for both sides
        float aktaW = aktaTexture.width  * aktaScale;
        float aktaH = aktaTexture.height * aktaScale;
        float rightW = aktaW;
        float rightH = aktaH;
        float totalW = aktaW + gap + rightW;
        float popX = (sw2 - totalW) / 2.0f;
        float popY = (sh2 - aktaH) / 2.0f;
        float rightX = popX + aktaW + gap;
        float rightY = popY;

        float btnMargin = 40.0f * globalScale;
        float btnGap    = 8.0f * globalScale;
        float btnW2     = (rightW - 2*btnMargin - btnGap) / 2.0f;
        float btnH2     = 28.0f * globalScale;
        float btnY2     = rightY + rightH - btnH2 - btnMargin;

        auto* prop = dynamic_cast<PropertyTile*>(gs->tiles[propertyLandingTileIdx]);
        if (!prop) { propertyLandingPending = false; return; }

        btnPropBuy.loadAsText("BELI", rightX + btnMargin, btnY2, btnW2, btnH2,
                              {70,140,80,255},{90,170,100,255},WHITE);
        btnPropAuction.loadAsText("LELANG", rightX + btnMargin + btnW2 + btnGap, btnY2, btnW2, btnH2,
                                  {244,206,43,255},{250,220,70,255},{80,40,35,255});

        Player* p = gs->currentTurnPlayer();
        if (btnPropBuy.isClicked()) {
            if (!BuyManager::buy(*p, *prop)) {
                showPopup = true; popupTitle = "BELI";
                popupMsg  = "Tidak bisa membeli\ntile ini.";
            } else {
                gs->addLog(*p, "BELI", prop->getTileCode());
                drainAndStoreEvents();
                pushLog(p->getUsername() + " membeli " + prop->getTileName()
                        + " (M" + std::to_string(prop->getBuyPrice()) + ")");
            }
            isDouble = propertyLandingDouble;
            propertyLandingPending = false;
            propertyLandingTileIdx = -1;
            return;
        }
        if (btnPropAuction.isClicked()) {
            startAuction();
            isDouble = propertyLandingDouble;
            propertyLandingPending = false;
            propertyLandingTileIdx = -1;
            return;
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
                                 {148,73,68,255},{170,85,78,255},WHITE);
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
                                     {148,73,68,255},{170,85,78,255},WHITE);
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
                            int newLvl = buildSelectedProp->getLevel();
                            std::string bldLabel = (newLvl == 5) ? "Hotel" : std::to_string(newLvl) + " Rumah";
                            pushLog(gs->currentTurnPlayer()->getUsername() + " bangun "
                                    + bldLabel + " di " + buildSelectedProp->getTileName());
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
                                       {148,73,68,255},{170,85,78,255},WHITE);
            if (btnBuildConfirm.isClicked()) {
                Player* bldP = gs->currentTurnPlayer();  // cache before state changes
                if (bldP && buildSelectedProp) {
                    try {
                        PropertyManager::build(*bldP, *buildSelectedProp);
                        int newLvl = buildSelectedProp->getLevel();
                        std::string bldLabel = (newLvl == 5) ? "Hotel" : std::to_string(newLvl) + " Rumah";
                        gs->addLog(*bldP, "BANGUN", buildSelectedProp->getTileCode());
                        drainAndStoreEvents();
                        pushLog(bldP->getUsername() + " bangun " + bldLabel
                                + " di " + buildSelectedProp->getTileName());
                    } catch (const UnablePayBuildException&) {
                        showPopup = true; popupTitle = "BUILD"; popupMsg = "Saldo tidak cukup!";
                    }
                }
                buildMode = false; buildStep = 0; buildSelectedProp = nullptr;
            }
        }
        return;
    }

    // ── selection popup (LASSO / DEMOLITION) ─────────────────────────────
    if (showSelPopup) { updateSelPopup(); return; }

    // ── skill card confirmation popup ─────────────────────────────────────
    if (skillCardConfirmPending && gs) {
        Player* p = gs->currentTurnPlayer();
        if (!p) { skillCardConfirmPending = false; skillCardConfirmIdx = -1; }
        else {
            int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
            float popW = 380.0f * globalScale;
            float popH = 220.0f * globalScale;
            float popX = (sw2 - popW) / 2.0f;
            float popY = (sh2 - popH) / 2.0f;
            float btnW = 100.0f * globalScale;
            float btnH = 32.0f * globalScale;
            float gap  = 16.0f * globalScale;
            float btnY = popY + popH - btnH - 20.0f * globalScale;
            float totalBtnW = btnW * 2 + gap;
            float btnStartX = popX + (popW - totalBtnW) / 2.0f;

            // Soft green for USE, transparent (popup-bg) for CANCEL
            btnSkillConfirmUse.loadAsText("GUNAKAN", btnStartX, btnY, btnW, btnH,
                                          {100,145,100,255},{120,170,120,255},WHITE);
            btnSkillConfirmCancel.loadAsText("BATAL", btnStartX + btnW + gap, btnY, btnW, btnH,
                                             {255,243,210,255},{255,235,202,255},{80,40,35,255});
            if (btnSkillConfirmUse.isClicked()) {
                auto* card = p->getHandCard(skillCardConfirmIdx);
                if (card) {
                    std::string t = card->getCardType();
                    if      (t == "TELEPORT")   useCardWithTarget(skillCardConfirmIdx, CardMode::TELEPORT_SELECT);
                    else if (t == "LASSO")      useCardWithTarget(skillCardConfirmIdx, CardMode::LASSO_SELECT);
                    else if (t == "DEMOLITION") useCardWithTarget(skillCardConfirmIdx, CardMode::DEMOLITION_SELECT);
                    else                        useCardImmediate(skillCardConfirmIdx);
                }
                skillCardConfirmPending = false;
                skillCardConfirmIdx = -1;
                return;
            }
            if (btnSkillConfirmCancel.isClicked()) {
                skillCardConfirmPending = false;
                skillCardConfirmIdx = -1;
                return;
            }
        }
        return;
    }

    // ── debt recovery mode ────────────────────────────────────────────────
    if (debtMode && gs) {
        Player* p = gs->currentTurnPlayer();

        // Safety: no valid current player → exit debt mode
        if (!p) { debtMode = false; debtLanding = false; return; }

        // Auto-resolve if player now can afford
        // card debt (debtAmount==0): resolve when balance ≥ 0; rent debt: balance ≥ debtAmount
        bool debtResolved = (debtAmount == 0) ? (p->getBalance() >= 0)
                                               : (p->getBalance() >= debtAmount);
        if (debtResolved) {
            debtMode = false;
            if (debtLanding) {
                debtLanding = false;
                try {
                    landing->applyLanding(*p);
                } catch (const GeneralException& e) {
                    pushLog(std::string("Error: ") + e.what());
                }
                drainAndStoreEvents();
                gameLoop->checkWinGui();
                if (gs->game_over) gameOverOverlay = true;
                else checkFestival(*p);
            } else if (pphDebtAmt > 0) {
                // Deferred PPH payment — player now has enough
                *p += -pphDebtAmt;
                gs->addLog(*p, "BAYAR_PAJAK", pphDebtDesc);
                gs->events.pushEvent(p->getUsername() + " membayar " + pphDebtDesc);
                pushLog("Bayar PPH: " + pphDebtDesc);
                pphDebtAmt = 0; pphDebtDesc.clear();
                drainAndStoreEvents();
                gameLoop->checkWinGui();
                if (gs->game_over) gameOverOverlay = true;
            }
            return;
        }

        // GADAI → open gadai popup (stays in debtMode after)
        if (btnDebtGadai.isClicked()) { openGadaiPopup(); return; }

        // Check if player just used SHIELD card → cancel debt landing
        if (debtLanding && p->isShieldActive()) {
            // Shield is now active — retry applyLanding, rent will be cancelled
            debtMode    = false;
            debtLanding = false;
            try { landing->applyLanding(*p); } catch (...) {}
            drainAndStoreEvents();
            pushLog("Shield melindungi! Sewa dibatalkan.");
            gameLoop->checkWinGui();
            if (gs->game_over) gameOverOverlay = true;
            return;
        }

        // Skill card section: handle card clicks even in debtMode
        if (!p->isSkillUsed() && !skillCardConfirmPending) {
            int handSz = p->getHandSize();
            for (int ci = 0; ci < handSz && ci < 4; ci++) {
                if (btnSkillCards[ci].isClicked()) {
                    auto* card = p->getHandCard(ci);
                    if (!card) continue;
                    std::string t = card->getCardType();
                    // Only SHIELD and DISCOUNT make sense in debt context
                    if (t == "SHIELD" || t == "DISCOUNT") {
                        skillCardConfirmPending = true;
                        skillCardConfirmIdx = ci;
                    } else {
                        pushLog("Kartu " + t + " tidak bisa dipakai saat dalam kondisi utang.");
                    }
                    return;
                }
            }
        }

        // Force proceed (bankrupt)
        if (btnDebtForce.isClicked()) {
            debtMode    = false;
            debtLanding = false;
            pphDebtAmt  = 0; pphDebtDesc.clear();
            if (p) {
                landing->applyLanding(*p);  // backend will bankrupt automatically
                drainAndStoreEvents();
                gameLoop->checkWinGui();
                if (gs->game_over) gameOverOverlay = true;
            }
        }
        return;
    }

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

    // ── bankrupt auction queue ────────────────────────────────────────────
    if (gs && !gs->bankruptAuctionQueue.empty() && !auctionMode)
        startNextBankruptAuction();

    // ── auction mode ──────────────────────────────────────────────────────
    if (auctionMode) { updateAuction(); return; }

    // ── roll dice button ──────────────────────────────────────────────────
    if (btnRollDice.isClicked() && !hasRolled) {
        diceRolling      = true;
        diceRollTimer    = 0;
        diceRollFrame    = 0;
        diceRollInterval = 0.06f;
    }

    // ── GADAI / TEBUS — accessible before OR after roll ──────────────────
    if (!currentPlayerIsBot() && activeTab == 0) {
        if (btnMortgage.isClicked()) openGadaiPopup();
        if (btnRedeem.isClicked())   openTebusPopup();
    }

    // ── skill card buttons — accessible before OR after roll (except debt) ──
    if (!currentPlayerIsBot() && activeTab == 0 && !debtMode) {
        Player* p = gs->currentTurnPlayer();
        if (p && !p->isSkillUsed() && !skillCardConfirmPending) {
            int handSz = p->getHandSize();
            for (int ci = 0; ci < handSz && ci < 4; ci++) {
                if (btnSkillCards[ci].isClicked()) {
                    auto* card = p->getHandCard(ci);
                    if (!card) continue;
                    skillCardConfirmPending = true;
                    skillCardConfirmIdx = ci;
                    return;
                }
            }
        }
    }

    // ── action buttons (PLAY tab) ─────────────────────────────────────────
    if (hasRolled && activeTab == 0 && !currentPlayerIsJailed()) {
        Player* p = gs->currentTurnPlayer();

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

    // ── houses & hotels on board ──────────────────────────────────────────
    if (gs) {
        float houseSz = 14.0f * globalScale;
        float hotelSz = 18.0f * globalScale;
        float houseGap = 0.125f * globalScale;
        for (int i = 0; i < 40; i++) {
            auto* prop = dynamic_cast<PropertyTile*>(gs->tiles[i]);
            if (!prop || prop->getTileType() != "STREET") continue;
            int lvl = prop->getLevel();
            if (lvl <= 0) continue;

            int row, col;
            getGridRC(i, row, col);
            Rectangle tb = tileBounds[i];

            if (lvl >= 5) {
                // Hotel
                float sc = hotelSz / (float)hotelTexture.width;
                float hw = hotelTexture.width * sc;
                float hh = hotelTexture.height * sc;
                float hx = tb.x + tb.width / 2.0f - hw / 2.0f;
                float hy = tb.y + tb.height / 2.0f - hh / 2.0f;
                if (row == 10) { // bottom row, strip at top
                    hy = tb.y + tb.height * 0.06f - hh / 2.0f;
                } else if (row == 0) { // top row, strip at bottom
                    hy = tb.y + tb.height * 0.94f - hh / 2.0f;
                } else if (col == 0) { // left col, strip at right
                    hx = tb.x + tb.width * 0.94f - hw / 2.0f;
                } else if (col == 10) { // right col, strip at left
                    hx = tb.x + tb.width * 0.06f - hw / 2.0f;
                }
                DrawTextureEx(hotelTexture, {hx, hy}, 0.0f, sc, WHITE);
            } else {
                // Houses (1-4)
                float sc = houseSz / (float)houseTexture.width;
                float hw = houseTexture.width * sc;
                float hh = houseTexture.height * sc;
                float totalLen = lvl * hw + (lvl - 1) * houseGap;
                for (int h = 0; h < lvl; h++) {
                    float hx, hy;
                    if (row == 10) { // bottom row, strip at top, horizontal
                        float startX = tb.x + (tb.width - totalLen) / 2.0f;
                        hx = startX + h * (hw + houseGap);
                        hy = tb.y + tb.height * 0.06f - hh / 2.0f;
                    } else if (row == 0) { // top row, strip at bottom, horizontal
                        float startX = tb.x + (tb.width - totalLen) / 2.0f;
                        hx = startX + h * (hw + houseGap);
                        hy = tb.y + tb.height * 0.94f - hh / 2.0f;
                    } else if (col == 0) { // left col, strip at right, vertical
                        float startY = tb.y + (tb.height - totalLen) / 2.0f;
                        hx = tb.x + tb.width * 0.94f - hw / 2.0f;
                        hy = startY + h * (hw + houseGap);
                    } else { // right col, strip at left, vertical
                        float startY = tb.y + (tb.height - totalLen) / 2.0f;
                        hx = tb.x + tb.width * 0.06f - hw / 2.0f;
                        hy = startY + h * (hw + houseGap);
                    }
                    DrawTextureEx(houseTexture, {hx, hy}, 0.0f, sc, WHITE);
                }
            }
        }
    }

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

    // ── festival multiplier marks on tiles ───────────────────────────────
    if (gs) {
        int festSz = std::max(8, (int)(11 * globalScale));
        for (int i = 0; i < (int)gs->tiles.size() && i < 40; i++) {
            auto* prop = dynamic_cast<PropertyTile*>(gs->tiles[i]);
            if (!prop || prop->getFestivalDuration() <= 0 || prop->getFestivalMultiplier() <= 1)
                continue;

            char mark[8];
            std::snprintf(mark, sizeof(mark), "x%d", prop->getFestivalMultiplier());
            int mw = MeasureText(mark, festSz);

            float cx2 = tileCenters[i][0];
            float cy2 = tileCenters[i][1];

            // small pill background
            float padX = 3.0f, padY = 2.0f;
            DrawRectangleRounded({cx2 - mw/2.0f - padX, cy2 - festSz/2.0f - padY,
                                   (float)mw + 2*padX, (float)festSz + 2*padY},
                                  0.4f, 4, {220, 80, 20, 200});
            DrawText(mark, (int)(cx2 - mw/2.0f), (int)(cy2 - festSz/2.0f), festSz, WHITE);
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
            float iconStartX  = cardX + (cardPanel.width * cardScale - iconSz) - 306.0f * globalScale;
            float iconY       = btnPlay.getY() + btnPlay.getHeight() - 198.0f * globalScale;
            float iconScaleVal = iconSz / (float)playerIconsB[activeIdx % 4].width;
            DrawRectangleRec({iconStartX - 3*globalScale, iconY - 3*globalScale, iconSz + 6*globalScale, iconSz + 6*globalScale},
                             {255,235,202,255});
            DrawRectangleLinesEx({iconStartX - 3*globalScale, iconY - 2*globalScale, iconSz + 6*globalScale, iconSz + 6*globalScale},
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
        const char* h = "Klik tile tujuan teleport";
        int hw2 = MeasureText(h, hSz);
        DrawText(h, (int)(boardX + (bw2 - hw2) / 2.0f),
                 (int)(boardY + bh2 - 24.0f * globalScale), hSz, WHITE);
    }

    // tile overlay
    if (selectedTileIdx >= 0) drawTileOverlay();

    // auction overlay (drawn before popups so error popups appear on top)
    if (auctionMode) drawAuctionUI(cardX, cardScale);

    // popup — draw at most ONE exclusive popup (priority: dropCard > jailChoice > card)
    if (dropCardPending)        drawDropCardPopup();
    else if (jailChoicePending) drawJailChoice();
    if (showCardPopup) drawCardPopup();
    if (showPopup) drawPopup();

    // selection popup (LASSO / DEMOLITION)
    if (showSelPopup) drawSelPopup();

    // skill card confirmation popup
    if (skillCardConfirmPending && gs) {
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
        DrawRectangle(0, 0, sw2, sh2, {0,0,0,180});

        float popW = 380.0f * globalScale;
        float popH = 220.0f * globalScale;
        float popX = (sw2 - popW) / 2.0f;
        float popY = (sh2 - popH) / 2.0f;

        DrawRectangleRounded({popX, popY, popW, popH}, 0.12f, 8, {255,243,210,255});
        DrawRectangleRoundedLines({popX, popY, popW, popH}, 0.12f, 8, {80,40,35,255});

        Player* p = gs->currentTurnPlayer();
        if (p && skillCardConfirmIdx >= 0) {
            auto* card = p->getHandCard(skillCardConfirmIdx);
            if (card) {
                // Big card title centered
                int titleSz = (int)(22 * globalScale);
                std::string title = card->getCardType() + " CARD";
                int tw = MeasureText(title.c_str(), titleSz);
                DrawText(title.c_str(), (int)(popX + (popW - tw)/2.0f),
                         (int)(popY + 24.0f*globalScale), titleSz, {148,73,68,255});

                // Description — bigger, centered
                int descSz = (int)(14 * globalScale);
                std::string desc = card->describe();
                float maxDescW = popW - 48.0f * globalScale;
                float descY = popY + 65.0f * globalScale;

                // simple word wrap
                std::vector<std::string> lines;
                std::string current;
                for (char c : desc) {
                    current += c;
                    if (c == ' ') {
                        if (MeasureText(current.c_str(), descSz) > maxDescW) {
                            size_t sp = current.find_last_of(' ');
                            if (sp != std::string::npos && sp > 0) {
                                lines.push_back(current.substr(0, sp));
                                current = current.substr(sp + 1);
                            }
                        }
                    }
                }
                if (!current.empty()) lines.push_back(current);
                if (lines.empty() && !desc.empty()) lines.push_back(desc);

                float totalDescH = lines.size() * (descSz + 4);
                float descStartY = popY + 70.0f*globalScale;
                for (const auto& ln : lines) {
                    int lw = MeasureText(ln.c_str(), descSz);
                    DrawText(ln.c_str(), (int)(popX + (popW - lw)/2.0f),
                             (int)descStartY, descSz, {80,40,35,255});
                    descStartY += descSz + 4;
                }
            }
        }
        btnSkillConfirmUse.draw();
        btnSkillConfirmCancel.draw();
        // manual outline for cancel (transparent bg needs visible border)
        DrawRectangleLinesEx({btnSkillConfirmCancel.getX(), btnSkillConfirmCancel.getY(),
                              btnSkillConfirmCancel.getWidth(), btnSkillConfirmCancel.getHeight()},
                             1.5f, {80,40,35,255});
    }

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

    // save popup
    if (saveMode) {
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
        DrawRectangle(0, 0, sw2, sh2, {0,0,0,180});

        float popW = 420.0f * globalScale;
        float popH = 220.0f * globalScale;
        float popX = (sw2 - popW) / 2.0f;
        float popY = (sh2 - popH) / 2.0f;

        DrawRectangleRounded({popX, popY, popW, popH}, 0.12f, 8, {255,243,210,255});
        DrawRectangleRoundedLines({popX, popY, popW, popH}, 0.12f, 8, {80,40,35,255});

        int titleSz = (int)(20 * globalScale);
        int tw2 = MeasureText("SIMPAN GAME", titleSz);
        DrawText("SIMPAN GAME", (int)(popX + (popW - tw2)/2.0f), (int)(popY + 18.0f*globalScale), titleSz, {148,73,68,255});

        int subSz = (int)(14 * globalScale);
        const char* msg = "Nama file:";
        int mw = MeasureText(msg, subSz);
        DrawText(msg, (int)(popX + (popW - mw)/2.0f), (int)(popY + 55.0f*globalScale), subSz, BLACK);

        // text input box
        float inpW = popW - 40.0f * globalScale;
        float inpH = 34.0f * globalScale;
        float inpX = popX + 20.0f * globalScale;
        float inpY = popY + 85.0f * globalScale;
        DrawRectangleRec({inpX, inpY, inpW, inpH}, {255,255,255,255});
        DrawRectangleLinesEx({inpX, inpY, inpW, inpH}, 1.5f, {80,40,35,255});

        int txtSz = (int)(16 * globalScale);
        DrawText(saveInputBuf, (int)(inpX + 8.0f*globalScale), (int)(inpY + (inpH - txtSz)/2.0f), txtSz, BLACK);

        // cursor blink
        if ((int)(GetTime() * 2) % 2 == 0) {
            int cursorX = (int)(inpX + 8.0f*globalScale + MeasureText(saveInputBuf, txtSz) + 2);
            DrawLine(cursorX, (int)(inpY + 6.0f*globalScale), cursorX, (int)(inpY + inpH - 6.0f*globalScale), BLACK);
        }

        btnSaveConfirm.draw();
        btnSaveCancel.draw();
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

        // scissor for scrollable list
        float rowH = 30.0f * globalScale;
        float cancelH = 28.0f * globalScale;
        float listStartY = popY + 70.0f * globalScale;
        float listEndY   = popY + popH - cancelH - 16.0f * globalScale;
        float visibleH   = listEndY - listStartY;
        float contentH   = (float)btnFestivalProps.size() * (rowH + 4.0f);
        float listW      = popW - 40.0f * globalScale;

        BeginScissorMode((int)(popX + 18.0f*globalScale), (int)listStartY,
                         (int)(popW - 36.0f*globalScale), (int)visibleH);
        for (auto& b : btnFestivalProps) b.draw();
        EndScissorMode();

        // scrollbar
        if (contentH > visibleH) {
            float sbX      = popX + popW - 24.0f * globalScale;
            float sbY      = listStartY;
            float sbW      = 6.0f * globalScale;
            float sbH      = visibleH;
            float thumbH   = (visibleH / contentH) * sbH;
            float thumbY   = sbY + (festivalScroll / contentH) * sbH;
            DrawRectangleRec({sbX, sbY, sbW, sbH}, {200, 190, 180, 120});
            DrawRectangleRec({sbX, thumbY, sbW, thumbH}, {148, 73, 68, 200});
        }

        btnFestivalCancel.draw();
    }

    // property landing popup (split: left=akta, right=akta bg+info+buttons)
    if (propertyLandingPending) {
        int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
        DrawRectangle(0, 0, sw2, sh2, {0,0,0,180});

        float gap = 20.0f * globalScale;
        float aktaScale = 0.42f * globalScale;        // same scale for both sides
        float aktaW = aktaTexture.width  * aktaScale;
        float aktaH = aktaTexture.height * aktaScale;
        float rightW = aktaW;
        float rightH = aktaH;
        float totalW = aktaW + gap + rightW;
        float popX = (sw2 - totalW) / 2.0f;
        float popY = (sh2 - aktaH) / 2.0f;
        float rightX = popX + aktaW + gap;
        float rightY = popY;

        const TileInfo& info = tileData[propertyLandingTileIdx];

        // ── Left: Akta card ──
        DrawTextureEx(aktaTexture, {popX, popY}, 0.0f, aktaScale, WHITE);

        int nameSz = (int)(aktaH * 0.055f);
        int nameW  = MeasureText(info.name.c_str(), nameSz);
        DrawText(info.name.c_str(), (int)(popX + (aktaW - nameW) / 2.0f),
                 (int)(popY + aktaH * 0.04f), nameSz, BLACK);

        int typeSz = (int)(aktaH * 0.030f);
        int typeW  = MeasureText(info.type.c_str(), typeSz);
        DrawText(info.type.c_str(), (int)(popX + (aktaW - typeW) / 2.0f),
                 (int)(popY + aktaH * 0.13f), typeSz, {148,73,68,255});

        float lx = popX + aktaW * 0.08f;
        float rx = popX + aktaW * 0.92f;
        float ry = popY + aktaH * 0.25f;
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
        }

        // ── Right: Akta background + info + buttons ──
        DrawTextureEx(aktaTexture, {rightX, rightY}, 0.0f, aktaScale, WHITE);

        auto* prop = dynamic_cast<PropertyTile*>(gs->tiles[propertyLandingTileIdx]);
        if (prop) {
            float cx = rightX + rightW / 2.0f;

            int titleSz = (int)(26 * globalScale);
            int tW = MeasureText(prop->getTileName().c_str(), titleSz);
            DrawText(prop->getTileName().c_str(), (int)(cx - tW / 2.0f),
                     (int)(rightY + rightH * 0.06f), titleSz, {148,73,68,255});

            int subSz = (int)(16 * globalScale);
            const char* typeStr = (info.type == "STREET") ? "Properti Jalan"
                                  : (info.type == "RAILROAD") ? "Stasiun Kereta"
                                  : (info.type == "UTILITY") ? "Utilitas" : "Properti";
            int typeW2 = MeasureText(typeStr, subSz);
            DrawText(typeStr, (int)(cx - typeW2 / 2.0f),
                     (int)(rightY + rightH * 0.14f), subSz, {80,40,35,255});

            // Pesan pembelian
            int msgSz = (int)(15 * globalScale);
            const char* line1 = "Anda mendarat di petak ini.";
            int l1w = MeasureText(line1, msgSz);
            DrawText(line1, (int)(cx - l1w / 2.0f),
                     (int)(rightY + rightH * 0.26f), msgSz, BLACK);

            char priceBuf[128];
            std::snprintf(priceBuf, sizeof(priceBuf), "Apakah Anda ingin membeli");
            int l2w = MeasureText(priceBuf, msgSz);
            DrawText(priceBuf, (int)(cx - l2w / 2.0f),
                     (int)(rightY + rightH * 0.33f), msgSz, BLACK);

            std::snprintf(priceBuf, sizeof(priceBuf), "dengan harga M%d?", prop->getBuyPrice());
            int l3w = MeasureText(priceBuf, msgSz);
            DrawText(priceBuf, (int)(cx - l3w / 2.0f),
                     (int)(rightY + rightH * 0.40f), msgSz, BLACK);

            const char* line4 = "Atau melelangnya?";
            int l4w = MeasureText(line4, msgSz);
            DrawText(line4, (int)(cx - l4w / 2.0f),
                     (int)(rightY + rightH * 0.48f), msgSz, {120,80,75,255});

            // Buttons at bottom
            btnPropBuy.draw();
            btnPropAuction.draw();
        }
    }

    // game over
    if (gameOverOverlay) drawGameOverOverlay();
}

// ─── drawPlayTab ──────────────────────────────────────────────────────────────
void GameScreen::drawPlayTab(float cardX, float cardScale) {
    float boxPad = 12.0f * globalScale;
    float boxX   = cardX + boxPad + 20.0f * globalScale;
    float boxY   = btnPlay.getY() + btnPlay.getHeight() + boxPad + 10.0f * globalScale;
    float boxW   = cardPanel.width * cardScale - 6.0f * boxPad;
    float boxH   = 120.0f * globalScale;

    DrawRectangleRec({boxX, boxY, boxW, boxH}, {255,243,210,255});
    DrawRectangleLinesEx({boxX, boxY, boxW, boxH}, 2.5f, {50,25,20,255});

    int titleSz = (int)(15 * globalScale);
    int subSz   = (int)(12 * globalScale);

    float textX = boxX + 8*globalScale;
    float textY = boxY + 6*globalScale;

    if (diceRolling) {
        DrawText("ROLLING...", (int)textX, (int)textY, titleSz, {148,73,68,255});
        textY += titleSz + 4*globalScale;
    } else if (hasRolled) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Dadu: %d + %d = %d", dice1, dice2, dice1 + dice2);
        DrawText(buf, (int)textX, (int)textY, subSz, {148,73,68,255});
        textY += subSz + 3*globalScale;
    } else {
        const char* hint = currentPlayerIsJailed()
            ? "Di penjara — lempar dadu / bayar denda"
            : "Lempar dadu untuk bermain";
        DrawText(hint, (int)textX, (int)textY, subSz, {148,73,68,200});
        textY += subSz + 3*globalScale;
    }

    int logSz   = (int)(11 * globalScale);
    float lineH = (float)(logSz + 3);
    float maxLineW = boxW - 16*globalScale;
    float logMaxY = boxY + boxH - 4*globalScale;

    auto wrapLine = [&](const std::string& text, int fontSize, float maxWidth) -> std::vector<std::string> {
        std::vector<std::string> lines;
        std::string remaining = text;
        while (!remaining.empty()) {
            int low = 1, high = (int)remaining.size(), best = (int)remaining.size();
            while (low <= high) {
                int mid = (low + high) / 2;
                if (MeasureText(remaining.substr(0, mid).c_str(), fontSize) <= (int)maxWidth)
                { best = mid; low = mid + 1; } else { high = mid - 1; }
            }
            int breakAt = best;
            if (breakAt < (int)remaining.size()) {
                size_t sp = remaining.rfind(' ', breakAt);
                if (sp != std::string::npos && sp > 0) breakAt = (int)sp;
            }
            lines.push_back(remaining.substr(0, breakAt));
            while (breakAt < (int)remaining.size() && remaining[breakAt] == ' ') breakAt++;
            remaining = remaining.substr(breakAt);
        }
        if (lines.empty() && !text.empty()) lines.push_back(text);
        return lines;
    };

    int idx = 0;
    for (const auto& entry : eventLog) {
        if (textY >= logMaxY) break;
        Color c = (idx++ == 0) ? Color{70, 180, 70, 255} : Color{60, 30, 25, 180};
        auto wrapped = wrapLine(entry, logSz, maxLineW);
        for (const auto& ln : wrapped) {
            if (textY >= logMaxY) break;
            DrawText(ln.c_str(), (int)textX, (int)textY, logSz, c);
            textY += lineH;
        }
    }

    float actionsLabelY = boxY + boxH + 10.0f * globalScale;
    int labelSz = (int)(13 * globalScale);

    if (debtMode) {
        drawDebtMode(cardX, cardScale);
        return;
    }

    DrawText("ACTIONS", (int)boxX, (int)actionsLabelY, labelSz, {148,73,68,255});
    auto drawBtn = [&](Button& btn) {
        btn.draw();
        Color border = btn.isDisabled() ? Color{170,170,170,180} : Color{80,40,35,255};
        DrawRectangleLinesEx({btn.getX(), btn.getY(), btn.getWidth(), btn.getHeight()},
                             1.5f, border);
    };
    drawBtn(btnMortgage);
    drawBtn(btnRedeem);
    drawBtn(btnBuildHouse);

    // skill cards section — below action buttons
    float cardSectionY = btnBuildHouse.getY() + btnBuildHouse.getHeight() + 10.0f * globalScale;
    drawSkillCardSection(cardX, cardScale, cardSectionY);
}

// ─── drawAssetsTab ────────────────────────────────────────────────────────────
void GameScreen::drawAssetsTab(float cardX, float cardScale) {
    if (!gs) return;
    Player* p = gs->currentTurnPlayer();
    if (!p) return;

    float pad   = 12.0f * globalScale;
    float listX = cardX + pad + 20.0f * globalScale;
    float listY = btnPlay.getY() + btnPlay.getHeight() + pad + 10.0f * globalScale;
    float listW = cardPanel.width * cardScale - 6.0f * pad;
    float rowH  = 24.0f * globalScale;
    int   rowSz = (int)(13 * globalScale);

    DrawText("Properti Dimiliki:", (int)listX, (int)listY, rowSz + 2, {80,40,35,255});
    listY += rowH + 4.0f;

    auto props = p->getOwnedProperties();
    if (props.empty()) {
        DrawText("Belum ada properti.", (int)listX, (int)listY, rowSz, {120,80,75,255});
    } else {
        int sh2 = GetScreenHeight();
        float cardY = (sh2 - cardPanel.height * cardScale) / 2.0f;
        float panelBottom = cardY + cardPanel.height * cardScale;
        float maxY = panelBottom - pad;
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

            // text — vertically centered in the strip
            float textX = stripX + stripW + 6.0f * globalScale;
            float textY = stripY + (stripH - rowSz) / 2.0f;
            std::string info = "[" + prop->getTileCode() + "] " + prop->getTileName();
            if (prop->isMortgage()) info += " [GADAI]";
            int lvl = prop->getLevel();
            if (lvl > 0 && lvl < 5) {
                char b[16]; std::snprintf(b, sizeof(b), " [%dR]", lvl);
                info += b;
            } else if (lvl == 5) {
                info += " [H]";
            }
            DrawText(info.c_str(), (int)textX, (int)textY, rowSz, BLACK);
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
    float listX   = cardX + pad + 20.0f * globalScale;
    float listY   = btnPlay.getY() + btnPlay.getHeight() + pad + 10.0f * globalScale;
    float listW   = cardPanel.width * cardScale - 6.0f * pad;
    float cardH   = 58.0f * globalScale;
    float cardGap = 5.0f * globalScale;
    float iconBox = cardH - 8.0f * globalScale;
    int   nameSz  = (int)(15 * globalScale);
    int   statSz  = (int)(13 * globalScale);
    float statIconH = (float)(statSz + 4);

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
    float listX = cardX + pad + 20.0f * globalScale;
    float listY = btnPlay.getY() + btnPlay.getHeight() + pad + 10.0f * globalScale;
    float listW = cardPanel.width * cardScale - 6.0f * pad;
    int   rowSz = (int)(11 * globalScale);
    float lineH = (float)(rowSz + 4);
    int   sh2   = GetScreenHeight();
    float cardY = (sh2 - cardPanel.height * cardScale) / 2.0f;
    float panelBottom = cardY + cardPanel.height * cardScale;
    float maxY  = panelBottom - pad;

    DrawText("Log Permainan:", (int)listX, (int)listY, rowSz + 2, {80,40,35,255});
    listY += (float)(rowSz + 6);

    auto& log = gs->transaction_log;
    if (log.empty()) {
        DrawText("Belum ada log.", (int)listX, (int)listY, rowSz, {120,80,75,255});
        return;
    }

    // Map raw action codes to human-readable labels + color
    auto formatEntry = [](const std::string& action, const std::string& desc,
                          const std::string& uname, int turn,
                          std::string& outLine, Color& outColor) {
        std::string prefix = "T" + std::to_string(turn) + " [" + uname + "] ";
        outColor = {60, 30, 25, 220};
        if      (action == "DADU")             { outLine = prefix + "Lempar dadu: " + desc; outColor = {60,60,180,255}; }
        else if (action == "BELI")             { outLine = prefix + "Beli: " + desc; outColor = {40,140,60,255}; }
        else if (action == "BANGUN")           { outLine = prefix + "Bangun di: " + desc; outColor = {40,140,60,255}; }
        else if (action == "GADAI")            { outLine = prefix + "Gadai: " + desc; outColor = {180,120,30,255}; }
        else if (action == "TEBUS")            { outLine = prefix + "Tebus: " + desc; outColor = {180,120,30,255}; }
        else if (action == "GAJI_GO")          { outLine = prefix + "Gaji GO: " + desc; outColor = {40,140,60,255}; }
        else if (action == "SEWA")             { outLine = prefix + "Bayar sewa: " + desc; outColor = {180,50,50,255}; }
        else if (action == "SEWA_BEBAS")       { outLine = prefix + "Sewa gratis (pemilik penjara): " + desc; outColor = {100,160,100,255}; }
        else if (action == "BAYAR_PAJAK")      { outLine = prefix + "Bayar pajak: " + desc; outColor = {180,50,50,255}; }
        else if (action == "KARTU")            { outLine = prefix + "Kartu: " + desc; outColor = {120,40,140,255}; }
        else if (action == "DRAW_KEMAMPUAN")   { outLine = prefix + "Dapat kartu kemampuan: " + desc; outColor = {120,40,140,255}; }
        else if (action == "GUNAKAN_KEMAMPUAN"){ outLine = prefix + "Gunakan kemampuan: " + desc; outColor = {120,40,140,255}; }
        else if (action == "DROP_KEMAMPUAN")   { outLine = prefix + "Buang kartu kemampuan: " + desc; outColor = {130,100,30,255}; }
        else if (action == "FESTIVAL")         { outLine = prefix + "Festival: " + desc; outColor = {180,120,30,255}; }
        else if (action == "LELANG")           { outLine = prefix + "Lelang: " + desc; outColor = {40,140,60,255}; }
        else if (action == "MASUK_PENJARA")    { outLine = prefix + "Masuk penjara"; outColor = {160,40,40,255}; }
        else if (action == "BANGKRUT")         { outLine = prefix + "BANGKRUT!"; outColor = {200,0,0,255}; }
        else if (action == "OTOMATIS")         { outLine = prefix + desc; }
        else                                   { outLine = prefix + action + ": " + desc; }
    };

    auto wrapText = [&](const std::string& text, int fontSize, float maxWidth) -> std::vector<std::string> {
        std::vector<std::string> lines;
        std::string remaining = text;
        while (!remaining.empty()) {
            int low = 1, high = (int)remaining.size(), best = (int)remaining.size();
            while (low <= high) {
                int mid = (low + high) / 2;
                if (MeasureText(remaining.substr(0, mid).c_str(), fontSize) <= (int)maxWidth)
                { best = mid; low = mid + 1; } else { high = mid - 1; }
            }
            int breakAt = best;
            if (breakAt < (int)remaining.size()) {
                size_t sp = remaining.rfind(' ', breakAt);
                if (sp != std::string::npos && sp > 0) breakAt = (int)sp;
            }
            lines.push_back(remaining.substr(0, breakAt));
            while (breakAt < (int)remaining.size() && remaining[breakAt] == ' ') breakAt++;
            remaining = remaining.substr(breakAt);
        }
        if (lines.empty() && !text.empty()) lines.push_back(text);
        return lines;
    };

    float contentH = 0;
    std::vector<std::vector<std::string>> allWrapped;
    std::vector<Color> entryColors;
    for (int i = (int)log.size() - 1; i >= 0; i--) {
        const auto& entry = log[i];
        std::string line; Color col;
        formatEntry(entry.getActionType(), entry.getDescription(),
                    entry.getUsername(), entry.getTurn(), line, col);
        auto wrapped = wrapText(line, rowSz, listW);
        contentH += (float)wrapped.size() * lineH;
        allWrapped.push_back(wrapped);
        entryColors.push_back(col);
    }

    if (contentH > (maxY - listY)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            logScroll -= wheel * 40.0f * globalScale;
            logScroll = std::max(0.0f, std::min(logScroll, contentH - (maxY - listY)));
        }
    } else {
        logScroll = 0.0f;
    }

    BeginScissorMode((int)(listX - 2), (int)listY, (int)(listW + 4), (int)(maxY - listY));
    float drawY = listY - logScroll;
    for (int wi = 0; wi < (int)allWrapped.size(); wi++) {
        Color col = (wi < (int)entryColors.size()) ? entryColors[wi] : Color{60,30,25,220};
        for (int li = 0; li < (int)allWrapped[wi].size(); li++) {
            if (drawY + lineH >= listY && drawY < maxY) {
                DrawText(allWrapped[wi][li].c_str(), (int)listX, (int)drawY, rowSz, col);
            }
            drawY += lineH;
        }
    }
    EndScissorMode();

    if (contentH > (maxY - listY)) {
        float sbX = listX + listW - 6.0f * globalScale;
        float sbH = maxY - listY;
        float thumbH = std::max(20.0f, ((maxY - listY) / contentH) * sbH);
        float thumbY = listY + (logScroll / contentH) * sbH;
        DrawRectangleRec({sbX, listY, 6.0f * globalScale, sbH}, {200,190,180,120});
        DrawRectangleRec({sbX, thumbY, 6.0f * globalScale, thumbH}, {148,73,68,200});
    }
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
void GameScreen::startAuction(PropertyTile* prop) {
    if (!gs) return;
    Player* trigger = gs->currentTurnPlayer();

    if (prop) {
        auctionProp = prop;  // bankruptcy queue auction — no specific trigger
    } else {
        int tileIdx = trigger->getCurrTile();
        auctionProp = dynamic_cast<PropertyTile*>(gs->tiles[tileIdx]);
    }
    if (!auctionProp) return;

    // build bidder order: all active players (bankruptcy) or others first + trigger last
    auctionOrder.clear();
    int sz = (int)gs->turn_order.size();
    if (prop) {
        // bankruptcy auction: all non-bankrupt players bid
        for (auto* pl : gs->turn_order)
            if (pl->getStatus() != "BANKRUPT") auctionOrder.push_back(pl);
    } else {
        int trigIdx = 0;
        for (int i = 0; i < sz; i++) {
            if (gs->turn_order[i] == trigger) { trigIdx = i; break; }
        }
        for (int i = 1; i < sz; i++) {
            Player* pl = gs->turn_order[(trigIdx + i) % sz];
            if (pl->getStatus() != "BANKRUPT") auctionOrder.push_back(pl);
        }
        auctionOrder.push_back(trigger);  // trigger bids last
    }

    auctionIdx = 0;
    auctionHighBid = 0;
    auctionHighBidder = nullptr;
    auctionPassed.assign(auctionOrder.size(), false);
    auctionInputLen = 0;
    auctionInputBuf[0] = '\0';
    auctionMode = true;
}

void GameScreen::startNextBankruptAuction() {
    if (!gs || gs->bankruptAuctionQueue.empty()) return;
    PropertyTile* next = gs->bankruptAuctionQueue.front();
    gs->bankruptAuctionQueue.erase(gs->bankruptAuctionQueue.begin());
    startAuction(next);
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

    // helper: advance idx to next non-passed bidder safely
    auto advanceAuction = [&]() {
        int sz = (int)auctionOrder.size();
        int next = (auctionIdx + 1) % sz;
        int steps = 0;
        while (auctionPassed[next] && steps < sz) {
            next = (next + 1) % sz;
            steps++;
        }
        auctionIdx = next;
    };

    // Auto-pass if player can't afford to beat current highest bid
    if (bidder->getBalance() <= auctionHighBid) {
        auctionPassed[auctionIdx] = true;
        pushLog(bidder->getUsername() + " tidak sanggup menawar (saldo kurang dari M" + std::to_string(auctionHighBid) + ")");
        advanceAuction();
        return;
    }

    // block auction input while error popup is shown
    if (showPopup) {
        if (btnPopupOk.isClicked()) showPopup = false;
        return;
    }

    if (btnAuctionBid.isClicked() && auctionInputLen > 0) {
        int bid = std::stoi(std::string(auctionInputBuf));
        bool valid = (bid > auctionHighBid) && (bid <= bidder->getBalance());
        if (valid) {
            auctionHighBid = bid;
            auctionHighBidder = bidder;
            auctionInputLen = 0; auctionInputBuf[0] = '\0';
            advanceAuction();
        } else {
            std::string msg;
            if (bid > bidder->getBalance()) {
                msg = "Tawaran melebihi saldo Anda.\nSaldo: M" + std::to_string(bidder->getBalance());
            } else {
                msg = "Tawaran harus lebih tinggi dari\nM" + std::to_string(auctionHighBid) + " (tawaran tertinggi saat ini).";
            }
            showPopup = true; popupTitle = "TAWARAN TIDAK VALID"; popupMsg = msg;
        }
    }

    if (btnAuctionPass.isClicked()) {
        auctionPassed[auctionIdx] = true;
        auctionInputLen = 0; auctionInputBuf[0] = '\0';
        advanceAuction();
    }

    // count remaining (non-passed) bidders
    int remaining = 0;
    for (bool p2 : auctionPassed) if (!p2) remaining++;

    if (remaining == 0 || remaining == 1) {
        // If 1 remaining but no one has bid yet, that last person wins with bid 0
        if (remaining == 1 && auctionHighBidder == nullptr) {
            for (int i = 0; i < (int)auctionOrder.size(); i++) {
                if (!auctionPassed[i]) {
                    auctionHighBidder = auctionOrder[i];
                    auctionHighBid = 0;
                    break;
                }
            }
        }

        // auction ends — winner takes property
        if (auctionHighBidder) {
            auctionHighBidder->operator+=(-auctionHighBid);
            auctionHighBidder->addOwnedProperty(auctionProp);
            gs->recomputeMonopolyForGroup(auctionProp->getColorGroup());
            gs->addLog(*auctionHighBidder, "LELANG", auctionProp->getTileCode()
                       + " M" + std::to_string(auctionHighBid));
            pushLog(auctionHighBidder->getUsername() + " menang lelang "
                    + auctionProp->getTileName() + " M" + std::to_string(auctionHighBid));
        } else {
            pushLog("Tidak ada pemenang lelang — " + (auctionProp ? auctionProp->getTileName() : "properti") + " kembali ke bank.");
        }
        drainAndStoreEvents();
        gameLoop->checkWinGui();
        if (gs->game_over) gameOverOverlay = true;
        auctionMode = false;
        auctionProp = nullptr;
    }
}

void GameScreen::drawAuctionUI(float /*cardX*/, float /*cardScale*/) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 180});

    // ── scale & draw the existing Lelang_back.png asset ───────────────────
    float maxW = sw * 0.45f, maxH = sh * 0.85f;
    float sc   = 1.0f;
    if (lelangBackTexture.width > 0 && lelangBackTexture.height > 0)
        sc = std::min(maxW / (float)lelangBackTexture.width,
                      maxH / (float)lelangBackTexture.height);
    float bgW = lelangBackTexture.width  * sc;
    float bgH = lelangBackTexture.height * sc;
    float bgX = (sw - bgW) / 2.0f;
    float bgY = (sh - bgH) / 2.0f;
    DrawTextureEx(lelangBackTexture, {bgX, bgY}, 0.0f, sc, WHITE);

    // ── overlay layout (matches texture structure) ────────────────────────
    //  Zone A (top, above main divider ~22%): property name
    //  Zone B (below main divider): Giliran → sub-divider → Harga Dasar
    //                               → Tawaran tertinggi → player list
    //                               → input → BID/PASS

    int  nameSz   = (int)(32 * globalScale);  // property name
    int  gilSz    = (int)(20 * globalScale);  // "Giliran:"
    int  hargaSz  = (int)(13 * globalScale);  // "Harga Dasar" (small, centered)
    int  tawaranSz= (int)(15 * globalScale);  // "Tawaran tertinggi" (bigger)
    int  playerSz = (int)(12 * globalScale);  // player list (smallest)
    float rowGap  = 5.0f * globalScale;
    float pad     = bgW * 0.10f;
    float leftX   = bgX + pad;
    float centerX = bgX + bgW / 2.0f;
    // main divider is at ~22% of texture height
    float dividerY = bgY + bgH * 0.22f;

    // ── Zone A: Nama properti (above main divider, vertically centered) ───
    std::string propName = auctionProp ? auctionProp->getTileName() : "Properti";
    int pnW = MeasureText(propName.c_str(), nameSz);
    float propNameY = bgY + (dividerY - bgY - nameSz + 10.0f) / 2.0f;
    DrawText(propName.c_str(), (int)(centerX - pnW / 2.0f), (int)propNameY,
             nameSz, {80,40,35,255});

    // ── Zone B ────────────────────────────────────────────────────────────
    float cy = dividerY + 12.0f * globalScale;

    // "Giliran: player" — centered
    if (!auctionOrder.empty() && auctionIdx < (int)auctionOrder.size()) {
        std::string cur = "Giliran: " + auctionOrder[auctionIdx]->getUsername();
        int cW = MeasureText(cur.c_str(), gilSz);
        DrawText(cur.c_str(), (int)(centerX - cW / 2.0f), (int)cy, gilSz, {148,73,68,255});
        cy += gilSz + rowGap;
    }

    cy += 18.0f * globalScale;

    // "Harga Dasar" — small, centered
    if (auctionProp) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Harga Dasar: M%d", auctionProp->getBuyPrice());
        int bW = MeasureText(buf, hargaSz);
        DrawText(buf, (int)(centerX - bW / 2.0f), (int)cy, hargaSz, {120,80,75,255});
        cy += hargaSz + rowGap * 2;
    }

    // "Tawaran tertinggi" — bigger font, left-aligned
    char bidBuf[128];
    if (auctionHighBidder) {
        std::snprintf(bidBuf, sizeof(bidBuf), "Tawaran tertinggi: M%d  (%s)",
                      auctionHighBid, auctionHighBidder->getUsername().c_str());
        DrawText(bidBuf, (int)leftX, (int)cy, tawaranSz, {50,130,65,255});
    } else {
        DrawText("Tawaran tertinggi: M0", (int)leftX, (int)cy, tawaranSz, {160,100,80,255});
    }
    cy += tawaranSz + rowGap * 2;

    // Player list — smaller font, left-aligned
    for (int i = 0; i < (int)auctionOrder.size(); i++) {
        std::string name = auctionOrder[i]->getUsername() + ":";
        if (auctionPassed[i])                          name += "  [PASS]";
        else if (auctionOrder[i] == auctionHighBidder) name += "  M" + std::to_string(auctionHighBid);
        Color c = auctionPassed[i]  ? Color{170,170,170,255}
                : (i == auctionIdx) ? Color{50,140,70,255}
                                    : Color{50,30,25,255};
        std::string prefix = (i == auctionIdx ? "> " : "   ");
        DrawText((prefix + name).c_str(), (int)leftX, (int)cy, playerSz, c);
        cy += playerSz + rowGap;
    }
    cy += rowGap * 2;

    // Input field — centered
    float inW = bgW * 0.55f, inH = 28.0f * globalScale;
    float inX = centerX - inW / 2.0f;
    DrawRectangleRec({inX, cy, inW, inH}, WHITE);
    DrawRectangleLinesEx({inX, cy, inW, inH}, 1.5f, {80,40,35,255});
    std::string inp(auctionInputBuf);
    if ((int)(GetTime() * 2) % 2 == 0) inp += "|";
    int inpW = MeasureText(inp.c_str(), gilSz);
    DrawText(inp.c_str(), (int)(inX + (inW - inpW) / 2.0f),
             (int)(cy + (inH - gilSz) / 2.0f), gilSz, BLACK);
    cy += inH + rowGap * 2;

    // BID / PASS — centered
    float btnW = bgW * 0.30f, btnH = 32.0f * globalScale;
    float btnGap = bgW * 0.04f;
    float bidX  = centerX - btnW - btnGap / 2.0f;
    float passX = centerX + btnGap / 2.0f;
    btnAuctionBid.loadAsText("BID",  bidX,  cy, btnW, btnH,
                             {148,73,68,255},{170,85,78,255}, WHITE);
    btnAuctionPass.loadAsText("PASS", passX, cy, btnW, btnH,
                              {255,235,202,255},{255,220,180,255},{80,40,35,255});
    btnAuctionBid.draw();
    btnAuctionPass.draw();
}

// ══════════════════════════════════════════════════════════════════════════════
// SKILL CARDS
// ══════════════════════════════════════════════════════════════════════════════

// Shared helper: resolve landing after a skill-card move (MOVE card / TELEPORT).
// Mirrors the unowned-property interception logic from executeRollResult.
void GameScreen::resolveCardLanding(Player& p, int tileIdx) {
    Tile* tile = gs->tiles[tileIdx];
    auto* prop = dynamic_cast<PropertyTile*>(tile);
    bool unownedStreet = prop && prop->getTileOwner() == nullptr
                         && prop->getTileType() == "STREET";

    if (unownedStreet) {
        if (p.getBalance() < prop->getBuyPrice()) {
            startAuction();
        } else {
            propertyLandingPending = true;
            propertyLandingTileIdx = tileIdx;
            propertyLandingDouble  = false;
        }
        drainAndStoreEvents();
        gameLoop->checkWinGui();
        if (gs->game_over) gameOverOverlay = true;
        return;
    }

    // PPH intercept (STREET owned by self with no buildings)
    if (checkPPH(p)) return;

    try {
        landing->applyLanding(p);
    } catch (const UnablePayRentException& e) {
        std::string msg = e.what();
        int rent = 0; auto pos = msg.find('M');
        if (pos != std::string::npos) rent = std::stoi(msg.substr(pos + 1));
        debtMode = true; debtLanding = true; debtAmount = rent > 0 ? rent : debtAmount;
        pushLog("Tidak sanggup bayar sewa M" + std::to_string(debtAmount));
    } catch (const UnablePayPBMTaxException& e) {
        std::string msg = e.what();
        int amt = 0; auto pos = msg.find('M');
        if (pos != std::string::npos) amt = std::stoi(msg.substr(pos + 1));
        debtMode = true; debtLanding = true; debtAmount = amt > 0 ? amt : debtAmount;
        pushLog("Tidak sanggup bayar pajak PBM M" + std::to_string(debtAmount));
    } catch (const UnablePayPPHTaxException&) {
        pushLog("Tidak sanggup bayar pajak PPH.");
    } catch (const InsufficientMoneyException& e) {
        std::string msg = e.what();
        int need = 0; auto pos = msg.find('M');
        if (pos != std::string::npos) need = std::stoi(msg.substr(pos + 1));
        debtMode = true; debtLanding = false; debtAmount = need;
        pushLog("Tidak sanggup bayar kartu! Perlu naikkan M" + std::to_string(need) + " via gadai.");
    } catch (...) {}

    drainAndStoreEvents();
    gameLoop->checkWinGui();
    if (gs->game_over) gameOverOverlay = true;
    if (!gs->game_over) checkFestival(p);
}

void GameScreen::useCardImmediate(int cardIdx) {
    Player* p = gs->currentTurnPlayer();
    if (!p) return;
    int oldTile = p->getCurrTile();
    cardProc->cmdGunakanKemampuan(*p, cardIdx);
    int newTile = p->getCurrTile();
    drainAndStoreEvents();
    if (newTile != oldTile) {
        landing->applyGoSalary(*p, oldTile);
        pushLog("Kartu MOVE: ke [" + gs->tiles[newTile]->getTileCode() + "] " + gs->tiles[newTile]->getTileName());
        resolveCardLanding(*p, newTile);
    } else {
        gameLoop->checkWinGui();
        if (gs->game_over) gameOverOverlay = true;
    }
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
        if (opts.empty()) { pushLog("Tidak ada pemain lain."); return; }
        pendingCardIdx = cardIdx;
        cardMode       = mode;
        openSelPopup("Pilih Pemain", opts, SelAction::LASSO);
    } else if (mode == CardMode::DEMOLITION_SELECT) {
        // step 1: pick opponent who has buildings
        std::vector<std::string> opts;
        for (auto* pl : gs->players) {
            if (pl == p || pl->getStatus() == "BANKRUPT") continue;
            for (auto* prop : pl->getOwnedProperties())
                if (prop->getLevel() > 0) { opts.push_back(pl->getUsername()); break; }
        }
        if (opts.empty()) { pushLog("Tidak ada bangunan lawan."); return; }
        pendingCardIdx       = cardIdx;
        cardMode             = mode;
        demolitionPlayerPick = -1;
        openSelPopup("DEMOLITION — Pilih Pemain", opts, SelAction::DEMOLITION_PLAYER);
    }
}

void GameScreen::finishTeleport(int tileIdx) {
    Player* p = gs->currentTurnPlayer();
    if (!p) { cardMode = CardMode::NONE; pendingCardIdx = -1; return; }
    int oldTile = p->getCurrTile();
    p->removeHandCard(pendingCardIdx);
    SkillCardManager::applyTeleport(*p, tileIdx, gs->board.getTileCount());
    p->setSkillUsed(true);
    gs->addLog(*p, "GUNAKAN_KEMAMPUAN", "TeleportCard -> " + gs->tiles[tileIdx]->getTileCode());
    pushLog("Teleport ke [" + gs->tiles[tileIdx]->getTileCode() + "] " + gs->tiles[tileIdx]->getTileName());
    cardMode = CardMode::NONE; pendingCardIdx = -1;
    // GO salary only if moving forward past tile 0
    landing->applyGoSalary(*p, oldTile);
    resolveCardLanding(*p, tileIdx);
}

void GameScreen::finishLasso(int playerIdx) {
    Player* p = gs->currentTurnPlayer();
    if (!p) { cardMode = CardMode::NONE; pendingCardIdx = -1; return; }

    std::vector<Player*> others;
    for (auto* pl : gs->players)
        if (pl != p && pl->getStatus() != "BANKRUPT") others.push_back(pl);
    if (playerIdx < 0 || playerIdx >= (int)others.size()) {
        cardMode = CardMode::NONE; pendingCardIdx = -1; return;
    }
    Player* target = others[playerIdx];
    int destTile = p->getCurrTile();
    p->removeHandCard(pendingCardIdx);

    // If target is jailed, free them before pulling
    if (target->getStatus() == "JAIL") {
        target->setStatus("ACTIVE");
        gs->jail_turns.erase(target);
        pushLog(target->getUsername() + " bebas dari penjara (efek LASSO)!");
    }

    SkillCardManager::applyLasso(*p, *target, destTile);
    p->setSkillUsed(true);
    gs->addLog(*p, "GUNAKAN_KEMAMPUAN",
               "LassoCard -> " + target->getUsername()
               + " ke " + gs->tiles[destTile]->getTileCode());
    pushLog(target->getUsername() + " ditarik ke " + gs->tiles[destTile]->getTileName());

    // Defer landing resolution to the target player's next turn
    // (they will resolve the tile effect before rolling dice)
    gs->deferredLandings[target] = destTile;
    drainAndStoreEvents();
    gameLoop->checkWinGui();
    if (gs->game_over) gameOverOverlay = true;
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
    pushLog("Bangunan di " + tProp->getTileCode() + " dihancurkan!");
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
    float sectX = cardX + pad + 20.0f * globalScale;
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
        std::string label = card->getCardType() + " CARD";
        bool usable = !used;
        if (debtMode) {
            std::string t = card->getCardType();
            usable = usable && (t == "SHIELD" || t == "DISCOUNT");
        }
        Color bg    = usable ? Color{255,235,202,255} : Color{210,210,210,255};
        Color hover = usable ? Color{255,220,180,255} : Color{210,210,210,255};
        Color fg    = usable ? Color{80,40,35,255}   : Color{170,170,170,255};
        btnSkillCards[i].loadAsText(label, sectX, sectY, sectW, btnH, bg, hover, fg);
        btnSkillCards[i].setDisabled(!usable);
        btnSkillCards[i].draw();
        Color border = usable ? Color{80,40,35,255} : Color{170,170,170,180};
        DrawRectangleLinesEx({sectX, sectY, sectW, btnH}, 1.0f, border);
        sectY += btnH + btnGap;
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// SELECTION POPUP (LASSO / DEMOLITION)
// ══════════════════════════════════════════════════════════════════════════════
void GameScreen::openSelPopup(const std::string& title, const std::vector<std::string>& opts,
                               SelAction action) {
    selPopupTitle  = title;
    selOptions     = opts;
    selPopupResult = -1;
    selAction      = action;
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
    // Click outside the popup box = cancel
    if (showSelPopup && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float popW = 340.0f * globalScale;
        float popH = (80.0f + selOptions.size() * 32.0f) * globalScale;
        float popX = (sw - popW) / 2.0f, popY = (sh - popH) / 2.0f;
        Vector2 mouse = GetMousePosition();
        if (!CheckCollisionPointRec(mouse, {popX, popY, popW, popH})) {
            showSelPopup = false; selPopupResult = -1;
            cardMode = CardMode::NONE; pendingCardIdx = -1; demolitionPlayerPick = -1;
            selAction = SelAction::NONE;
        }
    }
    if (!showSelPopup && selPopupResult >= 0) {
        switch (selAction) {
        case SelAction::LASSO:
            finishLasso(selPopupResult);
            break;
        case SelAction::DEMOLITION_PLAYER:
            if (demolitionPlayerPick < 0) {
                demolitionPlayerPick = selPopupResult;
                Player* p = gs->currentTurnPlayer();
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
                    openSelPopup("DEMOLITION — Pilih Bangunan", propOpts,
                                 SelAction::DEMOLITION_PROP);
                }
            }
            break;
        case SelAction::DEMOLITION_PROP:
            finishDemolition(demolitionPlayerPick, selPopupResult);
            break;
        case SelAction::GADAI:
            if (selPopupResult < (int)gadaiProps.size()) {
                Player* p = gs->currentTurnPlayer();
                if (p && gadaiProps[selPopupResult]) {
                    if (!PropertyManager::mortgage(*p, *gadaiProps[selPopupResult])) {
                        showPopup = true; popupTitle = "GADAI";
                        popupMsg  = "Tidak bisa menggadai properti ini.";
                    } else {
                        drainAndStoreEvents();
                        pushLog("Properti digadaikan. Saldo: M"
                                    + std::to_string(p->getBalance()));
                    }
                }
            }
            selAction = SelAction::NONE;
            break;
        case SelAction::TEBUS:
            if (selPopupResult < (int)tebusProps.size()) {
                Player* p = gs->currentTurnPlayer();
                if (p && tebusProps[selPopupResult]) {
                    bankruptcy->cmdTebus(*p, tebusProps[selPopupResult]->getTileCode());
                    drainAndStoreEvents();
                    pushLog("Properti ditebus. Saldo: M"
                                + std::to_string(p->getBalance()));
                }
            }
            selAction = SelAction::NONE;
            break;
        default:
            break;
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
    const char* hint = "Klik di luar untuk batal";
    DrawText(hint, (int)(popX + 16.0f*globalScale), (int)(popY + popH - 16.0f*globalScale),
             hintSz, {160,120,100,160});

    for (auto& b : btnSelOptions) b.draw();
}

// ──────────────────────────────────────────────────────────────────────────────
// GADAI / TEBUS POPUP HELPERS
// ──────────────────────────────────────────────────────────────────────────────
void GameScreen::openGadaiPopup() {
    if (!gs) return;
    Player* p = gs->currentTurnPlayer();
    if (!p) return;

    gadaiProps.clear();
    std::vector<std::string> opts;
    for (auto* prop : p->getOwnedProperties()) {
        if (prop && prop->getTileOwner() == p && !prop->isMortgage()) {
            std::string label = prop->getTileCode() + " " + prop->getTileName()
                                + "  [+M" + std::to_string(prop->getMortgageValue()) + "]";
            opts.push_back(label);
            gadaiProps.push_back(prop);
        }
    }
    if (opts.empty()) {
        showPopup = true; popupTitle = "GADAI";
        popupMsg  = "Tidak ada properti yang\nbisa digadaikan.";
        return;
    }
    openSelPopup("GADAI — Pilih Properti", opts, SelAction::GADAI);
}

void GameScreen::openTebusPopup() {
    if (!gs) return;
    Player* p = gs->currentTurnPlayer();
    if (!p) return;

    tebusProps.clear();
    std::vector<std::string> opts;
    for (auto* prop : p->getOwnedProperties()) {
        if (prop && prop->getTileOwner() == p && prop->isMortgage()) {
            int redeemCost = static_cast<int>(prop->getMortgageValue() * 1.1f);
            std::string label = prop->getTileCode() + " " + prop->getTileName()
                                + "  [-M" + std::to_string(redeemCost) + "]";
            opts.push_back(label);
            tebusProps.push_back(prop);
        }
    }
    if (opts.empty()) {
        showPopup = true; popupTitle = "TEBUS";
        popupMsg  = "Tidak ada properti yang\nsedang digadaikan.";
        return;
    }
    openSelPopup("TEBUS — Pilih Properti", opts, SelAction::TEBUS);
}

// ──────────────────────────────────────────────────────────────────────────────
// DEBT MODE DRAW
// ──────────────────────────────────────────────────────────────────────────────
void GameScreen::drawDebtMode(float cardX, float cardScale) {
    if (!gs) return;
    Player* p = gs->currentTurnPlayer();

    float pad   = 12.0f * globalScale;
    float boxX  = cardX + pad + 20.0f * globalScale;
    float boxY  = btnPlay.getY() + btnPlay.getHeight() + pad + 10.0f * globalScale;
    float boxW  = cardPanel.width * cardScale - 6.0f * pad;
    float boxH  = 100.0f * globalScale;

    // warning box
    DrawRectangleRec({boxX, boxY, boxW, boxH}, {255,220,210,255});
    DrawRectangleLinesEx({boxX, boxY, boxW, boxH}, 2.5f, {180,50,40,255});

    int titleSz = (int)(14 * globalScale);
    int subSz   = (int)(12 * globalScale);

    bool isCardDebt = (debtAmount == 0);  // card debt: balance went negative
    int  balance    = p ? p->getBalance() : 0;
    int  shortage   = isCardDebt ? std::max(0, -balance) : (debtAmount - balance);

    const char* titleStr = isCardDebt ? "! SALDO NEGATIF — EFEK KARTU"
                                       : "! TIDAK SANGGUP BAYAR";
    DrawText(titleStr, (int)(boxX + 8*globalScale), (int)(boxY + 8*globalScale),
             titleSz, {180,50,40,255});

    char buf1[64], buf2[64];
    if (isCardDebt) {
        std::snprintf(buf1, sizeof(buf1), "Saldo: M%d", balance);
        std::snprintf(buf2, sizeof(buf2), "Gadai properti untuk naikkan M%d", shortage);
    } else {
        std::snprintf(buf1, sizeof(buf1), "Tagihan: M%d", debtAmount);
        std::snprintf(buf2, sizeof(buf2), "Saldo: M%d  (kurang M%d)", balance, shortage);
    }
    DrawText(buf1, (int)(boxX + 8*globalScale),
             (int)(boxY + 8*globalScale + titleSz + 4*globalScale), subSz, BLACK);
    DrawText(buf2, (int)(boxX + 8*globalScale),
             (int)(boxY + 8*globalScale + titleSz + 4*globalScale + subSz + 4*globalScale),
             subSz, {120,50,45,255});

    // section label
    float labY = boxY + boxH + 10.0f * globalScale;
    int   labSz = (int)(13 * globalScale);
    DrawText("CARA BAYAR", (int)boxX, (int)labY, labSz, {148,73,68,255});

    // GADAI only (no LELANG in debt mode)
    btnDebtGadai.draw();
    DrawRectangleLinesEx({btnDebtGadai.getX(), btnDebtGadai.getY(),
                          btnDebtGadai.getWidth(), btnDebtGadai.getHeight()},
                         1.5f, {150,100,30,255});

    // no extra hint text — skill cards are shown in the section below

    // force / bankrupt button — only if no properties left
    bool debtResolved = (debtAmount == 0) ? (p && p->getBalance() >= 0)
                                          : (p && p->getBalance() >= debtAmount);
    bool noOptions = !debtResolved && p && p->getOwnedProperties().empty();
    if (noOptions) {
        btnDebtForce.setDisabled(false);
        btnDebtForce.draw();
    }

    // skill card section still drawn so SHIELD/DISCOUNT can be clicked
    float skillY = btnDebtGadai.getY() + btnDebtGadai.getHeight() + 10.0f * globalScale;
    drawSkillCardSection(cardX, cardScale, skillY);
}

// ──────────────────────────────────────────────────────────────────────────────
// CARD DRAW POPUP (Kesempatan / Dana Umum)
// ──────────────────────────────────────────────────────────────────────────────
void GameScreen::drawCardPopup() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 170});

    Texture2D& cardTex = cardPopupIsChance ? chanceCardTex : communityCardTex;

    // scale card to fit nicely — max 55% screen height, 40% width
    float maxW = sw * 0.40f;
    float maxH = sh * 0.55f;
    float sc   = 1.0f;
    if (cardTex.width > 0 && cardTex.height > 0) {
        sc = std::min(maxW / (float)cardTex.width, maxH / (float)cardTex.height);
    }
    float cW = cardTex.width  * sc;
    float cH = cardTex.height * sc;
    float cX = (sw - cW) / 2.0f;
    float cY = (sh - cH) / 2.0f;

    DrawTextureEx(cardTex, {cX, cY}, 0.0f, sc, WHITE);

    // ── desc text: word-wrap into lines first, then draw centered ────────
    int descSz     = (int)(15 * globalScale);
    float lineH    = (float)(descSz + 6);
    float maxLineW = cW * 0.80f;
    Color descC    = {40, 30, 20, 240};

    // 1. build wrapped lines
    std::vector<std::string> lines;
    std::string remaining = cardPopupDesc;
    while (!remaining.empty()) {
        int low = 1, high = (int)remaining.size(), best = (int)remaining.size();
        while (low <= high) {
            int mid = (low + high) / 2;
            if (MeasureText(remaining.substr(0, mid).c_str(), descSz) <= (int)maxLineW)
            { best = mid; low = mid + 1; } else { high = mid - 1; }
        }
        int breakAt = best;
        if (breakAt < (int)remaining.size()) {
            size_t sp = remaining.rfind(' ', breakAt);
            if (sp != std::string::npos && sp > 0) breakAt = (int)sp;
        }
        lines.push_back(remaining.substr(0, breakAt));
        while (breakAt < (int)remaining.size() && remaining[breakAt] == ' ') breakAt++;
        remaining = remaining.substr(breakAt);
    }

    // 2. draw lines vertically centered inside card
    float totalH = lines.size() * lineH;
    float startY = cY + (cH - totalH) / 2.0f;
    for (const auto& line : lines) {
        int lw = MeasureText(line.c_str(), descSz);
        DrawText(line.c_str(), (int)(cX + (cW - lw) / 2.0f), (int)startY, descSz, descC);
        startY += lineH;
    }

    // hint inside card, near bottom
    int hintSz = (int)(10 * globalScale);
    const char* hint = "Klik di mana saja untuk lanjut";
    int hw = MeasureText(hint, hintSz);
    DrawText(hint, (int)(cX + (cW - hw) / 2.0f),
             (int)(cY + cH - hintSz - 14.0f * globalScale), hintSz, {120, 90, 60, 150});
}

// ──────────────────────────────────────────────────────────────────────────────
// JAIL CHOICE POPUP
// ──────────────────────────────────────────────────────────────────────────────
void GameScreen::drawJailChoice() {
    if (!gs) return;
    Player* p = gs->currentTurnPlayer();
    if (!p) return;

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 160});

    float popW = 380.0f * globalScale;
    float popH = 220.0f * globalScale;
    float popX = (sw - popW) / 2.0f;
    float popY = (sh - popH) / 2.0f;

    // popup box
    DrawRectangleRounded({popX, popY, popW, popH}, 0.12f, 8, {255, 243, 210, 255});
    DrawRectangleRoundedLines({popX, popY, popW, popH}, 0.12f, 8, {80, 40, 35, 255});

    // header band
    float bandH = 46.0f * globalScale;
    DrawRectangleRounded({popX, popY, popW, bandH}, 0.12f, 8, {160, 50, 45, 255});
    DrawRectangle((int)popX, (int)(popY + bandH/2.0f), (int)popW, (int)(bandH/2.0f), {160, 50, 45, 255});

    int titleSz = (int)(17 * globalScale);
    const char* title = "ANDA DI DALAM PENJARA";
    int tw = MeasureText(title, titleSz);
    DrawText(title, (int)(popX + (popW - tw)/2.0f),
             (int)(popY + (bandH - titleSz)/2.0f), titleSz, WHITE);

    // jail turn counter
    int jailTurns = gs->jail_turns.count(p) ? gs->jail_turns[p] : 0;
    int infoSz = (int)(13 * globalScale);
    int jailFine = gs->config.getJailFine();

    char turnBuf[64];
    std::snprintf(turnBuf, sizeof(turnBuf), "Giliran penjara ke-%d dari 3", jailTurns + 1);
    int tbW = MeasureText(turnBuf, infoSz);
    DrawText(turnBuf, (int)(popX + (popW - tbW)/2.0f),
             (int)(popY + bandH + 14.0f * globalScale), infoSz, {100, 60, 55, 255});

    // question text
    int qSz = (int)(14 * globalScale);
    char q1[80], q2[80];
    std::snprintf(q1, sizeof(q1), "Apakah Anda ingin membayar denda penjara");
    std::snprintf(q2, sizeof(q2), "sebesar M%d untuk bebas sekarang?", jailFine);
    int q1W = MeasureText(q1, qSz), q2W = MeasureText(q2, qSz);
    float qY = popY + bandH + 14.0f*globalScale + infoSz + 12.0f*globalScale;
    DrawText(q1, (int)(popX + (popW - q1W)/2.0f), (int)qY, qSz, BLACK);
    DrawText(q2, (int)(popX + (popW - q2W)/2.0f), (int)(qY + qSz + 4.0f*globalScale), qSz, BLACK);

    // warning if can't afford
    bool canAfford = (p->getBalance() >= jailFine);
    if (!canAfford) {
        int wSz = (int)(11 * globalScale);
        const char* warn = "(Saldo tidak cukup untuk membayar denda)";
        int wW = MeasureText(warn, wSz);
        DrawText(warn, (int)(popX + (popW - wW)/2.0f),
                 (int)(qY + 2*(qSz + 4.0f*globalScale) + 4.0f*globalScale), wSz, {180, 50, 40, 255});
    }

    // buttons
    float btnW = popW * 0.38f, btnH = 34.0f * globalScale;
    float gap  = 16.0f * globalScale;
    float btnY = popY + popH - btnH - 14.0f * globalScale;
    float payX = popX + (popW - 2*btnW - gap) / 2.0f;
    float rollX = payX + btnW + gap;

    Color payBg    = canAfford ? Color{70,160,90,255}  : Color{160,160,160,255};
    Color payHover = canAfford ? Color{90,180,110,255} : Color{160,160,160,255};
    btnJailPay.loadAsText("BAYAR DENDA", payX, btnY, btnW, btnH, payBg, payHover, WHITE);
    btnJailPay.setDisabled(!canAfford);
    btnJailPay.draw();

    btnJailRoll.loadAsText("LEMPAR DADU", rollX, btnY, btnW, btnH,
                            {220,150,40,255},{240,170,60,255}, WHITE);
    btnJailRoll.draw();
}

// ──────────────────────────────────────────────────────────────────────────────
// DROP-CARD POPUP — kartu kemampuan melebihi batas 3
// ──────────────────────────────────────────────────────────────────────────────
void GameScreen::drawDropCardPopup() {
    if (!gs) return;
    Player* p = gs->currentTurnPlayer();
    if (!p || p->getHandSize() <= 3) return;

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 170});

    int   handSz  = p->getHandSize();
    float pad     = 24.0f  * globalScale;
    float popW    = 520.0f * globalScale;
    Color cream   = {255, 243, 210, 255};   // same cream as tab menu
    Color maroon  = {80,  40,  35,  255};
    Color redCard = {148, 73,  68,  255};   // #944944
    Color redHov  = {170, 85,  78,  255};

    if (dropCardConfirm && dropCardConfirmIdx >= 0
        && dropCardConfirmIdx < handSz) {
        // ── Confirmation popup ──────────────────────────────────────────────
        float confW = 420.0f * globalScale;
        float confH = 250.0f * globalScale;
        float confX = (sw - confW) / 2.0f;
        float confY = (sh - confH) / 2.0f;

        DrawRectangleRounded({confX, confY, confW, confH}, 0.14f, 8, cream);
        DrawRectangleLinesEx({confX, confY, confW, confH}, 2.5f, maroon);

        // header stripe
        float hH = 46.0f * globalScale;
        DrawRectangleRounded({confX, confY, confW, hH}, 0.14f, 8, redCard);
        DrawRectangle((int)confX, (int)(confY+hH/2), (int)confW, (int)(hH/2), redCard);
        int hSz = (int)(16 * globalScale);
        const char* htitle = "Buang Kartu Ini?";
        int htw = MeasureText(htitle, hSz);
        DrawText(htitle, (int)(confX+(confW-htw)/2.0f),
                 (int)(confY+(hH-hSz)/2.0f), hSz, WHITE);

        // Card type + description
        auto* c = p->getHandCard(dropCardConfirmIdx);
        if (c) {
            int  tSz = (int)(15 * globalScale);
            std::string tname = c->getCardType() + " CARD";
            int tnw = MeasureText(tname.c_str(), tSz);
            DrawText(tname.c_str(), (int)(confX+(confW-tnw)/2.0f),
                     (int)(confY+hH+16.0f*globalScale), tSz, maroon);

            int  dSz = (int)(12 * globalScale);
            std::string desc = c->describe();
            int dw = MeasureText(desc.c_str(), dSz);
            DrawText(desc.c_str(), (int)(confX+(confW-dw)/2.0f),
                     (int)(confY+hH+16.0f*globalScale + tSz + 8.0f*globalScale), dSz, {80,40,35,255});

            int  qSz = (int)(13 * globalScale);
            const char* q = "Apakah ingin membuang kartu ini?";
            int qw = MeasureText(q, qSz);
            DrawText(q, (int)(confX+(confW-qw)/2.0f),
                     (int)(confY+confH-75.0f*globalScale), qSz, maroon);
        }

        // YES / NO buttons
        float btnW2 = 120.0f * globalScale, btnH2 = 36.0f * globalScale;
        float gap2  = 18.0f * globalScale;
        float yesX  = confX + (confW - 2*btnW2 - gap2) / 2.0f;
        float noX   = yesX + btnW2 + gap2;
        float btnY2 = confY + confH - btnH2 - 16.0f * globalScale;

        btnDropYes.loadAsText("YA, BUANG", yesX, btnY2, btnW2, btnH2,
                              redCard, redHov, WHITE);
        btnDropNo.loadAsText("TIDAK",      noX,  btnY2, btnW2, btnH2,
                             cream, {255,235,210,255}, maroon);
        btnDropYes.draw();
        DrawRectangleLinesEx({yesX, btnY2, btnW2, btnH2}, 1.5f, maroon);
        btnDropNo.draw();
        DrawRectangleLinesEx({noX, btnY2, btnW2, btnH2}, 1.5f, maroon);
        return;
    }

    // ── Card list popup ─────────────────────────────────────────────────────
    float btnH   = 50.0f * globalScale;
    float btnGap = 8.0f  * globalScale;
    float headerH = 58.0f * globalScale;
    float subInfoH = 28.0f * globalScale;
    float popH = headerH + subInfoH + pad*0.5f + handSz*(btnH+btnGap) + pad;
    float popX = (sw - popW) / 2.0f;
    float popY = (sh - popH) / 2.0f;

    // body
    DrawRectangleRounded({popX, popY, popW, popH}, 0.10f, 8, cream);
    DrawRectangleLinesEx({popX, popY, popW, popH}, 2.5f, maroon);

    // header
    DrawRectangleRounded({popX, popY, popW, headerH}, 0.10f, 8, redCard);
    DrawRectangle((int)popX, (int)(popY+headerH/2), (int)popW, (int)(headerH/2), redCard);
    int titleSz = (int)(16 * globalScale);
    const char* title = "! KARTU PENUH — PILIH 1 UNTUK DIBUANG";
    int tw = MeasureText(title, titleSz);
    DrawText(title, (int)(popX+(popW-tw)/2.0f),
             (int)(popY+(headerH-titleSz)/2.0f), titleSz, WHITE);

    // sub-info
    int subSz = (int)(12 * globalScale);
    char sub[80];
    std::snprintf(sub, sizeof(sub),
                  "Kamu memiliki %d kartu (maks 3). Klik kartu yang ingin dibuang:", handSz);
    int subW = MeasureText(sub, subSz);
    DrawText(sub, (int)(popX+(popW-subW)/2.0f),
             (int)(popY+headerH+8.0f*globalScale), subSz, maroon);

    // card buttons — 944944 color
    float btnY = popY + headerH + subInfoH + pad*0.5f;
    for (int i = 0; i < handSz && i < (int)btnDropCards.size(); i++) {
        auto* c = p->getHandCard(i);
        std::string label = c ? c->getCardType() : "?";
        btnDropCards[i].loadAsText(label, popX+pad, btnY,
                                   popW-2*pad, btnH, redCard, redHov, WHITE);
        btnDropCards[i].draw();
        DrawRectangleLinesEx({popX+pad, btnY, popW-2*pad, btnH}, 1.5f, maroon);
        btnY += btnH + btnGap;
    }
}
