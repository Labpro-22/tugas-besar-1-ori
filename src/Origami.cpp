#include "Origami.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>
#include <string>

#include "include/core/AuctionManager.hpp"
#include "include/core/BuyManager.hpp"
#include "include/core/CardManager.hpp"
#include "include/core/FestivalManager.hpp"
#include "include/core/JailManager.hpp"
#include "include/core/PropertyManager.hpp"
#include "include/core/SkillCardManager.hpp"
#include "include/core/TaxManager.hpp"
#include "include/models/card/ChanceCard.hpp"
#include "include/models/card/CommunityChest.hpp"
#include "include/models/card/SpecialPowerCard.hpp"
#include "include/models/player/Bot.hpp"
#include "include/models/tiles/PropertyTile.hpp"
#include "include/utils/exceptions/SaveLoadException.hpp"
#include "include/utils/file-manager/FileManager.hpp"

using namespace std;

// ── I/O helpers ───────────────────────────────────────────────────────────────
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

// ── constructor / destructor ──────────────────────────────────────────────────
Origami::Origami(vector<Tile*> t, vector<Player*> p,
                 vector<Player*> order, GameConfig cfg,
                 int maxT, int currT, int activeId)
    : tiles(move(t)), board(tiles), players(move(p)),
      active_player_id(activeId), current_turn(currT), max_turn(maxT),
      turn_order(move(order)), transaction_log(),
      config(cfg), game_over(false), turn_order_idx(0), jail_turns()
{
    if (!turn_order.empty() && active_player_id < static_cast<int>(players.size())) {
        Player *active = players[active_player_id];
        for (int i = 0; i < static_cast<int>(turn_order.size()); i++)
            if (turn_order[i] == active) { turn_order_idx = i; break; }
    }
}

Origami::~Origami() {
    for (auto *t : tiles)           delete t;
    for (auto *p : players)         delete p;
    for (auto *c : chance_cards)    delete c;
    for (auto *c : community_cards) delete c;
    for (auto *c : skill_cards)     delete c;
}

// ── initDecks ─────────────────────────────────────────────────────────────────
void Origami::initDecks() {
    int boardSize = board.getTileCount();

    int jailPos = 0;
    for (int i = 0; i < boardSize; i++)
        if (tiles[i]->getTileType() == "JAIL") { jailPos = i; break; }

    vector<int> railroadPos;
    for (int i = 0; i < boardSize; i++)
        if (tiles[i]->getTileType() == "RAILROAD") railroadPos.push_back(i);

    chance_cards = {
        new StepbackCard(boardSize),
        new NearestStreetCard(boardSize, railroadPos),
        new JailCard(jailPos)
    };
    for (auto *c : chance_cards) chance_deck.addCard(c);
    chance_deck.shuffle();

    community_cards = {
        new HappyBirthdayCard(),
        new SickCard(),
        new LegislativeCard()
    };
    for (auto *c : community_cards) community_deck.addCard(c);
    community_deck.shuffle();
}

// ── distributeSkillCards ──────────────────────────────────────────────────────
void Origami::distributeSkillCards() {
    int boardSize = board.getTileCount();
    skill_cards = {
        new MoveCard(3, boardSize),
        new MoveCard(5, boardSize),
        new DiscountCard(50),
        new ShieldCard(),
        new TeleportCard(),
        new LassoCard(),
        new DemolitionCard()
    };

    vector<SpecialPowerCard*> pool = skill_cards;
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    shuffle(pool.begin(), pool.end(), rng);

    for (int i = 0; i < static_cast<int>(players.size()) &&
                    i < static_cast<int>(pool.size()); i++)
        players[i]->addHandCard(pool[i]);
}

// ── helpers ───────────────────────────────────────────────────────────────────
void Origami::addLog(Player &p, const string &action, const string &detail) {
    transaction_log.emplace_back(current_turn, p.getUsername(), action, detail);
}

int Origami::activePlayerCount() const {
    int cnt = 0;
    for (auto *p : players)
        if (p->getStatus() != "BANKRUPT") cnt++;
    return cnt;
}

Player* Origami::getCreditorAt(int tileIdx) const {
    auto *prop = dynamic_cast<PropertyTile*>(tiles[tileIdx]);
    if (!prop) return nullptr;
    return prop->getTileOwner();
}

bool Origami::hasPendingAction(Player &p) const {
    int tileIdx = p.getCurrTile();
    Tile *t = tiles[tileIdx];
    string type = t->getTileType();

    // Cek properti yang belum dimiliki (harus beli atau lelang)
    if (type == "STREET") {
        auto *prop = dynamic_cast<PropertyTile*>(t);
        if (prop && !prop->getTileOwner() && !prop->isMortgage()) {
            return true;  // Harus beli atau lelang
        }
    }

    // Cek sewa yang harus dibayar
    if (type == "STREET" || type == "RAILROAD" || type == "UTILITY") {
        auto *prop = dynamic_cast<PropertyTile*>(t);
        if (prop && prop->getTileOwner() && prop->getTileOwner() != &p && !prop->isMortgage()) {
            return true;  // Harus bayar sewa
        }
    }

    // Cek pajak yang harus dibayar
    if (type == "TAX_PPH" || type == "TAX_PBM") {
        return true;  // Harus bayar pajak
    }

    return false;
}

// ── promptNewGame ─────────────────────────────────────────────────────────────
tuple<vector<Player*>, vector<Player*>, int>
Origami::promptNewGame(int initBalance) {
    int count = readInt("Jumlah pemain (2-4): ", 2, 4);

    vector<Player*> ps;
    for (int i = 0; i < count; i++) {
        cout << "\n--- Pemain " << (i + 1) << " ---\n";
        string name;
        cout << "Username: ";
        cin >> name;
        char type = readChar("Tipe (H=Human, B=Bot): ", "HB");
        Player *p = (type == 'B') ? static_cast<Player*>(new Bot(name))
                                  : new Player(name);
        p->addBalance(initBalance);
        ps.push_back(p);
    }

    vector<Player*> order = ps;
    mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    shuffle(order.begin(), order.end(), rng);

    cout << "\n=== Urutan Giliran ===\n";
    for (int i = 0; i < static_cast<int>(order.size()); i++)
        cout << (i + 1) << ". " << order[i]->getUsername() << "\n";

    int activeId = 0;
    for (int i = 0; i < static_cast<int>(ps.size()); i++)
        if (ps[i] == order[0]) { activeId = i; break; }

    return {ps, order, activeId};
}

// ── buildPlayersFromState ─────────────────────────────────────────────────────
tuple<vector<Player*>, vector<Player*>, int>
Origami::buildPlayersFromState(const GameStates::SaveState &state) {
    vector<Player*> ps;
    map<string, Player*> byName;

    for (const auto &s : state.players) {
        Player *p = new Player(s.username);
        p->addBalance(s.money);
        p->setStatus(s.status);
        ps.push_back(p);
        byName[s.username] = p;
    }

    vector<Player*> order;
    for (const auto &name : state.turn_order) {
        auto it = byName.find(name);
        if (it != byName.end()) order.push_back(it->second);
    }

    int activeId = 0;
    auto it = byName.find(state.active_turn_player);
    if (it != byName.end())
        for (int i = 0; i < static_cast<int>(ps.size()); i++)
            if (ps[i] == it->second) { activeId = i; break; }

    return {ps, order, activeId};
}

// ── applyPropertyState ────────────────────────────────────────────────────────
void Origami::applyPropertyState(const GameStates::SaveState &state) {
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
        int lvl = 0;
        if (ps.building_count == "H") lvl = 5;
        else if (!ps.building_count.empty() && isdigit(ps.building_count[0]))
            lvl = stoi(ps.building_count);
        prop->setLevel(lvl);
        if (ps.owner_username != "BANK") {
            auto it = byName.find(ps.owner_username);
            if (it != byName.end()) it->second->addOwnedProperty(prop);
        }
    }
    current_turn = state.current_turn;
}

// ── buildSaveState ────────────────────────────────────────────────────────────
GameStates::SaveState Origami::buildSaveState() const {
    GameStates::SaveState s;
    s.current_turn = current_turn;
    s.max_turn     = max_turn;

    for (auto *p : players) {
        GameStates::PlayerState ps;
        ps.username = p->getUsername();
        ps.money    = p->getBalance();
        ps.status   = p->getStatus();
        int tidx    = p->getCurrTile();
        if (tidx >= 0 && tidx < static_cast<int>(tiles.size()))
            ps.tile_code = tiles[tidx]->getTileCode();
        for (auto *c : p->getHandCards()) {
            GameStates::CardState cs;
            if      (dynamic_cast<MoveCard*>(c))        cs.type = "MOVE";
            else if (dynamic_cast<DiscountCard*>(c))    cs.type = "DISCOUNT";
            else if (dynamic_cast<ShieldCard*>(c))      cs.type = "SHIELD";
            else if (dynamic_cast<TeleportCard*>(c))    cs.type = "TELEPORT";
            else if (dynamic_cast<LassoCard*>(c))       cs.type = "LASSO";
            else if (dynamic_cast<DemolitionCard*>(c))  cs.type = "DEMOLITION";
            else                                         cs.type = "UNKNOWN";
            ps.hand_cards.push_back(cs);
        }
        s.players.push_back(ps);
    }

    for (auto *p : turn_order)
        s.turn_order.push_back(p->getUsername());

    if (active_player_id >= 0 && active_player_id < static_cast<int>(players.size()))
        s.active_turn_player = players[active_player_id]->getUsername();

    for (auto *t : tiles) {
        auto *prop = dynamic_cast<PropertyTile*>(t);
        if (!prop) continue;
        GameStates::PropertyState ps;
        ps.tile_code           = prop->getTileCode();
        ps.type                = prop->getTileType();
        ps.owner_username      = prop->getTileOwner()
                                 ? prop->getTileOwner()->getUsername() : "BANK";
        ps.status              = prop->isMortgage() ? "MORTGAGED" : "NORMAL";
        ps.festival_multiplier = prop->getFestivalMultiplier();
        ps.festival_duration   = prop->getFestivalDuration();
        int lvl                = prop->getLevel();
        ps.building_count      = (lvl == 5) ? "H" : to_string(lvl);
        s.properties.push_back(ps);
    }

    for (const auto &le : transaction_log) {
        GameStates::LogState ls;
        ls.turn        = le.getTurn();
        ls.username    = le.getUsername();
        ls.action_type = le.getActionType();
        ls.detail      = le.getDescription();
        s.logs.push_back(ls);
    }

    return s;
}

// ── rollAndMove ───────────────────────────────────────────────────────────────
bool Origami::rollAndMove(Player &p, bool manual, int d1, int d2) {
    int boardSize = board.getTileCount();
    if (manual) dice.setDice(d1, d2);
    else        dice.throwDice();

    int oldTile = p.getCurrTile();
    int newTile = (oldTile + dice.getTotal()) % boardSize;
    p.setCurrTile(newTile);

    cout << p.getUsername() << " melempar dadu: " << dice.getDie1() << " + " << dice.getDie2()
         << " = " << dice.getTotal()
         << (dice.isDouble() ? " (GANDA)" : "") << "\n";
    cout << "Pindah dari petak " << oldTile << " ke petak " << newTile << ".\n";

    applyGoSalary(p, oldTile);
    return dice.isDouble();
}

// ── applyGoSalary ─────────────────────────────────────────────────────────────
void Origami::applyGoSalary(Player &p, int oldTile) {
    int newTile = p.getCurrTile();
    if (newTile < oldTile) {
        p.addBalance(config.getGoSalary());
        cout << p.getUsername() << " melewati GO! Menerima gaji Rp"
             << config.getGoSalary() << ".\n";
        addLog(p, "GAJI_GO", "Rp" + to_string(config.getGoSalary()));
    }
}

// ── applyLanding ──────────────────────────────────────────────────────────────
void Origami::applyLanding(Player &p) {
    int     tileIdx = p.getCurrTile();
    Tile   *t       = tiles[tileIdx];
    string  type    = t->getTileType();

    cout << p.getUsername() << " mendarat di [" << t->getTileCode()
         << "] " << t->getTileName() << "\n";

    if (type == "GO_JAIL") {
        int jailIdx = 0;
        for (int i = 0; i < board.getTileCount(); i++)
            if (tiles[i]->getTileType() == "JAIL") { jailIdx = i; break; }
        JailManager::goToJail(p, jailIdx);
        jail_turns[&p] = 0;
        cout << p.getUsername() << " masuk penjara!\n";
        addLog(p, "MASUK_PENJARA", "");

    } else if (type == "FESTIVAL") {
        cout << "Anda dapat mengadakan festival! Gunakan: FESTIVAL <kode> <multiplier> <durasi>\n";

    } else if (type == "TAX_PPH") {
        int amt = config.getPphFlat() +
                  static_cast<int>(p.getBalance() * config.getPphPercentage() / 100.0);
        cout << "Pajak PPH: Rp" << amt << ". Gunakan BAYAR_PAJAK.\n";

    } else if (type == "TAX_PBM") {
        cout << "Pajak PBM: Rp" << config.getPbmFlat() << ". Gunakan BAYAR_PAJAK.\n";

    } else if (type == "CHANCE") {
        ChanceCard *card = chance_deck.draw();
        if (card) {
            cout << "--- Kartu Kesempatan! ---\n";
            int before = p.getCurrTile();
            CardManager::resolveCardEffect(*card, p, players);
            chance_deck.discardCard(card);
            if (p.getStatus() == "JAIL" && jail_turns.find(&p) == jail_turns.end())
                jail_turns[&p] = 0;
            if (p.getCurrTile() != before && p.getStatus() != "JAIL")
                applyLanding(p);
            addLog(p, "KARTU_KESEMPATAN", "");
        }

    } else if (type == "COMMUNITY_CHEST") {
        CommunityChestCard *card = community_deck.draw();
        if (card) {
            cout << "--- Kartu Dana Umum! ---\n";
            CardManager::resolveCardEffect(*card, p, players);
            community_deck.discardCard(card);
            addLog(p, "KARTU_DANA_UMUM", "");
        }

    } else if (type == "STREET" || type == "RAILROAD" || type == "UTILITY") {
        auto *prop = dynamic_cast<PropertyTile*>(t);
        if (!prop) return;
        if (prop->isMortgage()) {
            cout << "Properti ini sedang digadaikan.\n";
            return;
        }
        Player *owner = prop->getTileOwner();
        if (!owner) {
            cout << "Properti belum dimiliki. Gunakan BELI atau LELANG.\n";
        } else if (owner == &p) {
            cout << "Ini properti Anda sendiri.\n";
        } else {
            if (p.isShieldActive()) {
                cout << "Perisai aktif! Tidak perlu membayar sewa.\n";
            } else {
                int rent = prop->calculateRent();
                cout << "Anda harus membayar sewa Rp" << rent
                     << " ke " << owner->getUsername() << ". Gunakan BAYAR_SEWA.\n";
            }
        }
    }
    // START, JAIL, FREE_PARKING → no special effect
}

// ── commands ──────────────────────────────────────────────────────────────────
void Origami::cmdCetakPapan() {
    formatter.printBoard(board, players, current_turn, max_turn);
}

void Origami::cmdCetakAkta(const string &code) {
    Tile *t = board.getTileByCode(code);
    if (!t) { cout << "Kode properti tidak ditemukan.\n"; return; }
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop) { cout << "Bukan properti.\n"; return; }
    formatter.printAkta(*prop, board);
}

void Origami::cmdCetakProperti(Player &p) {
    formatter.printProperty(p, board);
}

void Origami::cmdBeli(Player &p) {
    Tile *t = tiles[p.getCurrTile()];
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop || prop->getTileOwner()) {
        cout << "Tidak bisa membeli properti ini.\n"; return;
    }
    if (BuyManager::buy(p, *t)) {
        cout << p.getUsername() << " membeli " << t->getTileName() << ".\n";
        addLog(p, "BELI", t->getTileCode());
    } else {
        cout << "Pembelian gagal (saldo tidak cukup).\n";
    }
}

void Origami::cmdBayarSewa(Player &p) {
    int tileIdx   = p.getCurrTile();
    Tile *t       = tiles[tileIdx];
    Player *owner = getCreditorAt(tileIdx);
    if (!owner || owner == &p) {
        cout << "Tidak ada sewa yang harus dibayar di sini.\n"; return;
    }
    if (p.isShieldActive()) {
        cout << "Perisai aktif! Sewa tidak perlu dibayar.\n"; return;
    }
    if (PropertyManager::compensate(p, *t)) {
        cout << p.getUsername() << " membayar sewa ke " << owner->getUsername() << ".\n";
        addLog(p, "BAYAR_SEWA", t->getTileCode());
    } else {
        cout << "Saldo tidak cukup.\n";
    }
}

void Origami::cmdBayarPajak(Player &p) {
    string type = tiles[p.getCurrTile()]->getTileType();
    int amt = 0;
    if (type == "TAX_PPH")
        amt = config.getPphFlat() +
              static_cast<int>(p.getBalance() * config.getPphPercentage() / 100.0);
    else if (type == "TAX_PBM")
        amt = config.getPbmFlat();
    else { cout << "Bukan petak pajak.\n"; return; }

    if (TaxManager::payTax(p, amt)) {
        cout << p.getUsername() << " membayar pajak Rp" << amt << ".\n";
        addLog(p, "BAYAR_PAJAK", to_string(amt));
    } else {
        cout << "Saldo tidak cukup.\n";
    }
}

void Origami::cmdGadai(Player &p, const string &code) {
    Tile *t = board.getTileByCode(code);
    if (!t) { cout << "Kode tidak ditemukan.\n"; return; }
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop || prop->getTileOwner() != &p) {
        cout << "Bukan properti Anda.\n"; return;
    }
    if (PropertyManager::mortgage(p, *prop)) {
        cout << "Properti " << code << " berhasil digadaikan.\n";
        addLog(p, "GADAI", code);
    } else {
        cout << "Gagal menggadaikan.\n";
    }
}

void Origami::cmdTebus(Player &p, const string &code) {
    Tile *t = board.getTileByCode(code);
    if (!t) { cout << "Kode tidak ditemukan.\n"; return; }
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop || prop->getTileOwner() != &p || !prop->isMortgage()) {
        cout << "Properti tidak dapat ditebus.\n"; return;
    }
    int cost = prop->getMortgageValue();
    if (p.getBalance() < cost) {
        cout << "Saldo tidak cukup (perlu Rp" << cost << ").\n"; return;
    }
    p.addBalance(-cost);
    prop->setMortgageStatus(false);
    cout << "Properti " << code << " ditebus seharga Rp" << cost << ".\n";
    addLog(p, "TEBUS", code);
}

void Origami::cmdBangun(Player &p, const string &code) {
    // Helper untuk padding string
    auto padRight = [](const string &str, int width) -> string {
        if ((int)str.length() >= width) return str.substr(0, width);
        return str + string(width - str.length(), ' ');
    };

    // Helper untuk cek apakah pemain memonopoli color group
    auto hasMonopoly = [&](const string &colorGroup) -> bool {
        vector<PropertyTile*> groupProps = board.getPropertiesByColorGroup(colorGroup);
        if (groupProps.empty()) return false;
        for (auto *gp : groupProps) {
            if (gp->getTileOwner() != &p) return false;
        }
        return true;
    };

    // Helper untuk mendapatkan level minimum di color group
    auto getMinLevelInGroup = [&](const string &colorGroup) -> int {
        vector<PropertyTile*> groupProps = board.getPropertiesByColorGroup(colorGroup);
        if (groupProps.empty()) return 0;
        int minLvl = groupProps[0]->getLevel();
        for (auto *gp : groupProps) {
            if (gp->getLevel() < minLvl) minLvl = gp->getLevel();
        }
        return minLvl;
    };

    // Helper untuk cek apakah semua petak di group sudah level 4 (siap upgrade ke hotel)
    auto allReadyForHotel = [&](const string &colorGroup) -> bool {
        vector<PropertyTile*> groupProps = board.getPropertiesByColorGroup(colorGroup);
        if (groupProps.empty()) return false;
        for (auto *gp : groupProps) {
            if (gp->getLevel() < 4) return false;
        }
        return true;
    };

    // Jika code kosong, tampilkan menu lengkap
    if (code.empty()) {
        // Kumpulkan color group yang memenuhi syarat (monopoli)
        map<string, vector<PropertyTile*>> eligibleGroups;
        vector<PropertyTile*> allProps = p.getOwnedProperties();

        for (auto *prop : allProps) {
            // Hanya STREET yang bisa dibangun
            if (prop->getTileType() != "STREET") continue;
            string colorGroup = prop->getColorGroup();
            if (colorGroup.empty()) continue;
            if (!hasMonopoly(colorGroup)) continue;

            // Cek apakah bisa dibangun (belum hotel dan memenuhi pemerataan)
            int currentLevel = prop->getLevel();
            if (currentLevel >= 5) continue; // Sudah hotel

            int minLevelInGroup = getMinLevelInGroup(colorGroup);

            // Bisa bangun jika level saat ini == min level di group (pemerataan)
            // Atau jika semua sudah level 4 (upgrade ke hotel)
            bool canBuild = false;
            if (currentLevel < 4) {
                // Bangun rumah
                canBuild = (currentLevel == minLevelInGroup);
            } else if (currentLevel == 4) {
                // Upgrade ke hotel
                canBuild = allReadyForHotel(colorGroup);
            }

            if (canBuild) {
                eligibleGroups[colorGroup].push_back(prop);
            }
        }

        if (eligibleGroups.empty()) {
            cout << "Tidak ada color group yang memenuhi syarat untuk dibangun.\n";
            cout << "Kamu harus memiliki seluruh petak dalam satu color group terlebih dahulu,\n";
            cout << "atau masih ada bangunan yang tidak merata di color group tersebut.\n";
            return;
        }

        // Tampilkan color group yang memenuhi syarat
        cout << "\n=== Color Group yang Memenuhi Syarat ===\n";
        vector<string> groupNames;
        int idx = 1;
        for (auto &[groupName, props] : eligibleGroups) {
            groupNames.push_back(groupName);
            cout << idx << ". [" << groupName << "]\n";
            for (auto *prop : props) {
                int lvl = prop->getLevel();
                string lvlStr;
                if (lvl == 0) lvlStr = "0 rumah";
                else if (lvl == 1) lvlStr = "1 rumah";
                else if (lvl == 2) lvlStr = "2 rumah";
                else if (lvl == 3) lvlStr = "3 rumah";
                else if (lvl == 4) lvlStr = "4 rumah";
                else if (lvl == 5) lvlStr = "Hotel";

                int cost = (lvl >= 4) ? prop->getHotelCost() : prop->getHouseCost();
                cout << "   - " << padRight(prop->getTileName() + " (" + prop->getTileCode() + ")", 25)
                     << " : " << padRight(lvlStr, 12) << " (Harga: M" << cost << ")\n";
            }
            idx++;
        }

        cout << "\nUang kamu saat ini: M" << p.getBalance() << "\n";
        int choice = readInt("Pilih nomor color group (0 untuk batal): ", 0, static_cast<int>(groupNames.size()));
        if (choice == 0) return;

        string selectedGroup = groupNames[choice - 1];
        vector<PropertyTile*> &groupProps = eligibleGroups[selectedGroup];

        // Cek apakah semua sudah 4 rumah (siap upgrade ke hotel)
        bool readyForHotel = allReadyForHotel(selectedGroup);

        // Tampilkan petak yang bisa dibangun dalam color group
        cout << "\nColor group [" << selectedGroup << "]:\n";
        idx = 1;
        vector<PropertyTile*> buildableProps;
        for (auto *prop : groupProps) {
            int lvl = prop->getLevel();
            if (lvl >= 5) continue; // Skip yang sudah hotel

            string lvlStr;
            string marker;
            if (lvl == 0) lvlStr = "0 rumah";
            else if (lvl == 1) lvlStr = "1 rumah";
            else if (lvl == 2) lvlStr = "2 rumah";
            else if (lvl == 3) lvlStr = "3 rumah";
            else if (lvl == 4) { lvlStr = "4 rumah"; marker = " <- siap upgrade ke hotel"; }

            int cost = (lvl >= 4) ? prop->getHotelCost() : prop->getHouseCost();
            cout << idx << ". " << padRight(prop->getTileName() + " (" + prop->getTileCode() + ")", 25)
                 << " : " << lvlStr;
            if (!marker.empty()) cout << marker;
            cout << "\n";
            buildableProps.push_back(prop);
            idx++;
        }

        if (buildableProps.empty()) {
            cout << "Tidak ada petak yang dapat dibangun dalam color group ini.\n";
            return;
        }

        int propChoice = readInt("Pilih petak (0 untuk batal): ", 0, static_cast<int>(buildableProps.size()));
        if (propChoice == 0) return;

        PropertyTile *selectedProp = buildableProps[propChoice - 1];
        int currentLvl = selectedProp->getLevel();
        int cost = (currentLvl >= 4) ? selectedProp->getHotelCost() : selectedProp->getHouseCost();

        if (currentLvl >= 4) {
            // Upgrade ke hotel
            cout << "Upgrade ke hotel? Biaya: M" << cost << " (y/n): ";
            char confirm = readChar("", "YN");
            if (confirm == 'N') {
                cout << "Pembangunan dibatalkan.\n";
                return;
            }
        }

        // Lakukan pembangunan
        if (p.getBalance() < cost) {
            cout << "Saldo tidak cukup (perlu M" << cost << ", uang kamu: M" << p.getBalance() << ").\n";
            return;
        }

        p.addBalance(-cost);
        selectedProp->setLevel(currentLvl + 1);

        string buildType = (currentLvl >= 4) ? "Hotel" : "1 rumah";
        cout << "Kamu membangun " << buildType << " di " << selectedProp->getTileName()
             << ". Biaya: M" << cost << "\n";
        cout << "Uang tersisa: M" << p.getBalance() << "\n";

        // Tampilkan status terbaru
        cout << "\nStatus terbaru [" << selectedGroup << "]:\n";
        vector<PropertyTile*> allGroupProps = board.getPropertiesByColorGroup(selectedGroup);
        for (auto *gp : allGroupProps) {
            int lvl = gp->getLevel();
            string lvlStr;
            if (lvl == 0) lvlStr = "0 rumah";
            else if (lvl == 1) lvlStr = "1 rumah";
            else if (lvl == 2) lvlStr = "2 rumah";
            else if (lvl == 3) lvlStr = "3 rumah";
            else if (lvl == 4) lvlStr = "4 rumah";
            else if (lvl == 5) lvlStr = "Hotel";
            cout << "  - " << padRight(gp->getTileName() + " (" + gp->getTileCode() + ")", 25) << " : " << lvlStr << "\n";
        }

        addLog(p, "BANGUN", selectedProp->getTileCode());
        return;
    }

    // Jika code tidak kosong, gunakan perilaku lama (bangun langsung pada kode tertentu)
    // Tapi tetap dengan validasi lengkap
    Tile *t = board.getTileByCode(code);
    if (!t) { cout << "Kode tidak ditemukan.\n"; return; }
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop || prop->getTileOwner() != &p) {
        cout << "Bukan properti Anda.\n"; return;
    }
    // Hanya STREET yang bisa dibangun
    if (prop->getTileType() != "STREET") {
        cout << "Hanya properti STREET yang dapat dibangun. Stasiun dan Utilitas tidak dapat dibangun.\n";
        return;
    }
    string colorGroup = prop->getColorGroup();
    if (!hasMonopoly(colorGroup)) {
        cout << "Anda belum memonopoli color group [" << colorGroup << "].\n";
        cout << "Kamu harus memiliki seluruh petak dalam color group terlebih dahulu.\n";
        return;
    }
    if (prop->getLevel() >= 5) {
        cout << "Properti ini sudah mencapai level maksimal (Hotel).\n";
        return;
    }

    // Cek pemerataan
    int currentLvl = prop->getLevel();
    int minLvl = getMinLevelInGroup(colorGroup);
    if (currentLvl > minLvl) {
        cout << "Tidak dapat membangun! Bangunan harus dibangun secara merata.\n";
        cout << "Ada petak lain di color group [" << colorGroup << "] yang masih level " << minLvl << ".\n";
        return;
    }

    // Cek jika upgrade ke hotel, semua harus sudah 4 rumah
    if (currentLvl == 4 && !allReadyForHotel(colorGroup)) {
        cout << "Tidak dapat upgrade ke hotel! Semua petak di color group harus memiliki 4 rumah terlebih dahulu.\n";
        return;
    }

    int cost = (currentLvl >= 4) ? prop->getHotelCost() : prop->getHouseCost();
    if (p.getBalance() < cost) {
        cout << "Saldo tidak cukup (perlu M" << cost << ", uang kamu: M" << p.getBalance() << ").\n";
        return;
    }

    p.addBalance(-cost);
    prop->setLevel(currentLvl + 1);
    string buildType = (currentLvl >= 4) ? "Hotel" : "1 rumah";
    cout << "Kamu membangun " << buildType << " di " << prop->getTileName() << ".\n";
    cout << "Biaya: M" << cost << ", Uang tersisa: M" << p.getBalance() << "\n";
    addLog(p, "BANGUN", code);
}

void Origami::cmdLelang(Player &p, const string &code) {
    Tile *t = board.getTileByCode(code);
    if (!t) { cout << "Kode tidak ditemukan.\n"; return; }
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop) { cout << "Bukan properti.\n"; return; }

    formatter.printAuction();
    vector<Player*> participants;
    for (auto *pl : turn_order)
        if (pl->getStatus() != "BANKRUPT") participants.push_back(pl);

    auto result = AuctionManager::auctionProperty(p, *prop, participants);
    if (result.sold && result.winner) {
        cout << result.winner->getUsername() << " memenangkan lelang seharga Rp"
             << result.final_bid << ".\n";
        addLog(p, "LELANG", code + " -> " + result.winner->getUsername());
    } else {
        cout << "Lelang tidak terjual.\n";
    }
}

void Origami::cmdFestival(Player &p, const string &code, int mult, int dur) {
    if (tiles[p.getCurrTile()]->getTileType() != "FESTIVAL") {
        cout << "Hanya bisa di petak Festival.\n"; return;
    }
    Tile *t = board.getTileByCode(code);
    if (!t) { cout << "Kode tidak ditemukan.\n"; return; }
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop || prop->getTileOwner() != &p) {
        cout << "Bukan properti Anda.\n"; return;
    }
    FestivalManager::festival(*prop, mult, dur);
    cout << "Festival aktif di " << code << " (x" << mult
         << " selama " << dur << " giliran).\n";
    addLog(p, "FESTIVAL", code);
}

void Origami::cmdBangkrut(Player &p) {
    Player *creditor = getCreditorAt(p.getCurrTile());
    vector<Player*> others;
    for (auto *pl : players)
        if (pl != &p && pl->getStatus() != "BANKRUPT") others.push_back(pl);
    if (creditor)
        events.processBankruptcy(p, creditor);
    else
        events.processBankruptcy(p, others);
    cout << p.getUsername() << " bangkrut!\n";
    addLog(p, "BANGKRUT", "");
}

void Origami::cmdSimpan(const string &path, bool hasRolled) {
    try {
        GameStates::SavePermission perm;
        perm.has_rolled_dice     = hasRolled;
        perm.is_at_start_of_turn = !hasRolled;
        FileManager::saveConfig(path, buildSaveState(), perm);
        cout << "Game disimpan ke " << path << ".\n";
    } catch (const exception &e) {
        cout << "Gagal menyimpan: " << e.what() << "\n";
    }
}

void Origami::cmdCetakLog() {
    formatter.printLog(transaction_log);
}

void Origami::cmdBayarDenda(Player &p) {
    if (p.getStatus() != "JAIL") {
        cout << "Anda tidak dalam penjara.\n"; return;
    }
    int fine = config.getJailFine();
    if (p.getBalance() < fine) {
        cout << "Saldo tidak cukup (perlu Rp" << fine << ").\n"; return;
    }
    p.addBalance(-fine);
    p.setStatus("ACTIVE");
    jail_turns.erase(&p);
    cout << p.getUsername() << " membayar denda Rp" << fine
         << " dan bebas dari penjara. Lempar dadu di giliran berikutnya.\n";
    addLog(p, "BAYAR_DENDA", to_string(fine));
}

void Origami::cmdGunakanKemampuan(Player &p, int index) {
    SpecialPowerCard *card = p.getHandCard(index);
    if (!card) { cout << "Kartu tidak ditemukan.\n"; return; }
    if (!SkillCardManager::useSkillCard(p)) {
        cout << "Tidak bisa menggunakan kemampuan sekarang.\n"; return;
    }
    int before = p.getCurrTile();
    card->action(p);
    p.removeHandCard(index);
    if (p.getCurrTile() != before)
        applyLanding(p);
    addLog(p, "GUNAKAN_KEMAMPUAN", to_string(index + 1));
}

void Origami::cmdDropKemampuan(Player &p, int index) {
    if (!p.getHandCard(index)) { cout << "Kartu tidak ditemukan.\n"; return; }
    SkillCardManager::dropSkillCard(p);
    p.removeHandCard(index);
    cout << "Kartu dibuang.\n";
    addLog(p, "DROP_KEMAMPUAN", to_string(index + 1));
}

// ── humanTurn ─────────────────────────────────────────────────────────────────
void Origami::humanTurn(Player &p) {
    bool has_rolled          = false;
    bool turn_ended          = false;
    bool paid_fine_this_turn = false;
    bool has_bonus_turn      = false;  // Track if player has bonus turn from double
    int  consec_doubles      = 0;

    cout << "\n=== Giliran " << p.getUsername()
         << " (Turn " << current_turn << "/" << max_turn << ") ===\n";
    formatter.printPlayerStatus(p);

    while (!turn_ended && !game_over) {
        string cmd;
        cout << "> ";
        if (!(cin >> cmd)) { game_over = true; break; }
        for (auto &c : cmd) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));

        // jail-only pre-roll: pay fine (ends turn, no roll this turn)
        if (cmd == "BAYAR_DENDA") {
            if (p.getStatus() == "JAIL" && !has_rolled) {
                cmdBayarDenda(p);
                paid_fine_this_turn = true;
                turn_ended = true;
            } else {
                cout << "BAYAR_DENDA hanya bisa dilakukan di awal giliran dalam penjara.\n";
            }
            continue;
        }

        // universal (anytime) commands
        if (cmd == "CETAK_PAPAN") {
            cmdCetakPapan();
        } else if (cmd == "CETAK_AKTA") {
            string code; cin >> code;
            cmdCetakAkta(code);
        } else if (cmd == "CETAK_PROPERTI") {
            cmdCetakProperti(p);
        } else if (cmd == "CETAK_LOG") {
            cmdCetakLog();
        } else if (cmd == "SIMPAN") {
            string path; cin >> path;
            cmdSimpan(path, has_rolled);
        } else if (cmd == "GADAI") {
            string code; cin >> code;
            cmdGadai(p, code);
        } else if (cmd == "TEBUS") {
            string code; cin >> code;
            cmdTebus(p, code);
        } else if (cmd == "BANGUN") {
            // BANGUN tanpa argumen - tampilkan menu lengkap
            cmdBangun(p, "");
        } else if (cmd == "GUNAKAN_KEMAMPUAN") {
            int idx; cin >> idx;
            cmdGunakanKemampuan(p, idx - 1);
        } else if (cmd == "DROP_KEMAMPUAN") {
            int idx; cin >> idx;
            cmdDropKemampuan(p, idx - 1);

        // roll commands (pre-roll only)
        } else if ((cmd == "LEMPAR_DADU" || cmd == "ATUR_DADU") && !has_rolled) {
            bool manual = (cmd == "ATUR_DADU");
            int d1 = 0, d2 = 0;
            if (manual) cin >> d1 >> d2;

            if (p.getStatus() == "JAIL") {
                bool isDouble = rollAndMove(p, manual, d1, d2);
                if (isDouble) {
                    p.setStatus("ACTIVE");
                    jail_turns.erase(&p);
                    cout << "Dadu ganda! Bebas dari penjara.\n";
                    applyLanding(p);
                    has_rolled = true;
                } else {
                    auto it = jail_turns.find(&p);
                    if (it != jail_turns.end()) {
                        it->second++;
                        if (it->second >= 3) {
                            int fine = config.getJailFine();
                            p.addBalance(-fine);
                            p.setStatus("ACTIVE");
                            jail_turns.erase(&p);
                            cout << "3 giliran di penjara. Denda otomatis Rp" << fine << ".\n";
                            // still move with the dice already thrown
                            applyLanding(p);
                            has_rolled = true;
                        }
                    }
                    if (p.getStatus() == "JAIL") turn_ended = true;
                }
            } else {
                bool isDouble = rollAndMove(p, manual, d1, d2);
                applyLanding(p);
                has_rolled = true;
                if (isDouble) {
                    consec_doubles++;
                    if (consec_doubles >= 3) {
                        int jailIdx = 0;
                        for (int i = 0; i < board.getTileCount(); i++)
                            if (tiles[i]->getTileType() == "JAIL") { jailIdx = i; break; }
                        JailManager::goToJail(p, jailIdx);
                        jail_turns[&p] = 0;
                        cout << "3 dadu ganda berturut-turut! Masuk penjara!\n";
                        addLog(p, "MASUK_PENJARA", "3x ganda");
                        turn_ended = true;
                    } else {
                        cout << "Dadu ganda! Anda mendapat giliran tambahan setelah selesai.\n";
                        has_bonus_turn = true;  // Mark that we have bonus turn pending
                    }
                }
            }

        // post-roll commands
        } else if (cmd == "BELI") {
            if (!has_rolled) cout << "Lempar dadu terlebih dahulu.\n";
            else cmdBeli(p);
        } else if (cmd == "BAYAR_SEWA") {
            if (!has_rolled) cout << "Lempar dadu terlebih dahulu.\n";
            else cmdBayarSewa(p);
        } else if (cmd == "BAYAR_PAJAK") {
            if (!has_rolled) cout << "Lempar dadu terlebih dahulu.\n";
            else cmdBayarPajak(p);
        } else if (cmd == "LELANG") {
            if (!has_rolled) cout << "Lempar dadu terlebih dahulu.\n";
            else { string code; cin >> code; cmdLelang(p, code); }
        } else if (cmd == "FESTIVAL") {
            if (!has_rolled) cout << "Lempar dadu terlebih dahulu.\n";
            else {
                string code; int mult, dur;
                cin >> code >> mult >> dur;
                cmdFestival(p, code, mult, dur);
            }
        } else if (cmd == "BANGKRUT") {
            cmdBangkrut(p);
            turn_ended = true;

        } else if (cmd == "SELESAI") {
            if (!has_rolled && p.getStatus() != "JAIL" && !paid_fine_this_turn) {
                cout << "Harus melempar dadu sebelum selesai.\n";
            } else if (has_rolled && hasPendingAction(p)) {
                cout << "Anda masih memiliki kewajiban yang harus diselesaikan (beli/bayar sewa/bayar pajak/lelang).\n";
            } else {
                turn_ended = true;
                // Handle bonus turn from double dice
                if (has_bonus_turn && p.getStatus() != "JAIL" && p.getStatus() != "BANKRUPT") {
                    cout << "\n>>> Giliran tambahan dari dadu ganda! <<<\n";
                    has_rolled = false;
                    has_bonus_turn = false;
                    turn_ended = false;
                    cout << "\n=== Giliran Tambahan " << p.getUsername() << " ===\n";
                    formatter.printPlayerStatus(p);
                }
            }
        } else {
            cout << "Perintah tidak dikenal: " << cmd << "\n";
        }
    }
}

// ── botTurn ───────────────────────────────────────────────────────────────────
void Origami::botTurn(Player &p) {
    cout << "\n=== Giliran Bot " << p.getUsername()
         << " (Turn " << current_turn << "/" << max_turn << ") ===\n";

    // jail handling: auto-pay fine and end turn (roll next turn)
    if (p.getStatus() == "JAIL") {
        int fine = config.getJailFine();
        if (p.getBalance() >= fine) {
            p.addBalance(-fine);
            p.setStatus("ACTIVE");
            jail_turns.erase(&p);
            cout << p.getUsername() << " (bot) membayar denda Rp" << fine << ". Giliran selesai.\n";
            addLog(p, "BAYAR_DENDA", to_string(fine));
        } else {
            // try rolling doubles
            bool isDouble = rollAndMove(p, false);
            if (!isDouble) {
                auto it = jail_turns.find(&p);
                if (it != jail_turns.end()) it->second++;
                cout << p.getUsername() << " (bot) gagal keluar penjara.\n";
            } else {
                p.setStatus("ACTIVE");
                jail_turns.erase(&p);
                cout << p.getUsername() << " (bot) keluar penjara (dadu ganda).\n";
                applyLanding(p);
            }
        }
        return;
    }

    bool isDouble = rollAndMove(p, false);
    applyLanding(p);

    if (p.getStatus() == "BANKRUPT") return;

    // auto-buy: buy if balance >= 2x buy price
    auto *prop = dynamic_cast<PropertyTile*>(tiles[p.getCurrTile()]);
    if (prop && !prop->getTileOwner() && !prop->isMortgage()) {
        if (p.getBalance() >= prop->getBuyPrice() * 2) {
            if (BuyManager::buy(p, *tiles[p.getCurrTile()])) {
                cout << p.getUsername() << " (bot) membeli " << prop->getTileName() << ".\n";
                addLog(p, "BELI", prop->getTileCode());
            }
        }
    }

    // auto-pay rent
    Player *owner = getCreditorAt(p.getCurrTile());
    if (owner && owner != &p && !p.isShieldActive()) {
        if (!PropertyManager::compensate(p, *tiles[p.getCurrTile()])) {
            cmdBangkrut(p);
            return;
        }
        cout << p.getUsername() << " (bot) membayar sewa ke " << owner->getUsername() << ".\n";
        addLog(p, "BAYAR_SEWA", tiles[p.getCurrTile()]->getTileCode());
    }

    // auto-pay tax
    string ttype = tiles[p.getCurrTile()]->getTileType();
    if (ttype == "TAX_PPH" || ttype == "TAX_PBM") {
        int amt = (ttype == "TAX_PPH")
            ? config.getPphFlat() + static_cast<int>(p.getBalance() * config.getPphPercentage() / 100.0)
            : config.getPbmFlat();
        if (TaxManager::payTax(p, amt)) {
            cout << p.getUsername() << " (bot) membayar pajak Rp" << amt << ".\n";
            addLog(p, "BAYAR_PAJAK", to_string(amt));
        }
    }

    // bonus turn on double
    if (isDouble && p.getStatus() != "JAIL" && p.getStatus() != "BANKRUPT") {
        cout << p.getUsername() << " (bot) mendapat giliran tambahan!\n";
        botTurn(p);
    }
}

// ── nextTurn ──────────────────────────────────────────────────────────────────
void Origami::nextTurn() {
    // decrement festival durations
    for (auto *t : tiles) {
        auto *prop = dynamic_cast<PropertyTile*>(t);
        if (!prop || prop->getFestivalDuration() <= 0) continue;
        prop->setFestivalState(prop->getFestivalMultiplier(),
                               prop->getFestivalDuration() - 1);
    }

    // clear per-turn modifiers for current player
    if (!turn_order.empty())
        turn_order[turn_order_idx]->clearTurnModifiers();

    // advance idx, skip BANKRUPT players
    int attempts = 0;
    int sz = static_cast<int>(turn_order.size());
    do {
        turn_order_idx = (turn_order_idx + 1) % sz;
        attempts++;
    } while (turn_order[turn_order_idx]->getStatus() == "BANKRUPT" && attempts <= sz);

    // full lap → increment turn counter
    if (turn_order_idx == 0) current_turn++;

    // update active_player_id
    Player *next = turn_order[turn_order_idx];
    for (int i = 0; i < static_cast<int>(players.size()); i++)
        if (players[i] == next) { active_player_id = i; break; }
}

// ── checkWinCondition ─────────────────────────────────────────────────────────
void Origami::checkWinCondition() {
    if (activePlayerCount() <= 1) {
        for (auto *p : players)
            if (p->getStatus() != "BANKRUPT") { events.processWin(*p); break; }
        game_over = true;
        return;
    }
    if (current_turn > max_turn)
        game_over = true;
}

// ── start ─────────────────────────────────────────────────────────────────────
void Origami::start() {
    initDecks();
    distributeSkillCards();
    cout << "\nGame dimulai! Max " << max_turn << " giliran.\n";

    while (!game_over) {
        checkWinCondition();
        if (game_over) break;

        Player *p = turn_order[turn_order_idx];
        if (p->getStatus() == "BANKRUPT") { nextTurn(); continue; }

        if (dynamic_cast<Bot*>(p)) botTurn(*p);
        else                       humanTurn(*p);

        checkWinCondition();
        if (!game_over) nextTurn();
    }

    vector<Player> copies;
    for (auto *p : players) copies.push_back(*p);
    formatter.printWin(copies);
}

// ── run ───────────────────────────────────────────────────────────────────────
void Origami::run() {
    cout << "====================================\n";
    cout << "           NIMONSPOLI\n";
    cout << "====================================\n\n";

    string configDir = "config/";
    cout << "Path direktori konfigurasi: ";
    cin >> configDir;
    if (configDir.back() != '/') configDir += '/';

    GameConfig cfg    = FileManager::loadGameConfig(configDir);
    int maxTurn       = cfg.getMaxTurn();
    int initBalance   = cfg.getInitialBalance();

    cout << "\n1. New Game\n2. Load Game\n";
    int choice = readInt("> ", 1, 2);

    if (choice == 1) {
        auto tiles = FileManager::loadBoard(configDir);
        auto [ps, order, activeId] = promptNewGame(initBalance);
        Origami game(move(tiles), move(ps), move(order), cfg, maxTurn, 1, activeId);
        game.start();
    } else {
        string savePath;
        cout << "Path file save: ";
        cin >> savePath;
        try {
            auto state = FileManager::loadConfig(savePath);
            auto tiles = FileManager::loadBoard(configDir);
            auto [ps, order, activeId] = buildPlayersFromState(state);
            Origami game(move(tiles), move(ps), move(order), cfg,
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
