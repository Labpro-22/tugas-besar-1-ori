#include "include/core/GameLoop.hpp"

#include "include/core/GameState.hpp"
#include "include/core/CommandHandler.hpp"
#include "include/core/BotController.hpp"
#include "include/core/LandingProcessor.hpp"
#include "include/core/BankruptcyProcessor.hpp"
#include "include/core/CardProcessor.hpp"
#include "include/core/RentCollector.hpp"
#include "include/core/GameContext.hpp"
#include "include/core/GameConfig.hpp"
#include "include/models/card/ChanceCard.hpp"
#include "include/models/card/CommunityChest.hpp"
#include "include/models/card/SpecialPowerCard.hpp"
#include "include/models/player/Bot.hpp"
#include "include/models/player/Player.hpp"
#include "include/models/tiles/PropertyTile.hpp"
#include "include/utils/file-manager/FileManager.hpp"
#include "include/utils/exceptions/SaveLoadException.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <set>
#include <string>
#include <tuple>
#include <vector>

using namespace std;

namespace {
    int readInt(const string &prompt, int lo, int hi) {
        int v;
        while (true) {
            cout << prompt;
            if (cin >> v && v >= lo && v <= hi) {
                return v;
            }
            cout << "  Input tidak valid. Masukkan " << lo << "-" << hi << ".\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }

    char readChar(const string &prompt, const string &valid) {
        char c;
        while (true) {
            cout << prompt;
            if (cin >> c) {
                c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
                if (valid.find(c) != string::npos) {
                    return c;
                }
            }
            cout << "  Input tidak valid.\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }
}

// ── initDecks ─────────────────────────────────────────────────────────────────
void GameLoop::initDecks() {
    int boardSize = state->board.getTileCount();
    int jailPos = 0;

    for (int i = 0; i < boardSize; i++) {
        if (state->tiles[i]->getTileType() == "JAIL") { 
            jailPos = i; 
            break; 
        }
    }

    vector<int> railroadPos;
    for (int i = 0; i < boardSize; i++) {
        if (state->tiles[i]->getTileType() == "RAILROAD") {
            railroadPos.push_back(i);
        }
    }

    state->chance_cards = {
        new StepbackCard(boardSize),
        new NearestStreetCard(boardSize, railroadPos),
        new JailCard(jailPos)
    };

    for (auto *c : state->chance_cards) {
        state->chance_deck.addCard(c);
    }
    state->chance_deck.shuffle();

    state->community_cards = {
        new HappyBirthdayCard(),
        new SickCard(),
        new LegislativeCard()
    };

    for (auto *c : state->community_cards) {
        state->community_deck.addCard(c);
    }
    state->community_deck.shuffle();
}

// ── distributeSkillCards ──────────────────────────────────────────────────────
void GameLoop::distributeSkillCards() {
    int boardSize = state->board.getTileCount();
    mt19937 rngInit(chrono::steady_clock::now().time_since_epoch().count());
    uniform_int_distribution<int> stepDist(1, 6);
    uniform_int_distribution<int> pctDist(10, 50);

    state->skill_cards = {
        new MoveCard(stepDist(rngInit), boardSize),
        new MoveCard(stepDist(rngInit), boardSize),
        new MoveCard(stepDist(rngInit), boardSize),
        new MoveCard(stepDist(rngInit), boardSize),
        new DiscountCard(pctDist(rngInit)),
        new DiscountCard(pctDist(rngInit)),
        new DiscountCard(pctDist(rngInit)),
        new ShieldCard(),
        new ShieldCard(),
        new TeleportCard(),
        new TeleportCard(),
        new LassoCard(),
        new LassoCard(),
        new DemolitionCard(),
        new DemolitionCard()
    };

    vector<SpecialPowerCard*> pool = state->skill_cards;
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    shuffle(pool.begin(), pool.end(), rng);

    for (int i = 0; i < static_cast<int>(state->players.size()) && i < static_cast<int>(pool.size()); i++) {
        state->players[i]->addHandCard(pool[i]);
    }
}

// ── promptNewGame ─────────────────────────────────────────────────────────────
tuple<vector<Player*>, vector<Player*>, int> GameLoop::promptNewGame(int initBalance) {
    int count = readInt("Jumlah pemain (2-4): ", 2, 4);
    vector<Player*> ps;

    for (int i = 0; i < count; i++) {
        cout << "\n--- Pemain " << (i + 1) << " ---\n";
        string name;
        cout << "Username: ";
        cin >> name;
        
        char type = readChar("Tipe (H=Human, B=Bot): ", "HB");
        Player *p = (type == 'B') ? static_cast<Player*>(new Bot(name)) : new Player(name);
        
        p->operator+=(initBalance);
        ps.push_back(p);
    }

    vector<Player*> order = ps;
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    shuffle(order.begin(), order.end(), rng);

    cout << "\n=== Urutan Giliran ===\n";
    for (int i = 0; i < static_cast<int>(order.size()); i++) {
        cout << (i + 1) << ". " << order[i]->getUsername() << "\n";
    }

    int activeId = 0;
    for (int i = 0; i < static_cast<int>(ps.size()); i++) {
        if (ps[i] == order[0]) { 
            activeId = i; 
            break; 
        }
    }

    return {ps, order, activeId};
}

// ── buildPlayersFromState ─────────────────────────────────────────────────────
tuple<vector<Player*>, vector<Player*>, int> GameLoop::buildPlayersFromState(const GameStates::SaveState &sstate) {
    vector<Player*> ps;
    map<string, Player*> byName;

    for (const auto &s : sstate.players) {
        Player *p = s.is_bot ? static_cast<Player*>(new Bot(s.username)) : new Player(s.username);
        p->operator+=(s.money);
        p->setStatus((s.status == "JAILED") ? "JAIL" : s.status);
        ps.push_back(p);
        byName[s.username] = p;
    }

    vector<Player*> order;
    for (const auto &name : sstate.turn_order) {
        auto it = byName.find(name);
        if (it != byName.end()) {
            order.push_back(it->second);
        }
    }

    int activeId = 0;
    auto it = byName.find(sstate.active_turn_player);
    if (it != byName.end()) {
        for (int i = 0; i < static_cast<int>(ps.size()); i++) {
            if (ps[i] == it->second) { 
                activeId = i; 
                break; 
            }
        }
    }

    return {ps, order, activeId};
}

// ── applyPropertyState ────────────────────────────────────────────────────────
void GameLoop::applyPropertyState(const GameStates::SaveState &sstate) {
    map<string, Player*> byName;
    for (auto *p : state->players) {
        byName[p->getUsername()] = p;
    }

    for (const auto &ps : sstate.players) {
        auto it = byName.find(ps.username);
        if (it == byName.end()) continue;

        int idx = state->board.getTileIndex(ps.tile_code);
        if (idx >= 0) {
            it->second->setCurrTile(idx);
        }

        it->second->clearHandCards();
        int boardSize = state->board.getTileCount();

        for (const auto &cs : ps.hand_cards) {
            SpecialPowerCard *card = nullptr;
            if (cs.type == "MOVE") {
                int val = cs.value.empty() ? 3 : std::stoi(cs.value);
                card = new MoveCard(val, boardSize);
            } else if (cs.type == "DISCOUNT") {
                int val = cs.value.empty() ? 50 : std::stoi(cs.value);
                card = new DiscountCard(val);
            } else if (cs.type == "SHIELD") { 
                card = new ShieldCard();
            } else if (cs.type == "TELEPORT") { 
                card = new TeleportCard();
            } else if (cs.type == "LASSO") { 
                card = new LassoCard();
            } else if (cs.type == "DEMOLITION") { 
                card = new DemolitionCard(); 
            }

            if (card) {
                it->second->addHandCard(card);
            }
        }

        if (ps.status == "JAIL" && ps.jail_turns > 0) {
            state->jail_turns[it->second] = ps.jail_turns;
        }
    }

    state->transaction_log.clear();
    for (const auto &ls : sstate.logs) {
        state->transaction_log.emplace_back(ls.turn, ls.username, ls.action_type, ls.detail);
    }

    for (const auto &ps : sstate.properties) {
        auto *prop = dynamic_cast<PropertyTile*>(state->board.getTileByCode(ps.tile_code));
        if (!prop) continue;

        prop->setMortgageStatus(ps.status == "MORTGAGED");
        prop->setFestivalState(ps.festival_multiplier, ps.festival_duration);
        
        int lvl = 0;
        if (ps.building_count == "H") {
            lvl = 5;
        } else if (!ps.building_count.empty() && isdigit(ps.building_count[0])) {
            lvl = stoi(ps.building_count);
        }
        prop->setLevel(lvl);

        if (ps.owner_username != "BANK") {
            auto it = byName.find(ps.owner_username);
            if (it != byName.end()) {
                it->second->addOwnedProperty(prop);
            }
        }
    }
    state->current_turn = sstate.current_turn;
}

// ── buildSaveState ────────────────────────────────────────────────────────────
GameStates::SaveState GameLoop::buildSaveState() const {
    GameStates::SaveState s;
    s.current_turn = state->current_turn;
    s.max_turn = state->max_turn;

    for (auto *p : state->players) {
        GameStates::PlayerState ps;
        ps.username = p->getUsername();
        ps.money = p->getBalance();
        ps.status = (p->getStatus() == "JAIL") ? "JAILED" : p->getStatus();
        
        int tidx = p->getCurrTile();
        if (tidx >= 0 && tidx < static_cast<int>(state->tiles.size())) {
            ps.tile_code = state->tiles[tidx]->getTileCode();
        }

        for (auto *c : p->getHandCards()) {
            GameStates::CardState cs;
            cs.type = c->getCardType();
            if (cs.type == "MOVE") {
                cs.value = std::to_string(static_cast<MoveCard*>(c)->getMoveValue());
            } else if (cs.type == "DISCOUNT") {
                cs.value = std::to_string(static_cast<DiscountCard*>(c)->getDiscountValue());
            }
            ps.hand_cards.push_back(cs);
        }

        ps.is_bot = (dynamic_cast<Bot*>(p) != nullptr);
        auto jt = state->jail_turns.find(p);
        ps.jail_turns = (jt != state->jail_turns.end()) ? jt->second : 0;
        s.players.push_back(ps);
    }

    for (auto *p : state->turn_order) {
        s.turn_order.push_back(p->getUsername());
    }

    if (state->active_player_id >= 0 && state->active_player_id < static_cast<int>(state->players.size())) {
        s.active_turn_player = state->players[state->active_player_id]->getUsername();
    }

    for (auto *t : state->tiles) {
        auto *prop = dynamic_cast<PropertyTile*>(t);
        if (!prop) continue;

        GameStates::PropertyState ps;
        ps.tile_code = prop->getTileCode();
        ps.type = prop->getTileType();
        ps.owner_username = prop->getTileOwner() ? prop->getTileOwner()->getUsername() : "BANK";
        ps.status = prop->isMortgage() ? "MORTGAGED" : (prop->getTileOwner() ? "OWNED" : "BANK");
        ps.festival_multiplier = prop->getFestivalMultiplier();
        ps.festival_duration = prop->getFestivalDuration();
        
        int lvl = prop->getLevel();
        ps.building_count = (lvl == 5) ? "H" : to_string(lvl);
        s.properties.push_back(ps);
    }

    for (const auto &le : state->transaction_log) {
        GameStates::LogState ls;
        ls.turn = le.getTurn(); 
        ls.username = le.getUsername();
        ls.action_type = le.getActionType(); 
        ls.detail = le.getDescription();
        s.logs.push_back(ls);
    }

    return s;
}

// ── nextTurn ──────────────────────────────────────────────────────────────────
void GameLoop::nextTurn() {
    Player *currentPlayer = state->currentTurnPlayer();

    for (auto *t : state->tiles) {
        auto *prop = dynamic_cast<PropertyTile*>(t);
        if (!prop || prop->getFestivalDuration() <= 0) continue;

        Player *owner = prop->getTileOwner();
        if (!owner || owner == currentPlayer) {
            int newDur = prop->getFestivalDuration() - 1;
            if (newDur <= 0) {
                prop->setFestivalState(1, 0);
            } else {
                prop->setFestivalState(prop->getFestivalMultiplier(), newDur);
            }
        }
    }

    if (!state->turn_order.empty()) {
        state->turn_order[state->turn_order_idx]->clearTurnModifiers();
    }

    int sz = static_cast<int>(state->turn_order.size());
    int attempts = 0;

    do {
        int prev = state->turn_order_idx;
        state->turn_order_idx = (state->turn_order_idx + 1) % sz;
        if (state->turn_order_idx <= prev) {
            state->current_turn++;
        }
        attempts++;
    } while (state->turn_order[state->turn_order_idx]->getStatus() == "BANKRUPT" && attempts <= sz);

    Player *next = state->turn_order[state->turn_order_idx];
    for (int i = 0; i < static_cast<int>(state->players.size()); i++) {
        if (state->players[i] == next) { 
            state->active_player_id = i; 
            break; 
        }
    }
}

// ── checkWinCondition ─────────────────────────────────────────────────────────
void GameLoop::checkWinCondition() {
    if (state->activePlayerCount() <= 1) {
        for (auto *p : state->players) {
            if (p->getStatus() != "BANKRUPT") { 
                state->events.processWin(*p); 
                break; 
            }
        }
        state->events.flushEventsTo(cout);
        state->game_over_by_bankruptcy = true;
        state->game_over = true;
        return;
    }

    if (state->max_turn < 1) return;
    if (state->current_turn > state->max_turn) {
        state->game_over = true;
    }
}

// ── start ─────────────────────────────────────────────────────────────────────
void GameLoop::start() {
    initDecks();
    cout << "\nGame dimulai! Max " << state->max_turn << " giliran.\n";

    LandingProcessor landing(*state);
    BankruptcyProcessor bankruptcy(*state, landing);
    RentCollector rentCollector(*state, bankruptcy);
    state->rentCollector = &rentCollector;
    landing.setRentCollector(&rentCollector);
    landing.setBankruptcyProcessor(&bankruptcy);
    CardProcessor cardProc(*state, bankruptcy);
    CommandHandler cmdHandler(*state, landing, bankruptcy, cardProc, rentCollector, *this);
    BotController botCtrl(*state, landing, bankruptcy, cardProc, rentCollector);

    while (!state->game_over) {
        checkWinCondition();
        if (state->game_over) break;

        Player *p = state->currentTurnPlayer();
        if (p->getStatus() == "BANKRUPT") { 
            nextTurn(); 
            continue; 
        }

        if (dynamic_cast<Bot*>(p)) {
            botCtrl.executeTurn(*p);
        } else {
            cmdHandler.executeTurn(*p);
        }

        checkWinCondition();
        if (!state->game_over) {
            nextTurn();
        }
    }

    vector<Player> copies;
    for (auto *p : state->players) {
        copies.push_back(*p);
    }
    state->formatter.printWin(copies, state->game_over_by_bankruptcy);
}

// ── run ───────────────────────────────────────────────────────────────────────
void GameLoop::run() {
    cout << "====================================\n";
    cout << "           NIMONSPOLI\n";
    cout << "====================================\n\n";

    string configDir = "config/";
    cout << "Path direktori konfigurasi: ";
    cin >> configDir;
    if (configDir.back() != '/') configDir += '/';

    GameConfig cfg = FileManager::loadGameConfig(configDir);
    int maxTurn = cfg.getMaxTurn();
    int initBalance = cfg.getInitialBalance();

    cout << "\n1. New Game\n2. Load Game\n";
    int choice = readInt("> ", 1, 2);

    if (choice == 1) {
        auto tiles = FileManager::loadBoard(configDir);
        auto [ps, order, activeId] = promptNewGame(initBalance);
        GameState *gs = new GameState(move(tiles), move(ps), move(order), cfg, maxTurn, 1, activeId);
        
        GameLoop loop;
        loop.state = gs;
        loop.start();
        // ~GameLoop() deletes state
    } else {
        string savePath;
        cout << "Path file save: ";
        cin >> savePath;
        if (savePath.find('/') == string::npos && savePath.find('\\') == string::npos) {
            savePath = "save/" + savePath;
        }

        try {
            auto sstate = FileManager::loadConfig(savePath);
            auto tiles = FileManager::loadBoard(configDir);
            auto [ps, order, activeId] = buildPlayersFromState(sstate);
            GameState *gs = new GameState(move(tiles), move(ps), move(order), cfg,
                                           sstate.max_turn, sstate.current_turn, activeId);
            
            GameLoop loop;
            loop.state = gs;
            loop.applyPropertyState(sstate);
            loop.start();
            // ~GameLoop() deletes state
        } catch (const SaveLoadException &e) {
            cout << "Gagal memuat: " << e.what() << "\n";
        } catch (const exception &e) {
            cout << "Error: " << e.what() << "\n";
        }
    }
}
// ── Destructor ────────────────────────────────────────────────────────────────
GameLoop::~GameLoop() {
    delete state;
    state = nullptr;
}

// ── getState ──────────────────────────────────────────────────────────────────
GameState* GameLoop::getState() const { return state; }

// ── advanceTurn ───────────────────────────────────────────────────────────────
void GameLoop::advanceTurn() { nextTurn(); }

// ── checkWinGui ───────────────────────────────────────────────────────────────
void GameLoop::checkWinGui() { checkWinCondition(); }

// ── buildForGui ───────────────────────────────────────────────────────────────
GameLoop* GameLoop::buildForGui(GameConfig& cfg, const std::string& configDir) {
    GameConfig ruleCfg = FileManager::loadGameConfig(configDir);
    // copy player info from GUI config into the rule config
    ruleCfg.playerCount = cfg.playerCount;
    for (int i = 0; i < cfg.playerCount; i++) {
        std::snprintf(ruleCfg.playerNames[i], GameConfig::MAX_NAME_LEN + 1,
                      "%s", cfg.playerNames[i]);
    }

    auto tiles = FileManager::loadBoard(configDir);

    vector<Player*> ps;
    for (int i = 0; i < cfg.playerCount; i++) {
        Player* p = new Player(string(cfg.playerNames[i]));
        p->operator+=(ruleCfg.getInitialBalance());
        ps.push_back(p);
    }

    vector<Player*> order = ps;
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    shuffle(order.begin(), order.end(), rng);

    int activeId = 0;
    for (int i = 0; i < (int)ps.size(); i++) {
        if (ps[i] == order[0]) { activeId = i; break; }
    }

    GameState* gs = new GameState(move(tiles), move(ps), move(order),
                                   ruleCfg, ruleCfg.getMaxTurn(), 1, activeId);

    GameLoop* loop = new GameLoop();
    loop->state = gs;
    loop->initDecks();
    loop->distributeSkillCards();

    return loop;
}

// ── saveToFile ────────────────────────────────────────────────────────────────
void GameLoop::saveToFile(const std::string& filepath) {
    if (!state) return;
    auto sstate = buildSaveState();
    GameStates::SavePermission perm;
    perm.is_at_start_of_turn = !state->game_over;
    FileManager::saveConfig(filepath, sstate, perm);
}

// ── buildFromSave ─────────────────────────────────────────────────────────────
GameLoop* GameLoop::buildFromSave(const std::string& filepath, const std::string& configDir) {
    auto sstate   = FileManager::loadConfig(filepath);
    GameConfig cfg = FileManager::loadGameConfig(configDir);
    auto tiles    = FileManager::loadBoard(configDir);
    auto [ps, order, activeId] = buildPlayersFromState(sstate);

    GameState* gs = new GameState(std::move(tiles), std::move(ps), std::move(order),
                                   cfg, sstate.max_turn, sstate.current_turn, activeId);
    GameLoop* loop = new GameLoop();
    loop->state = gs;
    loop->initDecks();
    loop->applyPropertyState(sstate);
    return loop;
}
