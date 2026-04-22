#include "Origami.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>

#include "include/models/player/Bot.hpp"
#include "include/models/tiles/PropertyTile.hpp"
#include "include/utils/exceptions/SaveLoadException.hpp"
#include "include/utils/file-manager/FileManager.hpp"

using namespace std;

// ─── I/O helpers ──────────────────────────────────────────────────────────────
namespace {

int readInt(const string &prompt, int lo, int hi) {
    int v;
    while (true) {
        cout << prompt;
        if (cin >> v && v >= lo && v <= hi) return v;
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
            if (valid.find(c) != string::npos) return c;
        }
        cout << "  Input tidak valid.\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

} // namespace

// ─── constructor / destructor ─────────────────────────────────────────────────
Origami::Origami(vector<Tile*> t, vector<Player*> p,
                 vector<Player*> order, int maxT, int currT, int activeId)
    : tiles(move(t)), board(tiles), players(move(p)),
      active_player_id(activeId), current_turn(currT), max_turn(maxT),
      turn_order(move(order)), transaction_log() {}

Origami::~Origami() {
    for (auto *t : tiles)   delete t;
    for (auto *p : players) delete p;
}

// ─── new game ─────────────────────────────────────────────────────────────────
tuple<vector<Player*>, vector<Player*>, int>
Origami::promptNewGame(int initBalance) {
    int count = readInt("Jumlah pemain (2-4): ", 2, 4);

    vector<Player*> players;
    for (int i = 0; i < count; i++) {
        cout << "\n--- Pemain " << (i + 1) << " ---\n";
        string name;
        cout << "Username: ";
        cin >> name;
        char type = readChar("Tipe (H=Human, B=Bot): ", "HB");
        Player *p = (type == 'B') ? static_cast<Player*>(new Bot(name))
                                  : new Player(name);
        p->addBalance(initBalance);
        players.push_back(p);
    }

    vector<Player*> order = players;
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    shuffle(order.begin(), order.end(), rng);

    cout << "\n=== Urutan Giliran ===\n";
    for (int i = 0; i < static_cast<int>(order.size()); i++)
        cout << (i + 1) << ". " << order[i]->getUsername() << "\n";

    int activeId = 0;
    for (int i = 0; i < static_cast<int>(players.size()); i++)
        if (players[i] == order[0]) { activeId = i; break; }

    return {players, order, activeId};
}

// ─── load game ────────────────────────────────────────────────────────────────
tuple<vector<Player*>, vector<Player*>, int>
Origami::buildPlayersFromState(const GameConfig::SaveState &state) {
    vector<Player*> players;
    map<string, Player*> byName;

    for (const auto &ps : state.players) {
        Player *p = new Player(ps.username);
        p->addBalance(ps.money);
        p->setStatus(ps.status);
        players.push_back(p);
        byName[ps.username] = p;
    }

    vector<Player*> order;
    for (const auto &name : state.turn_order) {
        auto it = byName.find(name);
        if (it != byName.end()) order.push_back(it->second);
    }

    int activeId = 0;
    auto activeIt = byName.find(state.active_turn_player);
    if (activeIt != byName.end())
        for (int i = 0; i < static_cast<int>(players.size()); i++)
            if (players[i] == activeIt->second) { activeId = i; break; }

    return {players, order, activeId};
}

void Origami::applyPropertyState(const GameConfig::SaveState &state) {
    map<string, Player*> byName;
    for (auto *p : players) byName[p->getUsername()] = p;

    for (const auto &ps : state.players) {
        auto it = byName.find(ps.username);
        if (it == byName.end()) continue;
        int idx = board.getTileIndex(ps.tile_code);
        if (idx >= 0) it->second->setCurrTile(idx);
    }

    for (const auto &ps : state.properties) {
        auto *prop = dynamic_cast<PropertyTile*>(board.getTileByCode(ps.tile_code));
        if (!prop) continue;
        prop->setMortgageStatus(ps.status == "MORTGAGED");
        prop->setFestivalState(ps.festival_multiplier, ps.festival_duration);
        prop->setLevel((ps.building_count == "H") ? 5 : stoi(ps.building_count));
        if (ps.owner_username != "BANK") {
            auto it = byName.find(ps.owner_username);
            if (it != byName.end()) it->second->addOwnedProperty(prop);
        }
    }

    current_turn = state.current_turn;
}

// ─── game loop (stub) ─────────────────────────────────────────────────────────
void Origami::start() {
    cout << "\nGame dimulai! (Turn " << current_turn << "/" << max_turn << ")\n";
    string cmd;
    while (true) {
        cout << "> ";
        if (!(cin >> cmd)) break;
        if (cmd == "QUIT" || cmd == "quit") break;
        // TODO: CETAK_PAPAN, LEMPAR_DADU, ATUR_DADU, dll.
    }
}

void Origami::nextTurn()             {}
void Origami::checkWinCondition()    {}
void Origami::distributeSkillCards() {}

// ─── entry point ─────────────────────────────────────────────────────────────
void Origami::run() {
    cout << "====================================\n";
    cout << "           NIMONSPOLI\n";
    cout << "====================================\n\n";

    // Custom config dir enables Papan Dinamis (board layout from board.txt)
    string configDir = "config/";
    char useCustom = readChar("Gunakan konfigurasi kustom? (Y/N): ", "YN");
    if (useCustom == 'Y') {
        cout << "Path direktori konfigurasi: ";
        cin >> configDir;
        if (configDir.back() != '/') configDir += '/';
    }

    int maxTurn, initBalance;
    FileManager::loadMiscConfig(configDir, maxTurn, initBalance);

    cout << "\n1. New Game\n2. Load Game\n";
    int choice = readInt("> ", 1, 2);

    if (choice == 1) {
        auto tiles = FileManager::loadBoard(configDir);
        auto [players, turnOrder, activeId] = promptNewGame(initBalance);
        Origami game(move(tiles), move(players), move(turnOrder),
                     maxTurn, 1, activeId);
        game.start();

    } else {
        string savePath;
        cout << "Path file save: ";
        cin >> savePath;
        try {
            auto state  = FileManager::loadConfig(savePath);
            auto tiles  = FileManager::loadBoard(configDir);
            auto [players, turnOrder, activeId] = buildPlayersFromState(state);
            Origami game(move(tiles), move(players), move(turnOrder),
                         state.max_turn, state.current_turn, activeId);
            game.applyPropertyState(state);
            game.start();
        } catch (const SaveLoadException &e) {
            cout << "Gagal memuat: " << e.what() << "\n";
        } catch (const exception &e) {
            cout << "Error: " << e.what() << "\n";
        }
    }
}
