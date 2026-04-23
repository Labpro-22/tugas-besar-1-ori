#include "Origami.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <set>
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
#include "include/utils/exceptions/BankruptException.hpp"
#include "include/utils/exceptions/NoPropertyException.hpp"
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

    // Only unowned STREET requires action (BELI or LELANG).
    // Railroad/Utility are auto-acquired; rent and tax are auto-paid.
    if (type == "STREET") {
        auto *prop = dynamic_cast<PropertyTile*>(t);
        if (prop && !prop->getTileOwner() && !prop->isMortgage()) {
            return true;
        }
    }

    return false;
}

int Origami::getPlayerNum(Player &p) const {
    for (int i = 0; i < static_cast<int>(players.size()); i++)
        if (players[i] == &p) return i + 1;
    return -1;
}

int Origami::countRailroadsOwnedBy(Player *owner) const {
    if (!owner) return 0;
    int n = 0;
    for (auto *t : tiles) {
        auto *p = dynamic_cast<PropertyTile*>(t);
        if (p && p->getTileType() == "RAILROAD" && p->getTileOwner() == owner) n++;
    }
    return n;
}

int Origami::countUtilitiesOwnedBy(Player *owner) const {
    if (!owner) return 0;
    int n = 0;
    for (auto *t : tiles) {
        auto *p = dynamic_cast<PropertyTile*>(t);
        if (p && p->getTileType() == "UTILITY" && p->getTileOwner() == owner) n++;
    }
    return n;
}

int Origami::computeRent(PropertyTile &prop, int diceTotal) const {
    if (prop.isMortgage()) return 0;
    Player *owner = prop.getTileOwner();
    string type = prop.getTileType();
    int base = 0;

    if (type == "STREET") {
        base = prop.calculateRent();
    } else if (type == "RAILROAD") {
        int cnt = countRailroadsOwnedBy(owner);
        base = config.getRailroadRentForCount(cnt);
        base = prop.applyFestival(base);
    } else if (type == "UTILITY") {
        int cnt = countUtilitiesOwnedBy(owner);
        int mult = config.getUtilityMultiplierForCount(cnt);
        base = diceTotal * mult;
        base = prop.applyFestival(base);
    }

    return base;
}

void Origami::recomputeMonopolyForGroup(const string &colorGroup) {
    auto group = board.getPropertiesByColorGroup(colorGroup);
    if (group.empty()) return;
    Player *owner = group[0]->getTileOwner();
    bool monopoly = (owner != nullptr);
    for (auto *gp : group) {
        if (gp->getTileOwner() != owner) { monopoly = false; break; }
    }
    for (auto *gp : group) gp->setMonopolized(monopoly);
}

void Origami::drawSkillCardAtTurnStart(Player &p) {
    static const vector<string> kinds = {"MOVE","DISCOUNT","SHIELD","TELEPORT","LASSO","DEMOLITION"};
    static mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());
    int boardSize = board.getTileCount();

    uniform_int_distribution<int> kindDist(0, static_cast<int>(kinds.size()) - 1);
    string kind = kinds[kindDist(rng)];

    SpecialPowerCard *card = nullptr;
    if (kind == "MOVE") {
        uniform_int_distribution<int> stepDist(1, 6);
        card = new MoveCard(stepDist(rng), boardSize);
    } else if (kind == "DISCOUNT") {
        uniform_int_distribution<int> pctDist(10, 50);
        card = new DiscountCard(pctDist(rng));
    } else if (kind == "SHIELD") {
        card = new ShieldCard();
    } else if (kind == "TELEPORT") {
        card = new TeleportCard();
    } else if (kind == "LASSO") {
        card = new LassoCard();
    } else {
        card = new DemolitionCard();
    }
    skill_cards.push_back(card);
    cout << p.getUsername() << " mendapatkan kartu kemampuan baru: " << card->describe() << "\n";
    p.addHandCard(card);
    addLog(p, "DRAW_KEMAMPUAN", kind + " - " + card->describe());

    // Enforce max 3 hand size
    if (p.getHandSize() > 3) {
        cout << "PERINGATAN: " << p.getUsername()
             << " memiliki " << p.getHandSize() << " kartu (Maks 3). Wajib buang 1 kartu.\n";
        bool isBot = (dynamic_cast<Bot*>(&p) != nullptr);
        if (isBot) {
            // Bot drops the most recently drawn card
            int dropIdx = p.getHandSize() - 1;
            cout << p.getUsername() << " (bot) membuang kartu ke-" << (dropIdx+1) << ".\n";
            p.removeHandCard(dropIdx);
            addLog(p, "DROP_KEMAMPUAN", to_string(dropIdx + 1));
        } else {
            // Human: list and prompt
            for (int i = 0; i < p.getHandSize(); i++) {
                auto *c = p.getHandCard(i);
                cout << "  " << (i+1) << ". " << (c ? c->describe() : "(?)") << "\n";
            }
            int idx = readInt("Pilih nomor kartu yang ingin dibuang: ", 1, p.getHandSize());
            p.removeHandCard(idx - 1);
            addLog(p, "DROP_KEMAMPUAN", to_string(idx));
        }
    }
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
        p->operator+=(initBalance);
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
        Player *p;
        if (s.is_bot) {
            p = new Bot(s.username);
        } else {
            p = new Player(s.username);
        }
        p->operator+=(s.money);
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

        // Restore hand cards
        it->second->clearHandCards();
        int boardSize = board.getTileCount();
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
            if (card) it->second->addHandCard(card);

            // Restore discount duration
            if (cs.type == "DISCOUNT" && !cs.remaining_duration.empty()) {
                float disc = it->second->getDiscountActive();
                // discount is already set by DiscountCard constructor
            }
        }

        // Restore jail turns
        if (ps.status == "JAIL" && ps.jail_turns > 0) {
            jail_turns[it->second] = ps.jail_turns;
        }
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
            if      (dynamic_cast<MoveCard*>(c))      { cs.type = "MOVE"; cs.value = std::to_string(static_cast<MoveCard*>(c)->getMoveValue()); }
            else if (dynamic_cast<DiscountCard*>(c))   { cs.type = "DISCOUNT"; cs.value = std::to_string(static_cast<DiscountCard*>(c)->getDiscountValue()); }
            else if (dynamic_cast<ShieldCard*>(c))      cs.type = "SHIELD";
            else if (dynamic_cast<TeleportCard*>(c))    cs.type = "TELEPORT";
            else if (dynamic_cast<LassoCard*>(c))       cs.type = "LASSO";
            else if (dynamic_cast<DemolitionCard*>(c))  cs.type = "DEMOLITION";
            else                                         cs.type = "UNKNOWN";
            ps.hand_cards.push_back(cs);
        }
        ps.is_bot = (dynamic_cast<Bot*>(p) != nullptr);
        auto jt = jail_turns.find(p);
        ps.jail_turns = (jt != jail_turns.end()) ? jt->second : 0;
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
        p += config.getGoSalary();
        cout << p.getUsername() << " melewati GO! Menerima gaji M"
             << config.getGoSalary() << ".\n";
        addLog(p, "GAJI_GO", "M" + to_string(config.getGoSalary()));
    }
}

// ── applyLanding ──────────────────────────────────────────────────────────────
void Origami::applyLanding(Player &p) {
    int     tileIdx = p.getCurrTile();
    Tile   *t       = tiles[tileIdx];
    string  type    = t->getTileType();

    // Card display helpers (local lambdas)
    auto centerText = [](const string &s, int w) -> string {
        if ((int)s.size() >= w) return s.substr(0, w);
        int lp = (w - (int)s.size()) / 2;
        return string(lp, ' ') + s + string(w - (int)s.size() - lp, ' ');
    };
    auto leftPad = [](const string &s, int w) -> string {
        if ((int)s.size() >= w) return s.substr(0, w);
        return s + string(w - (int)s.size(), ' ');
    };

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
        auto myProps = p.getOwnedProperties();
        if (myProps.empty()) {
            cout << "Festival! Tapi Anda tidak punya properti yang bisa difestivalkan.\n";
        } else {
            cout << "Festival! Pilih properti Anda:\n";
            for (auto *fp : myProps) {
                if (!fp->isMortgage()) {
                    int currentRent = computeRent(*fp, dice.getTotal());
                    string propType = fp->getTileType();
                    if (propType == "RAILROAD" || propType == "UTILITY")
                        currentRent = computeRent(*fp, 1);
                    cout << "  [" << fp->getTileCode() << "] "
                         << fp->getTileName()
                         << " (sewa sekarang M" << currentRent << ")\n";
                }
            }
            cout << "Gunakan: FESTIVAL <KODE>\n";
        }

    } else if (type == "TAX_PPH") {
        if (p.isShieldActive()) {
            cout << "Perisai aktif! Bebas dari Pajak Penghasilan.\n";
        } else {
            cmdBayarPajak(p);
        }

    } else if (type == "TAX_PBM") {
        if (p.isShieldActive()) {
            cout << "Perisai aktif! Bebas dari Pajak Barang Mewah.\n";
        } else {
            cmdBayarPajak(p);
        }

    } else if (type == "CHANCE") {
        ChanceCard *card = chance_deck.draw();
        if (card) {
            string desc = card->describe();
            int boxW = max(24, (int)desc.size() + 4);
            string bar(boxW, '=');
            cout << "+" << bar << "+\n";
            cout << "| " << centerText("KARTU KESEMPATAN", boxW - 2) << " |\n";
            cout << "+" << bar << "+\n";
            cout << "| " << leftPad(desc, boxW - 2) << " |\n";
            cout << "+" << bar << "+\n";
            int before = p.getCurrTile();
            CardManager::resolveCardEffect(*card, p, players);
            chance_deck.discardCard(card);
            if (p.getStatus() == "JAIL" && jail_turns.find(&p) == jail_turns.end())
                jail_turns[&p] = 0;
            if (p.getCurrTile() != before && p.getStatus() != "JAIL" && p.getStatus() != "BANKRUPT")
                applyLanding(p);
            addLog(p, "KARTU", "Kesempatan: " + card->describe());
        }

    } else if (type == "COMMUNITY_CHEST") {
        CommunityChestCard *card = community_deck.draw();
        if (card) {
            string desc = card->describe();
            int boxW = max(24, (int)desc.size() + 4);
            string bar(boxW, '=');
            cout << "+" << bar << "+\n";
            cout << "| " << centerText("KARTU DANA UMUM", boxW - 2) << " |\n";
            cout << "+" << bar << "+\n";
            cout << "| " << leftPad(desc, boxW - 2) << " |\n";
            cout << "+" << bar << "+\n";
            if (p.isShieldActive()) {
                cout << "[SHIELD ACTIVE]: Efek ShieldCard melindungi Anda!\n";
                cout << "Tagihan dari kartu Dana Umum dibatalkan. Uang Anda tetap: M" << p.getBalance() << ".\n";
            } else {
                int balBefore = p.getBalance();
                CardManager::resolveCardEffect(*card, p, players);
                // Check bankruptcy after card effect
                if (p.getBalance() <= 0 && balBefore > 0) {
                    int maxLiq = PropertyManager::calculateMaxLiquidation(p);
                    if (maxLiq <= 0) {
                        cout << p.getUsername() << " tidak mampu membayar! Bangkrut!\n";
                        cmdBangkrut(p);
                    } else {
                        cout << "Saldo habis setelah efek kartu. Lakukan likuidasi aset.\n";
                    }
                }
            }
            community_deck.discardCard(card);
            addLog(p, "KARTU", "Dana Umum: " + card->describe());
        }

    } else if (type == "STREET" || type == "RAILROAD" || type == "UTILITY") {
        auto *prop = dynamic_cast<PropertyTile*>(t);
        if (!prop) return;
        if (prop->isMortgage()) {
            cout << "Properti ini sedang digadaikan. Tidak ada sewa.\n";
            return;
        }
        Player *owner = prop->getTileOwner();
        if (!owner) {
            if (type == "RAILROAD" || type == "UTILITY") {
                // Auto-acquire for free
                p.addOwnedProperty(prop);
                cout << "Belum ada yang menginjaknya duluan, "
                     << prop->getTileName() << " kini menjadi milikmu!\n";
                addLog(p, "OTOMATIS",
                       prop->getTileName() + " (" + prop->getTileCode() + ") jadi milik " + p.getUsername());
            } else {
                // STREET: prompt buy or auction
                cout << "Properti belum dimiliki. Gunakan BELI untuk membeli, atau ketik SELESAI untuk masuk lelang otomatis.\n";
            }
        } else if (owner == &p) {
            cout << "Ini properti Anda sendiri.\n";
        } else {
            if (p.isShieldActive()) {
                cout << "Perisai aktif! Tidak perlu membayar sewa.\n";
            } else {
                int rent = computeRent(*prop, dice.getTotal());
                cout << "Anda harus membayar sewa M" << rent
                     << " ke " << owner->getUsername() << ".\n";
                autoPayRent(p, *prop);
            }
        }
    }
    // START, JAIL, FREE_PARKING → no special effect
}

// ── autoPayRent ───────────────────────────────────────────────────────────────
void Origami::autoPayRent(Player &p, PropertyTile &prop) {
    Player *owner = prop.getTileOwner();
    if (!owner || owner == &p) return;

    int rent = computeRent(prop, dice.getTotal());
    if (rent <= 0) return;

    // Apply payer's discount card
    if (p.getDiscountActive() > 0.0F) {
        rent = static_cast<int>(rent * (1.0F - p.getDiscountActive()));
    }

    bool isBot = (dynamic_cast<Bot*>(&p) != nullptr);

    if (p.getBalance() < rent) {
        // Bankruptcy flow
        if (isBot) {
            // Pay what you can
            int payment = p.getBalance();
            if (payment > 0) { p += -payment; owner->operator+=(payment); }
            cmdBangkrut(p);
        } else {
            cout << "Saldo tidak cukup untuk membayar sewa M" << rent << "!\n";
            recoverForRent(p, prop);
        }
        return;
    }

    p += -rent;
    owner->operator+=(rent);
    cout << p.getUsername() << " membayar sewa M" << rent << " ke " << owner->getUsername() << ".\n";
    cout << "  Saldo " << p.getUsername() << ": M" << p.getBalance() << " | Saldo " << owner->getUsername() << ": M" << owner->getBalance() << "\n";
    int lvl = prop.getLevel();
    string lvlStr = (lvl == 0) ? "unimproved" : (lvl >= 5 ? "hotel" : to_string(lvl) + " rumah");
    string rentDetail = "Bayar M" + to_string(rent) + " ke " + owner->getUsername()
                      + " (" + prop.getTileCode() + ", " + lvlStr;
    if (prop.getFestivalDuration() > 0)
        rentDetail += ", festival x" + to_string(prop.getFestivalMultiplier());
    rentDetail += ")";
    addLog(p, "SEWA", rentDetail);
}

// ── recoverForRent ────────────────────────────────────────────────────────────
void Origami::recoverForRent(Player &p, PropertyTile &prop) {
    Player *owner = prop.getTileOwner();
    int rent = computeRent(prop, dice.getTotal());

    while (p.getBalance() < rent && p.getStatus() != "BANKRUPT") {
        int maxCanPay = PropertyManager::calculateMaxLiquidation(p);
        cout << "Saldo: M" << p.getBalance() << " / Butuh: M" << rent << "\n";

        if (maxCanPay < rent) {
            cout << "Tidak cukup aset untuk membayar sewa. Bangkrut!\n";
            cmdBangkrut(p);
            return;
        }

        cout << "Gadaikan atau lelang properti Anda dulu:\n";
        cout << "  GADAI <kode>       - Gadaikan properti\n";
        cout << "  LELANG <kode>      - Lelang properti milik Anda\n";
        cout << "  JUAL <kode>        - Jual properti ke Bank (harga beli)\n";
        cout << "  CETAK_PAPAN        - Lihat papan permainan\n";
        cout << "  CETAK_AKTA <kode>  - Lihat akta properti\n";
        cout << "  CETAK_PROPERTI     - Lihat daftar properti Anda\n";
        cout << "  CETAK_PROFIL       - Lihat profil pemain (uang, status, dll)\n";
        cout << "  BANGKRUT           - Menyerah\n";
        cout << "> ";
        string cmd;
        if (!(cin >> cmd)) { cmdBangkrut(p); return; }
        for (auto &c : cmd) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));

        if (cmd == "GADAI") {
            string code; cin >> code;
            cmdGadai(p, code);
        } else if (cmd == "LELANG") {
            string code; cin >> code;
            cmdLelang(p, code);
        } else if (cmd == "JUAL") {
            string code; cin >> code;
            Tile *t = board.getTileByCode(code);
            auto *prop = dynamic_cast<PropertyTile*>(t);
            if (!prop || prop->getTileOwner() != &p) {
                cout << "Bukan properti Anda.\n";
            } else {
                int sellPrice = prop->getBuyPrice();
                if (prop->getTileType() == "STREET" && prop->getLevel() > 0) {
                    int buildingValue = 0;
                    int lvl = prop->getLevel();
                    if (lvl >= 5) {
                        buildingValue = (4 * prop->getHouseCost()) + prop->getHotelCost();
                    } else {
                        buildingValue = lvl * prop->getHouseCost();
                    }
                    sellPrice += buildingValue / 2;
                }
                // Must sell buildings first if any exist on color group
                if (prop->getTileType() == "STREET") {
                    string cg = prop->getColorGroup();
                    auto group = board.getPropertiesByColorGroup(cg);
                    bool anyBuilding = false;
                    for (auto *gp : group) if (gp->getLevel() > 0) { anyBuilding = true; break; }
                    if (anyBuilding) {
                        cout << "Masih ada bangunan di color group [" << cg << "]. Bangunan harus dijual dulu.\n";
                        cout << "Gunakan GADAI untuk menjual bangunan, atau BANGUN untuk mengatur ulang.\n";
                        continue;
                    }
                }
                if (prop->isMortgage()) {
                    cout << "Properti sedang digadaikan. Tebus dulu sebelum menjual.\n";
                    continue;
                }
                p += sellPrice;
                p.removeOwnedProperty(prop->getTileCode());
                prop->setLevel(0);
                prop->setMortgageStatus(false);
                recomputeMonopolyForGroup(prop->getTileType() == "STREET" ? prop->getColorGroup() : "");
                cout << "Properti " << code << " dijual ke Bank seharga M" << sellPrice << ".\n";
                cout << "  Saldo " << p.getUsername() << ": M" << p.getBalance() << "\n";
                addLog(p, "JUAL_BANK", code + " M" + to_string(sellPrice));
            }
        } else if (cmd == "CETAK_PAPAN") {
            cmdCetakPapan();
        } else if (cmd == "CETAK_AKTA") {
            string code; cin >> code;
            cmdCetakAkta(code);
        } else if (cmd == "CETAK_PROPERTI") {
            cmdCetakProperti(p);
        } else if (cmd == "CETAK_PROFIL") {
            formatter.printPlayerStatus(p);
        } else if (cmd == "BANGKRUT") {
            cmdBangkrut(p); return;
        } else {
            cout << "Perintah tidak dikenali.\n";
        }
    }

    if (p.getStatus() == "BANKRUPT") return;

    if (owner) {
        p += -rent;
        owner->operator+=(rent);
        cout << p.getUsername() << " membayar sewa M" << rent << " ke " << owner->getUsername() << ".\n";
        addLog(p, "SEWA", prop.getTileCode() + " M" + to_string(rent));
    }
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
    formatter.printAkta(*prop, board, config);
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
    // Railroad/Utility are auto-acquired on landing
    if (prop->getTileType() != "STREET") {
        cout << "Railroad/Utility didapat otomatis saat mendarat. Tidak perlu BELI.\n";
        return;
    }
    int price = prop->getBuyPrice();
    // Apply discount if active
    if (p.getDiscountActive() > 0.0F) {
        price = static_cast<int>(price * (1.0F - p.getDiscountActive()));
    }
    if (p.getBalance() < price) {
        cout << "Saldo tidak cukup. Properti masuk lelang.\n";
        vector<Player*> participants;
        for (auto *pl : turn_order)
            if (pl->getStatus() != "BANKRUPT") participants.push_back(pl);
        formatter.printAuction();
        interactiveLelang(nullptr, *prop, participants);
        return;
    }
    p += -price;
    p.addOwnedProperty(prop);
    recomputeMonopolyForGroup(prop->getColorGroup());
    cout << p.getUsername() << " membeli " << t->getTileName() << " seharga M" << price << ".\n";
    addLog(p, "BELI", t->getTileName() + " (" + t->getTileCode() + ") M" + to_string(price));
}

void Origami::cmdBayarSewa(Player &p) {
    int tileIdx   = p.getCurrTile();
    Tile *t       = tiles[tileIdx];
    auto *prop    = dynamic_cast<PropertyTile*>(t);
    Player *owner = getCreditorAt(tileIdx);
    if (!owner || owner == &p) {
        cout << "Tidak ada sewa yang harus dibayar di sini.\n"; return;
    }
    if (p.isShieldActive()) {
        cout << "Perisai aktif! Sewa tidak perlu dibayar.\n"; return;
    }
    if (prop) autoPayRent(p, *prop);
}

void Origami::cmdBayarPajak(Player &p) {
    string type = tiles[p.getCurrTile()]->getTileType();
    int amt = 0;
    bool isBot = (dynamic_cast<Bot*>(&p) != nullptr);

    if (type == "TAX_PPH") {
        cout << "Petak Pajak Penghasilan (PPH)\n";
        cout << "  1. Bayar flat M" << config.getPphFlat() << "\n";
        cout << "  2. Bayar " << config.getPphPercentage() << "% dari total kekayaan\n";

        int choice;
        if (isBot) {
            // Bot picks flat if balance < flat, percentage otherwise (simple heuristic without computing net worth first)
            choice = (p.getBalance() < config.getPphFlat()) ? 1 : 2;
            cout << "Bot memilih opsi " << choice << ".\n";
        } else {
            choice = readInt("Pilihan (1/2): ", 1, 2);
        }

        if (choice == 1) {
            amt = config.getPphFlat();
            if (p.getBalance() < amt) {
                cout << "Tidak mampu membayar pajak flat M" << amt << ". Bangkrut!\n";
                if (isBot || p.getBalance() + PropertyManager::calculateMaxLiquidation(p) - p.getBalance() < amt) {
                    cmdBangkrut(p);
                } else {
                    recoverForRent(p, *dynamic_cast<PropertyTile*>(tiles[p.getCurrTile()]));
                }
                return;
            }
        } else {
            int wealth = p.getNetWorth();
            amt = static_cast<int>(wealth * config.getPphPercentage() / 100.0);
            cout << "Total kekayaan: M" << wealth << ", pajak: M" << amt << "\n";
        }
    } else if (type == "TAX_PBM") {
        amt = config.getPbmFlat();
    } else { cout << "Bukan petak pajak.\n"; return; }

    if (p.getBalance() >= amt) {
        p += -amt;
        cout << p.getUsername() << " membayar pajak M" << amt << ".\n";
        addLog(p, "BAYAR_PAJAK", type + " M" + to_string(amt));
    } else {
        // Try liquidation
        int maxLiq = PropertyManager::calculateMaxLiquidation(p);
        if (maxLiq < amt) {
            cout << "Tidak cukup aset. Bangkrut ke Bank!\n";
            cmdBangkrut(p);
            return;
        }
        if (isBot) {
            cmdBangkrut(p);
            return;
        }
        cout << "Saldo tidak cukup. Lakukan GADAI/LELANG dulu, lalu BAYAR_PAJAK lagi.\n";
    }
}

void Origami::cmdGadai(Player &p, const string &code) {
    Tile *t = board.getTileByCode(code);
    if (!t) { cout << "Kode tidak ditemukan.\n"; return; }
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop || prop->getTileOwner() != &p) {
        cout << "Bukan properti Anda.\n"; return;
    }
    if (prop->isMortgage()) {
        cout << "Sudah digadaikan.\n"; return;
    }

    // Enforce color-group building sale (STREET only)
    string cg = prop->getColorGroup();
    if (prop->getTileType() == "STREET") {
        auto group = board.getPropertiesByColorGroup(cg);
        bool anyBuilding = false;
        for (auto *gp : group) if (gp->getLevel() > 0) { anyBuilding = true; break; }
        if (anyBuilding) {
            cout << prop->getTileName() << " tidak dapat digadaikan!\n";
            cout << "Masih ada bangunan di color group [" << cg << "]. Bangunan harus dijual dulu (setengah harga).\n";
            bool isBot = (dynamic_cast<Bot*>(&p) != nullptr);
            char confirm = 'Y';
            if (!isBot) {
                cout << "Jual semua bangunan color group [" << cg << "]? (y/n): ";
                confirm = readChar("", "YN");
            }
            if (confirm == 'N') { cout << "Penggadaian dibatalkan.\n"; return; }
            for (auto *gp : group) {
                while (gp->getLevel() > 0) {
                    int lvl = gp->getLevel();
                    int unitCost = (lvl >= 5) ? gp->getHotelCost() : gp->getHouseCost();
                    int refund = unitCost / 2;
                    p += refund;
                    gp->setLevel(lvl - 1);
                    cout << "Bangunan " << gp->getTileName() << " level " << lvl
                         << " terjual. Diterima M" << refund << ".\n";
                    addLog(p, "JUAL_BANGUNAN", gp->getTileCode() + " M" + to_string(refund));
                }
            }
        }
    }

    prop->setMortgageStatus(true);
    p += prop->getMortgageValue();
    recomputeMonopolyForGroup(cg);
    cout << "Properti " << code << " berhasil digadaikan. Diterima M" << prop->getMortgageValue() << ".\n";
    cout << "  Saldo " << p.getUsername() << ": M" << p.getBalance() << "\n";
    addLog(p, "GADAI", code + " M" + to_string(prop->getMortgageValue()));
}

void Origami::cmdTebus(Player &p, const string &code) {
    Tile *t = board.getTileByCode(code);
    if (!t) { cout << "Kode tidak ditemukan.\n"; return; }
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop || prop->getTileOwner() != &p || !prop->isMortgage()) {
        cout << "Properti tidak dapat ditebus.\n"; return;
    }
    int cost = prop->getBuyPrice();
    if (cost <= 0) cost = prop->getMortgageValue() + (prop->getMortgageValue() / 2);
    if (cost <= 0) { cout << "Properti ini tidak dapat ditebus (harga 0).\n"; return; }
    if (p.getBalance() < cost) {
        cout << "Saldo tidak cukup (perlu M" << cost << ").\n"; return;
    }
    p += -cost;
    prop->setMortgageStatus(false);
    cout << "Properti " << code << " ditebus seharga M" << cost << ".\n";
    cout << "  Saldo " << p.getUsername() << ": M" << p.getBalance() << "\n";
    addLog(p, "TEBUS", code + " M" + to_string(cost));
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

        p += -cost;
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

    p += -cost;
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

    Player *owner = prop->getTileOwner();

    vector<Player*> participants;
    for (auto *pl : turn_order)
        if (pl->getStatus() != "BANKRUPT") participants.push_back(pl);

    if (!owner) {
        if (p.getCurrTile() != board.getTileIndex(code)) {
            cout << "Properti bank hanya bisa dilelang saat Anda berdiri di atasnya dan memilih tidak membeli.\n";
            return;
        }
        formatter.printAuction();
        interactiveLelang(nullptr, *prop, participants);
    } else if (owner == &p) {
        formatter.printAuction();
        interactiveLelang(&p, *prop, participants);
    } else {
        cout << "Hanya bisa melelang properti milik sendiri atau properti bank di petak Anda.\n";
    }
}

void Origami::interactiveLelang(Player *seller, PropertyTile &prop,
                                 vector<Player*> & /*participants*/) {
    cout << "Properti : [" << prop.getTileCode() << "] " << prop.getTileName() << "\n";

    // Build bidder rotation starting AFTER trigger. Trigger (seller or active player)
    // is always excluded from the main loop and added at the end only for bank auctions.
    Player *trigger = seller;
    if (!trigger) {
        if (active_player_id >= 0 && active_player_id < static_cast<int>(players.size()))
            trigger = players[active_player_id];
    }

    vector<Player*> bidders;
    int sz = static_cast<int>(turn_order.size());
    int triggerIdx = 0;
    for (int i = 0; i < sz; i++) if (turn_order[i] == trigger) { triggerIdx = i; break; }
    for (int i = 1; i <= sz; i++) {
        Player *pl = turn_order[(triggerIdx + i) % sz];
        if (pl == trigger) continue;           // always exclude trigger from main loop
        if (pl->getStatus() == "BANKRUPT") continue;
        bidders.push_back(pl);
    }
    // For bank-property auctions, the active player (trigger) is also a bidder — added once at end.
    if (!seller && trigger && trigger->getStatus() != "BANKRUPT") {
        bidders.push_back(trigger);
    }

    if (bidders.empty()) {
        cout << "Tidak ada peserta lelang.\n";
        return;
    }

    int needPasses = static_cast<int>(bidders.size()) - 1;
    if (needPasses < 1) needPasses = 1;

    cout << "Peserta (" << bidders.size() << " orang) — minimum bid M0.\n";
    cout << "Aksi: PASS atau BID <jumlah>\n";

    int highestBid     = -1;
    Player *highestBidder = nullptr;
    int  consecPasses  = 0;   // consecutive passes since last bid
    bool anyBid        = false;
    int  rot           = 0;
    // Players who can never bid (broke / bankrupt)
    set<Player*> permanentPass;

    while (true) {
        // Done when N-1 consecutive passes after at least one bid
        if (anyBid && consecPasses >= needPasses) break;
        // All players passed without any bid — force last eligible to bid
        if (!anyBid && consecPasses >= needPasses) {
            // Find next eligible bidder
            Player *last = nullptr;
            for (int j = 0; j < static_cast<int>(bidders.size()); j++) {
                Player *candidate = bidders[(rot + j) % static_cast<int>(bidders.size())];
                if (!permanentPass.count(candidate) && candidate->getStatus() != "BANKRUPT") {
                    last = candidate;
                    break;
                }
            }
            if (!last) {
                // Nobody eligible — no winner
                break;
            }
            cout << "[Wajib BID — semua pemain lain sudah PASS]\n";
            bool isBotForced = (dynamic_cast<Bot*>(last) != nullptr);
            int forcedBid = max(0, highestBid + 1);
            if (isBotForced) {
                forcedBid = min(max(0, forcedBid), last->getBalance());
            } else {
                cout << "Giliran: " << last->getUsername() << " (saldo M" << last->getBalance() << ")\n";
                cout << "Anda WAJIB melakukan BID. Minimum: M" << forcedBid << "\n";
                cout << "BID <jumlah>: ";
                cin >> forcedBid;
                if (forcedBid < 0) forcedBid = 0;
            }
            if (forcedBid >= 0 && forcedBid > highestBid && forcedBid <= last->getBalance()) {
                highestBid = forcedBid;
                highestBidder = last;
                anyBid = true;
            } else if (forcedBid == 0 && highestBid < 0) {
                // M0 is a valid opening bid
                highestBid = 0;
                highestBidder = last;
                anyBid = true;
            }
            break;
        }

        Player *cur = bidders[rot % static_cast<int>(bidders.size())];
        rot++;
        if (permanentPass.count(cur)) continue;
        if (cur->getStatus() == "BANKRUPT") { permanentPass.insert(cur); consecPasses++; continue; }

        int minNext = anyBid ? (highestBid + 1) : 0; // opening bid min M0
        if (cur->getBalance() < minNext) {
            cout << cur->getUsername() << " auto-pass (saldo M" << cur->getBalance() << " < min M" << minNext << ").\n";
            permanentPass.insert(cur);
            consecPasses++;
            continue;
        }

        bool isBot = (dynamic_cast<Bot*>(cur) != nullptr);

        if (isBot) {
            int proposed = highestBid + 5;
            if (!anyBid) proposed = max(1, max(prop.getMortgageValue(), prop.getBuyPrice() / 4));
            if (proposed > cur->getBalance() * 3 / 5) {
                cout << cur->getUsername() << " (bot) PASS\n";
                consecPasses++;
            } else {
                proposed = min(proposed, cur->getBalance());
                if (proposed <= highestBid) proposed = highestBid + 1;
                if (proposed > cur->getBalance()) {
                    cout << cur->getUsername() << " (bot) PASS (tak mampu)\n";
                    permanentPass.insert(cur);
                    consecPasses++;
                } else {
                    cout << cur->getUsername() << " (bot) BID M" << proposed << "\n";
                    highestBid    = proposed;
                    highestBidder = cur;
                    anyBid        = true;
                    consecPasses  = 0;
                }
            }
        } else {
            cout << "\nGiliran: " << cur->getUsername() << " (saldo M" << cur->getBalance() << ")\n";
            if (highestBidder)
                cout << "Penawaran tertinggi: M" << highestBid << " (" << highestBidder->getUsername() << ")\n";
            else
                cout << "Belum ada penawaran. Minimum bid: M0\n";
            cout << "Aksi (PASS / BID <jumlah>)\n> ";

            string action;
            cin >> action;
            for (auto &c : action) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));

            if (action == "PASS") {
                cout << cur->getUsername() << " PASS\n";
                consecPasses++;
            } else if (action == "BID") {
                int bid; cin >> bid;
                if (bid <= highestBid || bid < 0 || bid > cur->getBalance()) {
                    cout << "Bid tidak valid (harus > M" << highestBid
                         << " dan <= saldo M" << cur->getBalance() << "). Dianggap PASS.\n";
                    consecPasses++;
                } else {
                    highestBid    = bid;
                    highestBidder = cur;
                    anyBid        = true;
                    consecPasses  = 0;
                }
            } else {
                cout << "Perintah tidak dikenal. Silakan masukkan PASS atau BID <jumlah>.\n";
                continue; // don't count as pass, allow retry
            }
        }

        if (anyBid && consecPasses >= needPasses) break;
    }

    if (!highestBidder) {
        cout << "Lelang gagal — tidak ada pemenang.\n";
        return;
    }

    highestBidder->operator+=(-highestBid);
    if (seller) seller->removeOwnedProperty(prop.getTileCode());
    highestBidder->addOwnedProperty(&prop);
    recomputeMonopolyForGroup(prop.getColorGroup());

    cout << "\nLelang selesai!\n";
    cout << "Pemenang: " << highestBidder->getUsername() << "\n";
    cout << "Harga akhir: M" << highestBid << "\n";

    Player &logPlayer = seller ? *seller : *highestBidder;
    addLog(logPlayer, "LELANG", prop.getTileCode() + " -> " + highestBidder->getUsername()
                                + " M" + to_string(highestBid));
}

void Origami::cmdFestival(Player &p, const string &code, int /*mult*/, int /*dur*/) {
    if (tiles[p.getCurrTile()]->getTileType() != "FESTIVAL") {
        cout << "Hanya bisa di petak Festival.\n"; return;
    }
    Tile *t = board.getTileByCode(code);
    if (!t) { cout << "Kode '" << code << "' tidak ditemukan. Lihat kode di atas.\n"; return; }
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop || prop->getTileOwner() != &p) {
        cout << "Bukan properti Anda.\n"; return;
    }

    // Festival: rent x2 for 3 turns, stacks up to 3x (x8)
    // At max stacks, only reset duration
    int curMult = prop->getFestivalMultiplier();
    int newMult;
    bool atMax = false;
    if (curMult <= 1) newMult = 2;
    else if (curMult == 2) newMult = 4;
    else if (curMult == 4) newMult = 8;
    else { newMult = 8; atMax = true; }

    int oldRent = prop->calculateRent();
    prop->setFestivalState(newMult, 3);
    int newRent = prop->calculateRent();

    if (atMax) {
        cout << "Efek Festival di [" << code << "] " << prop->getTileName()
             << " sudah maksimum (x8). Durasi reset menjadi 3.\n";
    } else {
        cout << "Festival di [" << code << "] " << prop->getTileName()
             << "! Sewa M" << oldRent << " → M" << newRent
             << " (x" << newMult << " selama 3 giliran).\n";
    }
    addLog(p, "FESTIVAL", code + ": x" + to_string(newMult) + " 3 giliran");
}

void Origami::cmdBangkrut(Player &p) {
    Player *creditor = getCreditorAt(p.getCurrTile());
    vector<Player*> others;
    for (auto *pl : players)
        if (pl != &p && pl->getStatus() != "BANKRUPT") others.push_back(pl);

    if (creditor && creditor != &p) {
        // Bankrupt to player: transfer all cash + properties (incl. mortgaged)
        events.processBankruptcy(p, creditor);
        events.flushEventsTo(cout);
    } else {
        // Bankrupt to bank: forfeit cash, destroy buildings, auction one by one
        p.setStatus("BANKRUPT");
        int cash = p.getBalance();
        if (cash > 0) {
            p += -cash;
            cout << "Uang sisa M" << cash << " diserahkan ke Bank.\n";
        }
        // Snapshot properties (we'll release each before auctioning)
        auto props = p.getOwnedProperties();
        for (auto *prop : props) {
            if (!prop) continue;
            // Destroy buildings, clear status
            prop->setLevel(0);
            prop->setMortgageStatus(false);
            prop->setMonopolized(false);
            prop->setFestivalState(1, 0);
            // Release from owner
            p.removeOwnedProperty(prop->getTileCode());
            // Auction
            cout << "→ Melelang " << prop->getTileName() << " (" << prop->getTileCode() << ")...\n";
            interactiveLelang(nullptr, *prop, others);
        }
    }
    cout << p.getUsername() << " bangkrut!\n";
    addLog(p, "BANGKRUT", "");
}

void Origami::cmdSimpan(const string &path, bool hasRolled) {
    try {
        GameStates::SavePermission perm;
        perm.has_rolled_dice     = hasRolled;
        perm.is_at_start_of_turn = !hasRolled;

        string fullpath = path;
        if (path.find('/') == string::npos && path.find('\\') == string::npos) {
            fullpath = "save/" + path;
        }

        filesystem::create_directories(filesystem::path(fullpath).parent_path());

        FileManager::saveConfig(fullpath, buildSaveState(), perm);
        cout << "Game disimpan ke " << fullpath << ".\n";
        if (active_player_id >= 0 && active_player_id < static_cast<int>(players.size()))
            addLog(*players[active_player_id], "SIMPAN", fullpath);
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
        cout << "Saldo tidak cukup (perlu M" << fine << ").\n"; return;
    }
    p += -fine;
    p.setStatus("ACTIVE");
    jail_turns.erase(&p);
    cout << p.getUsername() << " membayar denda M" << fine
         << " dan bebas dari penjara. Lempar dadu di giliran berikutnya.\n";
    addLog(p, "BAYAR_DENDA", to_string(fine));
}

void Origami::cmdGunakanKemampuan(Player &p, int index) {
    SpecialPowerCard *card = p.getHandCard(index);
    if (!card) { cout << "Kartu tidak ditemukan.\n"; return; }
    try {
        SkillCardManager::useSkillCard(p);
    } catch (const exception &) {
        cout << "Kamu sudah menggunakan kartu kemampuan pada giliran ini! Penggunaan kartu dibatasi maksimal 1 kali dalam 1 giliran.\n";
        return;
    }
    int before = p.getCurrTile();
    string cardDesc = card->describe();

    if (dynamic_cast<TeleportCard*>(card)) {
        p.removeHandCard(index);
        cout << "TeleportCard: Pindah ke petak mana saja di papan.\n";
        formatter.printBoard(board, players, current_turn, max_turn);
        cout << "Daftar petak:\n";
        for (int i = 0; i < board.getTileCount(); i++) {
            Tile *t = tiles[i];
            cout << "  " << i << ". [" << t->getTileCode() << "] " << t->getTileName() << "\n";
        }
        int target = readInt("Pilih nomor petak tujuan (0-" + to_string(board.getTileCount()-1) + "): ", 0, board.getTileCount()-1);
        int oldTile = p.getCurrTile();
        p.setCurrTile(target);
        p.setSkillUsed(true);
        applyGoSalary(p, oldTile);
        cout << p.getUsername() << " berteleportasi ke [" << tiles[target]->getTileCode() << "] " << tiles[target]->getTileName() << "!\n";
        if (p.getCurrTile() != before)
            applyLanding(p);
        addLog(p, "GUNAKAN_KEMAMPUAN", "TeleportCard -> " + string(tiles[target]->getTileCode()));
    } else if (dynamic_cast<LassoCard*>(card)) {
        p.removeHandCard(index);
        vector<Player*> others;
        for (auto *pl : players)
            if (pl != &p && pl->getStatus() != "BANKRUPT") others.push_back(pl);
        if (others.empty()) {
            cout << "Tidak ada pemain lain yang bisa ditarik.\n";
            addLog(p, "GUNAKAN_KEMAMPUAN", "LassoCard - tidak ada target");
            return;
        }
        cout << "LassoCard: Tarik 1 pemain lain ke petak Anda.\n";
        for (int i = 0; i < static_cast<int>(others.size()); i++)
            cout << "  " << (i+1) << ". " << others[i]->getUsername() << " (petak " << others[i]->getCurrTile() << ")\n";
        int target = readInt("Pilih nomor pemain (0 untuk batal): ", 0, static_cast<int>(others.size()));
        if (target == 0) {
            cout << "LassoCard dibatalkan.\n";
            addLog(p, "GUNAKAN_KEMAMPUAN", "LassoCard - dibatalkan");
            return;
        }
        Player *targetPlayer = others[target-1];
        int myTile = p.getCurrTile();
        int oldTargetTile = targetPlayer->getCurrTile();
        targetPlayer->setCurrTile(myTile);
        p.setSkillUsed(true);
        cout << targetPlayer->getUsername() << " ditarik ke petak " << myTile << " [" << tiles[myTile]->getTileCode() << "]!\n";
        addLog(p, "GUNAKAN_KEMAMPUAN", "LassoCard -> " + targetPlayer->getUsername());
    } else if (dynamic_cast<DemolitionCard*>(card)) {
        p.removeHandCard(index);
        auto myProps = p.getOwnedProperties();
        vector<pair<Player*, vector<PropertyTile*>>> opponentsWithBuildings;
        for (auto *pl : players) {
            if (pl == &p || pl->getStatus() == "BANKRUPT") continue;
            vector<PropertyTile*> built;
            for (auto *prop : pl->getOwnedProperties())
                if (prop->getLevel() > 0) built.push_back(prop);
            if (!built.empty()) opponentsWithBuildings.push_back({pl, built});
        }
        if (opponentsWithBuildings.empty()) {
            cout << "Tidak ada bangunan lawan yang bisa dihancurkan.\n";
            addLog(p, "GUNAKAN_KEMAMPUAN", "DemolitionCard - tidak ada target");
            return;
        }
        cout << "DemolitionCard: Hancurkan 1 bangunan di properti lawan.\n";
        for (int i = 0; i < static_cast<int>(opponentsWithBuildings.size()); i++) {
            auto &[pl, built] = opponentsWithBuildings[i];
            cout << "  " << (i+1) << ". " << pl->getUsername() << ":\n";
            for (auto *prop : built) {
                string lvlStr = (prop->getLevel() >= 5) ? "Hotel" : to_string(prop->getLevel()) + " rumah";
                cout << "      [" << prop->getTileCode() << "] " << prop->getTileName() << " - " << lvlStr << "\n";
            }
        }
        int oppIdx = readInt("Pilih nomor pemain lawan (0 untuk batal): ", 0, static_cast<int>(opponentsWithBuildings.size()));
        if (oppIdx == 0) {
            cout << "DemolitionCard dibatalkan.\n";
            addLog(p, "GUNAKAN_KEMAMPUAN", "DemolitionCard - dibatalkan");
            return;
        }
        auto &[targetPlayer, built] = opponentsWithBuildings[oppIdx-1];
        for (int j = 0; j < static_cast<int>(built.size()); j++) {
            string lvlStr = (built[j]->getLevel() >= 5) ? "Hotel" : to_string(built[j]->getLevel()) + " rumah";
            cout << "  " << (j+1) << ". [" << built[j]->getTileCode() << "] " << built[j]->getTileName() << " - " << lvlStr << "\n";
        }
        int propIdx = readInt("Pilih bangunan yang ingin dihancurkan (0 untuk batal): ", 0, static_cast<int>(built.size()));
        if (propIdx == 0) {
            cout << "DemolitionCard dibatalkan.\n";
            addLog(p, "GUNAKAN_KEMAMPUAN", "DemolitionCard - dibatalkan");
            return;
        }
        PropertyTile *targetProp = built[propIdx-1];
        targetProp->setLevel(targetProp->getLevel() - 1);
        string newLvl = (targetProp->getLevel() >= 5) ? "Hotel" : (targetProp->getLevel() == 0 ? "tanah kosong" : to_string(targetProp->getLevel()) + " rumah");
        cout << "Bangunan di [" << targetProp->getTileCode() << "] " << targetProp->getTileName() << " dihancurkan! Sekarang: " << newLvl << ".\n";
        p.setSkillUsed(true);
        addLog(p, "GUNAKAN_KEMAMPUAN", "DemolitionCard -> " + targetProp->getTileCode());
    } else {
        // Cache card type before removing from hand
        bool isMoveCard = (dynamic_cast<MoveCard*>(card) != nullptr);
        bool isShieldCard = (dynamic_cast<ShieldCard*>(card) != nullptr);
        bool isDiscountCard = (dynamic_cast<DiscountCard*>(card) != nullptr);

        // Execute effect then remove from hand
        card->action(p);
        p.removeHandCard(index);

        if (isShieldCard) {
            cout << "ShieldCard diaktifkan! Anda kebal terhadap tagihan atau sanksi selama giliran ini.\n";
        } else if (isDiscountCard) {
            cout << "DiscountCard diaktifkan! " << cardDesc << "\n";
        } else if (isMoveCard) {
            cout << "MoveCard digunakan! " << cardDesc << "\n";
        } else {
            cout << "Kartu kemampuan digunakan: " << cardDesc << "\n";
        }
        if (p.getCurrTile() != before)
            applyLanding(p);
        addLog(p, "GUNAKAN_KEMAMPUAN", cardDesc);
    }
}

void Origami::cmdDropKemampuan(Player &p, int index) {
    if (!p.getHandCard(index)) { cout << "Kartu tidak ditemukan.\n"; return; }
    p.removeHandCard(index);
    cout << "Kartu dibuang.\n";
    addLog(p, "DROP_KEMAMPUAN", to_string(index + 1));
}

// ── humanTurn ─────────────────────────────────────────────────────────────────
void Origami::humanTurn(Player &p) {
    bool has_rolled            = false;
    bool turn_ended            = false;
    bool paid_fine_this_turn   = false;
    bool has_bonus_turn        = false;
    bool has_executed_action   = false;
    bool has_festival_used     = false;
    bool has_auction_done      = false;
    bool moved_via_card        = false;
    int  consec_doubles        = 0;
    int  pnum                  = getPlayerNum(p);

    // Draw 1 skill card at start of turn (not in jail)
    if (p.getStatus() != "JAIL") drawSkillCardAtTurnStart(p);

    cout << "\n=== Giliran " << p.getUsername()
         << " (P" << pnum << ") "
         << "(Turn " << current_turn << "/" << max_turn << ") ===\n";
    formatter.printPlayerStatus(p);

    // Print player number legend
    cout << "Pemain: ";
    for (int i = 0; i < static_cast<int>(players.size()); i++)
        cout << "P" << (i+1) << "=" << players[i]->getUsername()
             << (players[i]->getStatus() == "BANKRUPT" ? "[BANGKRUT]" : "") << "  ";
    cout << "\n\n";

    // Remind player they can save BEFORE rolling (safe save point)
    cout << "[Tip: ketik SIMPAN <file> sekarang untuk menyimpan sebelum giliran dimulai]\n";

    while (!turn_ended && !game_over) {
        if (p.getStatus() == "BANKRUPT") {
            cout << p.getUsername() << " bangkrut! Giliran berakhir.\n";
            turn_ended = true;
            break;
        }
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
        } else if (cmd == "CETAK_PROFIL") {
            formatter.printPlayerStatus(p);
        } else if (cmd == "CETAK_LOG") {
            int n = -1;
            if (cin.peek() != '\n') {
                int tmp;
                if (cin >> tmp) n = tmp;
            }
            formatter.printLog(transaction_log, n);
        } else if (cmd == "SIMPAN") {
            string slot; cin >> slot;
            if (has_executed_action || has_rolled) {
                cout << "SIMPAN hanya boleh di awal giliran sebelum aksi/dadu apa pun.\n";
            } else {
                cmdSimpan(slot, false);
            }
        } else if (cmd == "GADAI") {
            string code; cin >> code;
            cmdGadai(p, code);
            has_executed_action = true;
        } else if (cmd == "TEBUS") {
            string code; cin >> code;
            cmdTebus(p, code);
            has_executed_action = true;
        } else if (cmd == "BANGUN") {
            cmdBangun(p, "");
            has_executed_action = true;
        } else if (cmd == "GUNAKAN_KEMAMPUAN") {
            if (has_rolled) {
                cout << "Kartu kemampuan hanya bisa digunakan SEBELUM melempar dadu.\n";
            } else if (p.getHandSize() == 0) {
                cout << "Anda tidak memiliki kartu kemampuan.\n";
            } else {
                cout << "\nDaftar Kartu Kemampuan Spesial Anda:\n";
                for (int i = 0; i < p.getHandSize(); i++) {
                    auto *c = p.getHandCard(i);
                    cout << "  " << (i + 1) << ". " << (c ? c->describe() : "?") << "\n";
                }
                cout << "  0. Batal\n";
                int idx = readInt("Pilih kartu yang ingin digunakan (0-" + to_string(p.getHandSize()) + "): ", 0, p.getHandSize());
                if (idx == 0) { cout << "Dibatalkan.\n"; }
                else {
                    cmdGunakanKemampuan(p, idx - 1);
                    has_executed_action = true;
                    // Moved via skill card — allow BELI
                    if (p.isSkillUsed()) moved_via_card = true;
                }
            }
        } else if (cmd == "DROP_KEMAMPUAN") {
            int idx; cin >> idx;
            cmdDropKemampuan(p, idx - 1);
            has_executed_action = true;

        // roll commands (pre-roll only)
        } else if ((cmd == "LEMPAR_DADU" || cmd == "ATUR_DADU") && !has_rolled) {
            bool manual = (cmd == "ATUR_DADU");
            int d1 = 0, d2 = 0;
            if (manual) {
                cin >> d1 >> d2;
                if (d1 < 1 || d1 > 6 || d2 < 1 || d2 > 6) {
                    cout << "Nilai dadu harus 1-6.\n";
                    continue;
                }
            }
            has_executed_action = true;

            if (p.getStatus() == "JAIL") {
                // 4th turn in jail: must pay fine before rolling
                if (jail_turns.find(&p) == jail_turns.end()) jail_turns[&p] = 0;
                if (jail_turns[&p] >= 3) {
                    // Forced fine payment
                    int fine = config.getJailFine();
                    if (p.getBalance() >= fine) {
                        p += -fine;
                        p.setStatus("ACTIVE");
                        cout << "Giliran ke-4 di penjara. Wajib bayar denda M" << fine << ".\n";
                        addLog(p, "BAYAR_DENDA", "Paksa keluar M" + to_string(fine));
                    } else {
                        cout << "Giliran ke-4 di penjara. Wajib bayar denda M" << fine
                             << " tapi saldo tidak cukup (M" << p.getBalance() << ")!\n";
                        cmdBangkrut(p);
                        turn_ended = true;
                        break;
                    }
                    // Now roll and move normally
                    if (manual) dice.setDice(d1, d2);
                    else        dice.throwDice();
                    cout << p.getUsername() << " melempar dadu: "
                         << dice.getDie1() << " + " << dice.getDie2()
                         << " = " << dice.getTotal() << "\n";
                    int jailTile = p.getCurrTile();
                    int newTile  = (jailTile + dice.getTotal()) % board.getTileCount();
                    p.setCurrTile(newTile);
                    applyGoSalary(p, jailTile);
                    jail_turns.erase(&p);
                    cmdCetakPapan();
                    applyLanding(p);
                    if (p.getStatus() == "BANKRUPT") { turn_ended = true; break; }
                    has_rolled = true;
                } else {
                    // Attempts 1-3: roll, stay unless double
                    if (manual) dice.setDice(d1, d2);
                    else        dice.throwDice();

                    bool isDouble = dice.isDouble();
                    cout << p.getUsername() << " melempar dadu: "
                         << dice.getDie1() << " + " << dice.getDie2()
                         << " = " << dice.getTotal()
                         << (isDouble ? " (GANDA)" : "") << "\n";

                    {
                        string diceDetail = "Lempar: " + to_string(dice.getDie1()) + "+"
                                          + to_string(dice.getDie2()) + "=" + to_string(dice.getTotal())
                                          + (isDouble ? " (double)" : "") + " [PENJARA]";
                        addLog(p, "DADU", diceDetail);
                    }
                    if (isDouble) {
                        // Double: exit jail and move
                        int jailTile = p.getCurrTile();
                        int newTile  = (jailTile + dice.getTotal()) % board.getTileCount();
                        p.setCurrTile(newTile);
                        applyGoSalary(p, jailTile);
                        p.setStatus("ACTIVE");
                        jail_turns.erase(&p);
                        cout << "Dadu ganda! Bebas dari penjara!\n";
                        addLog(p, "PENJARA", "Bebas via double");
                        cmdCetakPapan();
                        applyLanding(p);
                        if (p.getStatus() == "BANKRUPT") { turn_ended = true; break; }
                        has_rolled = true;
                    } else {
                        jail_turns[&p]++;
                        cout << p.getUsername() << " gagal keluar penjara (giliran ke-"
                             << jail_turns[&p] << "/3).\n";
                        addLog(p, "PENJARA", "Gagal keluar giliran " + to_string(jail_turns[&p]) + "/3");
                        has_rolled = true;
                        turn_ended = true;
                    }
                }
            } else {
                // Normal roll (not in jail)
                bool isDouble = rollAndMove(p, manual, d1, d2);
                has_rolled = true;
                {
                    string landName = tiles[p.getCurrTile()]->getTileName();
                    string landCode = tiles[p.getCurrTile()]->getTileCode();
                    string diceDetail = "Lempar: " + to_string(dice.getDie1()) + "+"
                                      + to_string(dice.getDie2()) + "=" + to_string(dice.getTotal())
                                      + (isDouble ? " (double)" : "")
                                      + " → " + landName + " (" + landCode + ")";
                    addLog(p, "DADU", diceDetail);
                }

                if (isDouble) {
                    consec_doubles++;
                    if (consec_doubles >= 3) {
                        // 3 doubles → go to jail BEFORE applying landing
                        int jailIdx = 0;
                        for (int i = 0; i < board.getTileCount(); i++)
                            if (tiles[i]->getTileType() == "JAIL") { jailIdx = i; break; }
                        JailManager::goToJail(p, jailIdx);
                        jail_turns[&p] = 0;
                        cout << "3 dadu ganda berturut-turut! Masuk penjara!\n";
                        addLog(p, "MASUK_PENJARA", "3x double berturut-turut");
                        cmdCetakPapan();
                        turn_ended = true;
                    } else {
                        cmdCetakPapan();
                        applyLanding(p);
                        if (p.getStatus() == "BANKRUPT") { turn_ended = true; break; }
                        addLog(p, "DOUBLE", "Giliran tambahan ke-" + to_string(consec_doubles));
                        cout << "\n[Dadu ganda dari lemparan ini - giliran tambahan setelah SELESAI]\n";
                        has_bonus_turn = true;
                    }
                } else {
                    cmdCetakPapan();
                    applyLanding(p);
                    if (p.getStatus() == "BANKRUPT") { turn_ended = true; }
                }
            }

        // post-roll commands
        } else if (cmd == "BELI") {
            if (!has_rolled && !moved_via_card) cout << "Lempar dadu terlebih dahulu.\n";
            else { cmdBeli(p); has_auction_done = true; }
        } else if (cmd == "BAYAR_SEWA") {
            // Sewa dibayar otomatis saat mendarat — tidak bisa dipanggil manual
            cout << "Bayar sewa dilakukan otomatis saat mendarat di properti.\n";
        } else if (cmd == "BAYAR_PAJAK") {
            cout << "Pajak dibayar otomatis saat mendarat.\n";
        } else if (cmd == "LELANG") {
            if (!has_rolled) cout << "Lempar dadu terlebih dahulu.\n";
            else { string code; cin >> code; cmdLelang(p, code); has_auction_done = true; }
        } else if (cmd == "FESTIVAL") {
            if (!has_rolled) {
                cout << "Lempar dadu terlebih dahulu.\n";
            } else if (has_festival_used) {
                cout << "Festival sudah digunakan pada giliran ini.\n";
            } else if (tiles[p.getCurrTile()]->getTileType() != "FESTIVAL") {
                cout << "Hanya bisa digunakan saat berdiri di petak Festival.\n";
            } else if (p.getOwnedProperties().empty()) {
                cout << "Anda tidak punya properti yang bisa difestivalkan.\n";
            } else {
                string code; cin >> code;
                cmdFestival(p, code, 0, 0);
                has_festival_used = true;
            }
        } else if (cmd == "BANTUAN" || cmd == "HELP") {
            cout << "\n=== Daftar Perintah Nimonspoli ===\n";
            cout << "  CETAK_PAPAN            - Tampilkan papan permainan\n";
            cout << "  CETAK_AKTA <kode>      - Tampilkan akta kepemilikan properti\n";
            cout << "  CETAK_PROPERTI         - Tampilkan daftar properti Anda\n";
            cout << "  CETAK_PROFIL          - Tampilkan profil pemain (uang, status, dll)\n";
            cout << "  CETAK_LOG [n]          - Tampilkan log transaksi (n = jumlah terakhir)\n";
            cout << "  LEMPAR_DADU            - Lempar dadu secara acak\n";
            cout << "  ATUR_DADU <d1> <d2>    - Atur nilai dadu (1-6)\n";
            cout << "  BELI                   - Beli properti (setelah mendarat)\n";
            cout << "  GADAI <kode>           - Gadaikan properti\n";
            cout << "  TEBUS <kode>           - Tebus properti yang digadaikan\n";
            cout << "  BANGUN                 - Bangun rumah/hotel\n";
            cout << "  LELANG <kode>          - Lelang properti\n";
            cout << "  FESTIVAL <kode>        - Aktifkan festival (di petak Festival)\n";
            cout << "  GUNAKAN_KEMAMPUAN      - Gunakan kartu kemampuan\n";
            cout << "  DROP_KEMAMPUAN <idx>   - Buang kartu kemampuan\n";
            cout << "  BAYAR_DENDA            - Bayar denda penjara (di penjara)\n";
            cout << "  SIMPAN <file>          - Simpan game\n";
            cout << "  SELESAI                - Akhiri giliran\n";
            cout << "  BANTUAN                - Tampilkan daftar perintah ini\n\n";
        } else if (cmd == "BANGKRUT") {
            cmdBangkrut(p);
            turn_ended = true;

        } else if (cmd == "SELESAI") {
            if (!has_rolled && !paid_fine_this_turn) {
                if (p.getStatus() == "JAIL") {
                    cout << "Anda dalam penjara! Harus LEMPAR_DADU untuk mencoba keluar atau BAYAR_DENDA.\n";
                } else {
                    cout << "Harus melempar dadu sebelum selesai.\n";
                }
            } else if (has_rolled && hasPendingAction(p) && !has_auction_done) {
                // Auto-trigger auction for declined STREET property
                Tile *t = tiles[p.getCurrTile()];
                auto *prop = dynamic_cast<PropertyTile*>(t);
                if (prop && prop->getTileType() == "STREET" && !prop->getTileOwner()) {
                    cout << "Anda tidak membeli properti. Lelang otomatis dimulai...\n";
                    vector<Player*> participants;
                    for (auto *pl : turn_order)
                        if (pl->getStatus() != "BANKRUPT") participants.push_back(pl);
                    formatter.printAuction();
                    interactiveLelang(nullptr, *prop, participants);
                    has_auction_done = true;
                    turn_ended = true;
                    if (has_bonus_turn && p.getStatus() != "JAIL" && p.getStatus() != "BANKRUPT") {
                        has_rolled = false;
                        has_bonus_turn = false;
                        turn_ended = false;
                        has_festival_used = false;
                        has_auction_done = false;
                        cout << "\n=== Giliran Tambahan (Bonus Dadu Ganda) "
                             << p.getUsername() << " (P" << pnum << ") ===\n";
                        formatter.printPlayerStatus(p);
                    }
                } else {
                    cout << "Anda masih memiliki kewajiban yang belum diselesaikan.\n";
                }
            } else {
                turn_ended = true;
                if (has_bonus_turn && p.getStatus() != "JAIL" && p.getStatus() != "BANKRUPT") {
                    has_rolled     = false;
                    has_bonus_turn = false;
                    turn_ended     = false;
                    has_festival_used = false;
                    has_auction_done  = false;
                    cout << "\n=== Giliran Tambahan (Bonus Dadu Ganda) "
                         << p.getUsername() << " (P" << pnum << ") ===\n";
                    formatter.printPlayerStatus(p);
                }
            }
        } else {
            cout << "Perintah tidak dikenal: " << cmd << "\n";
        }
    }
}

// ── botTurn ───────────────────────────────────────────────────────────────────
void Origami::botTurn(Player &p, int consecDoubles) {
    cout << "\n=== Giliran Bot " << p.getUsername()
         << " (Turn " << current_turn << "/" << max_turn << ") ===\n";

    // Draw skill card at start of turn (not in jail)
    if (p.getStatus() != "JAIL") drawSkillCardAtTurnStart(p);

    // jail handling
    if (p.getStatus() == "JAIL") {
        if (jail_turns.find(&p) == jail_turns.end()) jail_turns[&p] = 0;
        int fine = config.getJailFine();

        // 4th turn: must pay fine or go bankrupt
        if (jail_turns[&p] >= 3) {
            if (p.getBalance() >= fine) {
                p += -fine;
                p.setStatus("ACTIVE");
                jail_turns.erase(&p);
                cout << p.getUsername() << " (bot) WAJIB membayar denda M" << fine
                     << " di giliran ke-4 penjara.\n";
                addLog(p, "BAYAR_DENDA", "Paksa keluar M" + to_string(fine));
                // Roll and move
                bool isDouble = rollAndMove(p, false);
                applyLanding(p);
                if (p.getStatus() == "BANKRUPT") return;
                if (isDouble && p.getStatus() != "JAIL" && p.getStatus() != "BANKRUPT") {
                    cout << p.getUsername() << " (bot) mendapat giliran tambahan!\n";
                    botTurn(p, 0);
                }
            } else {
                cout << p.getUsername() << " (bot) tidak mampu bayar denda. Bangkrut.\n";
                cmdBangkrut(p);
            }
            return;
        }

        // Try rolling doubles — in jail, only move if double
        dice.throwDice();
        bool isDouble = dice.isDouble();
        cout << p.getUsername() << " (bot) melempar dadu di penjara: "
             << dice.getDie1() << " + " << dice.getDie2()
             << " = " << dice.getTotal()
             << (isDouble ? " (GANDA)" : "") << "\n";
        if (!isDouble) {
            jail_turns[&p]++;
            cout << p.getUsername() << " (bot) gagal keluar penjara (giliran "
                 << jail_turns[&p] << "/3).\n";
            addLog(p, "DADU", "Lempar: " + to_string(dice.getDie1()) + "+"
                   + to_string(dice.getDie2()) + " [PENJARA] gagal");
        } else {
            int jailTile = p.getCurrTile();
            int newTile  = (jailTile + dice.getTotal()) % board.getTileCount();
            p.setCurrTile(newTile);
            applyGoSalary(p, jailTile);
            p.setStatus("ACTIVE");
            jail_turns.erase(&p);
            cout << p.getUsername() << " (bot) keluar penjara (dadu ganda)!\n";
            addLog(p, "DADU", "Lempar: " + to_string(dice.getDie1()) + "+"
                   + to_string(dice.getDie2()) + " [PENJARA] keluar");
            applyLanding(p);
            if (p.getStatus() == "BANKRUPT") return;
        }
        return;
    }

    int newConsec = consecDoubles;
    bool isDouble = rollAndMove(p, false);

    if (isDouble) {
        newConsec++;
        if (newConsec >= 3) {
            int jailIdx = 0;
            for (int i = 0; i < board.getTileCount(); i++)
                if (tiles[i]->getTileType() == "JAIL") { jailIdx = i; break; }
            JailManager::goToJail(p, jailIdx);
            jail_turns[&p] = 0;
            cout << p.getUsername() << " (bot) 3 dadu ganda berturut-turut! Masuk penjara!\n";
            addLog(p, "MASUK_PENJARA", "3x double berturut-turut");
            return;
        }
    } else {
        newConsec = 0;
    }

    applyLanding(p);
    if (p.getStatus() == "BANKRUPT") return;

    // Bot: auto-select property for festival
    if (tiles[p.getCurrTile()]->getTileType() == "FESTIVAL") {
        auto myProps = p.getOwnedProperties();
        PropertyTile *bestProp = nullptr;
        for (auto *fp : myProps) {
            if (!fp->isMortgage()) {
                bestProp = fp;
                break;
            }
        }
        if (bestProp) {
            cout << p.getUsername() << " (bot) memilih " << bestProp->getTileName()
                 << " (" << bestProp->getTileCode() << ") untuk Festival.\n";
            cmdFestival(p, bestProp->getTileCode(), 0, 0);
            addLog(p, "FESTIVAL", bestProp->getTileCode());
        } else {
            cout << p.getUsername() << " (bot) tidak punya properti untuk Festival.\n";
        }
    }

    // Auto-buy STREET if balance >= 2x price
    auto *prop = dynamic_cast<PropertyTile*>(tiles[p.getCurrTile()]);
    if (prop && !prop->getTileOwner() && !prop->isMortgage()
        && prop->getTileType() == "STREET") {
        if (p.getBalance() >= prop->getBuyPrice() * 2) {
            cmdBeli(p);
        } else {
            // Decline → trigger auction
            cout << p.getUsername() << " (bot) menolak beli. Lelang otomatis.\n";
            vector<Player*> participants;
            for (auto *pl : turn_order)
                if (pl->getStatus() != "BANKRUPT") participants.push_back(pl);
            formatter.printAuction();
            interactiveLelang(nullptr, *prop, participants);
        }
    }

    // bonus turn on double
    if (isDouble && p.getStatus() != "JAIL" && p.getStatus() != "BANKRUPT") {
        cout << p.getUsername() << " (bot) mendapat giliran tambahan!\n";
        botTurn(p, newConsec);
    }
}

// ── nextTurn ──────────────────────────────────────────────────────────────────
void Origami::nextTurn() {
    // decrement festival durations — only count full rounds for the owner
    // Festival "3 giliran" means 3 of the OWNER's turns, not 3 arbitrary turns
    Player *currentPlayer = turn_order[turn_order_idx];

    for (auto *t : tiles) {
        auto *prop = dynamic_cast<PropertyTile*>(t);
        if (!prop || prop->getFestivalDuration() <= 0) continue;
        Player *owner = prop->getTileOwner();
        // Decrement duration only when the festival owner's turn ends
        if (!owner || owner == currentPlayer) {
            int newDur = prop->getFestivalDuration() - 1;
            if (newDur <= 0) {
                prop->setFestivalState(1, 0);
            } else {
                prop->setFestivalState(prop->getFestivalMultiplier(), newDur);
            }
        }
    }

    // clear per-turn modifiers for current player
    if (!turn_order.empty())
        turn_order[turn_order_idx]->clearTurnModifiers();

    // advance idx; if we wrap around, increment turn counter.
    // Skip BANKRUPT players (each skip may also wrap and increment).
    int sz = static_cast<int>(turn_order.size());
    int attempts = 0;
    do {
        int prev = turn_order_idx;
        turn_order_idx = (turn_order_idx + 1) % sz;
        if (turn_order_idx <= prev) current_turn++;
        attempts++;
    } while (turn_order[turn_order_idx]->getStatus() == "BANKRUPT" && attempts <= sz);

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
        events.flushEventsTo(cout);
        game_over_by_bankruptcy = true;
        game_over = true;
        return;
    }
    // Bankruptcy mode: MAX_TURN < 1 means play until only one player left
    if (max_turn < 1) return;
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
    formatter.printWin(copies, game_over_by_bankruptcy);
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
        if (savePath.find('/') == string::npos && savePath.find('\\') == string::npos) {
            savePath = "save/" + savePath;
        }
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
