#include "include/core/CardProcessor.hpp"
#include "include/core/GameState.hpp"
#include "include/core/BankruptcyProcessor.hpp"
#include "include/core/CardManager.hpp"
#include "include/core/FestivalManager.hpp"
#include "include/core/LandingProcessor.hpp"
#include "include/core/PropertyManager.hpp"
#include "include/core/RentManager.hpp"
#include "include/core/RentCollector.hpp"
#include "include/core/SkillCardManager.hpp"
#include "include/models/player/Player.hpp"
#include "include/models/player/Bot.hpp"
#include "include/models/tiles/PropertyTile.hpp"
#include "include/models/card/SpecialPowerCard.hpp"

#include <iostream>
#include <limits>
#include <random>
#include <string>
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
            cout << "  Input tidak valid.\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }

    auto centerText = [](const string &s, int w) -> string {
        if ((int)s.size() >= w) {
            return s.substr(0, w);
        }
        int lp = (w - (int)s.size()) / 2;
        return string(lp, ' ') + s + string(w - (int)s.size() - lp, ' ');
    };

    auto leftPad = [](const string &s, int w) -> string {
        if ((int)s.size() >= w) {
            return s.substr(0, w);
        }
        return s + string(w - (int)s.size(), ' ');
    };
}

CardProcessor::CardProcessor(GameState &s, BankruptcyProcessor &bp)
    : state(s), bankruptcy(bp) {}

void CardProcessor::drawAndResolveChance(Player &p) {
    ChanceCard *card = state.chance_deck.draw();
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
        CardManager::resolveCardEffect(*card, p, state.players);
        state.chance_deck.discardCard(card);

        if (p.getStatus() == "JAIL" && state.jail_turns.find(&p) == state.jail_turns.end()) {
            state.jail_turns[&p] = 0;
        }

        state.addLog(p, "KARTU", "Kesempatan: " + card->describe());
    }
}

void CardProcessor::drawAndResolveCommunityChest(Player &p) {
    CommunityChestCard *card = state.community_deck.draw();
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
            CardManager::resolveCardEffect(*card, p, state.players);
            if (p.getBalance() < 0) {
                int debt = -p.getBalance();
                int maxLiq = PropertyManager::calculateMaxLiquidation(p);
                if (maxLiq + p.getBalance() < 0) {
                    cout << p.getUsername() << " tidak mampu membayar tagihan kartu Dana Umum! Bangkrut!\n";
                    bankruptcy.processBankruptcy(p);
                } else {
                    cout << "Saldo habis setelah efek kartu. Lakukan likuidasi aset.\n";
                }
            }
        }
        state.community_deck.discardCard(card);
        state.addLog(p, "KARTU", "Dana Umum: " + card->describe());
    }
}

void CardProcessor::drawSkillCardAtTurnStart(Player &p) {
    static const vector<string> kinds = {"MOVE", "DISCOUNT", "SHIELD", "TELEPORT", "LASSO", "DEMOLITION"};
    static mt19937 rng(random_device{}());
    int boardSize = state.board.getTileCount();

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

    state.skill_cards.push_back(card);
    cout << p.getUsername() << " mendapatkan kartu kemampuan baru: " << card->describe() << "\n";
    p.addHandCard(card);
    state.addLog(p, "DRAW_KEMAMPUAN", kind + " - " + card->describe());

    if (p.getHandSize() > 3) {
        cout << "PERINGATAN: " << p.getUsername() << " memiliki " << p.getHandSize() << " kartu (Maks 3). Wajib buang 1.\n";
        bool isBot = (dynamic_cast<Bot*>(&p) != nullptr);

        if (isBot) {
            int dropIdx = p.getHandSize() - 1;
            cout << p.getUsername() << " (bot) membuang kartu ke-" << (dropIdx + 1) << ".\n";
            p.removeHandCard(dropIdx);
            state.addLog(p, "DROP_KEMAMPUAN", to_string(dropIdx + 1));
        } else {
            for (int i = 0; i < p.getHandSize(); i++) {
                auto *c = p.getHandCard(i);
                cout << "  " << (i + 1) << ". " << (c ? c->describe() : "(?)") << "\n";
            }
            int idx = readInt("Pilih nomor kartu yang ingin dibuang: ", 1, p.getHandSize());
            p.removeHandCard(idx - 1);
            state.addLog(p, "DROP_KEMAMPUAN", to_string(idx));
        }
    }
}

void CardProcessor::cmdGunakanKemampuan(Player &p, int index) {
    SpecialPowerCard *card = p.getHandCard(index);
    if (!card) { 
        cout << "Kartu tidak ditemukan.\n"; 
        return; 
    }
    if (p.isSkillUsed()) {
        cout << "Kamu sudah menggunakan kartu kemampuan pada giliran ini!\n";
        return;
    }

    int before = p.getCurrTile();
    string cardType = card->getCardType();

    if (cardType == "TELEPORT") {
        cout << "TeleportCard: Pindah ke petak mana saja.\n";
        state.formatter.printBoard(state.board, state.players, state.current_turn, state.max_turn);
        cout << "Daftar petak:\n";
        for (int i = 0; i < state.board.getTileCount(); i++) {
            Tile *t = state.tiles[i];
            cout << "  " << i << ". [" << t->getTileCode() << "] " << t->getTileName() << "\n";
        }
        int target = readInt("Pilih nomor petak tujuan: ", 0, state.board.getTileCount() - 1);
        p.removeHandCard(index);
        SkillCardManager::applyTeleport(p, target, state.board.getTileCount());
        p.setSkillUsed(true);
        cout << p.getUsername() << " berteleportasi ke [" << state.tiles[target]->getTileCode() << "] " << state.tiles[target]->getTileName() << "!\n";
        state.addLog(p, "GUNAKAN_KEMAMPUAN", "TeleportCard -> " + string(state.tiles[target]->getTileCode()));
    } else if (cardType == "LASSO") {
        vector<Player*> others;
        for (auto *pl : state.players) {
            if (pl != &p && pl->getStatus() != "BANKRUPT") {
                others.push_back(pl);
            }
        }
        if (others.empty()) { 
            cout << "Tidak ada pemain lain.\n"; 
            return; 
        }
        cout << "LassoCard: Tarik 1 pemain lain ke petak Anda.\n";
        for (int i = 0; i < static_cast<int>(others.size()); i++) {
            cout << "  " << (i + 1) << ". " << others[i]->getUsername() << "\n";
        }
        int target = readInt("Pilih nomor pemain (0 untuk batal): ", 0, static_cast<int>(others.size()));
        if (target == 0) return;
        p.removeHandCard(index);
        Player *targetPlayer = others[target - 1];
        int myTile = p.getCurrTile();
        SkillCardManager::applyLasso(p, *targetPlayer, myTile);
        p.setSkillUsed(true);
        cout << targetPlayer->getUsername() << " ditarik ke petak " << myTile << "!\n";
        state.addLog(p, "GUNAKAN_KEMAMPUAN", "LassoCard -> " + targetPlayer->getUsername());
    } else if (cardType == "DEMOLITION") {
        vector<pair<Player*, vector<PropertyTile*>>> opponentsWithBuildings;
        for (auto *pl : state.players) {
            if (pl == &p || pl->getStatus() == "BANKRUPT") continue;
            vector<PropertyTile*> built;
            for (auto *prop : pl->getOwnedProperties()) {
                if (prop->getLevel() > 0) {
                    built.push_back(prop);
                }
            }
            if (!built.empty()) {
                opponentsWithBuildings.push_back({pl, built});
            }
        }
        if (opponentsWithBuildings.empty()) { 
            cout << "Tidak ada bangunan lawan.\n"; 
            return; 
        }
        cout << "DemolitionCard: Hancurkan 1 bangunan lawan.\n";
        for (int i = 0; i < static_cast<int>(opponentsWithBuildings.size()); i++) {
            auto &[pl, built] = opponentsWithBuildings[i];
            cout << "  " << (i + 1) << ". " << pl->getUsername() << ":\n";
            for (auto *prop : built) {
                string lvlStr = (prop->getLevel() >= 5) ? "Hotel" : to_string(prop->getLevel()) + " rumah";
                cout << "      [" << prop->getTileCode() << "] " << prop->getTileName() << " - " << lvlStr << "\n";
            }
        }
        int oppIdx = readInt("Pilih nomor pemain (0 untuk batal): ", 0, static_cast<int>(opponentsWithBuildings.size()));
        if (oppIdx == 0) return;
        auto &[targetPlayer, built] = opponentsWithBuildings[oppIdx - 1];
        for (int j = 0; j < static_cast<int>(built.size()); j++) {
            string lvlStr = (built[j]->getLevel() >= 5) ? "Hotel" : to_string(built[j]->getLevel()) + " rumah";
            cout << "  " << (j + 1) << ". [" << built[j]->getTileCode() << "] " << built[j]->getTileName() << " - " << lvlStr << "\n";
        }
        int propIdx = readInt("Pilih bangunan (0 untuk batal): ", 0, static_cast<int>(built.size()));
        if (propIdx == 0) return;
        p.removeHandCard(index);
        PropertyTile *targetProp = built[propIdx - 1];
        SkillCardManager::applyDemolition(*targetProp);
        cout << "Bangunan di [" << targetProp->getTileCode() << "] dihancurkan!\n";
        p.setSkillUsed(true);
        state.addLog(p, "GUNAKAN_KEMAMPUAN", "DemolitionCard -> " + targetProp->getTileCode());
    } else {
        SkillCardManager::applyGeneric(p, card);
        p.removeHandCard(index);
        p.setSkillUsed(true);
        if (cardType == "SHIELD") {
            cout << "ShieldCard diaktifkan!\n";
        } else if (cardType == "DISCOUNT") {
            cout << "DiscountCard diaktifkan! " << card->describe() << "\n";
        } else if (cardType == "MOVE") {
            cout << "MoveCard digunakan! " << card->describe() << "\n";
        } else {
            cout << "Kartu kemampuan digunakan: " << card->describe() << "\n";
        }
        state.addLog(p, "GUNAKAN_KEMAMPUAN", card->describe());
    }
}

void CardProcessor::cmdFestival(Player &p, const string &code) {
    if (state.tiles[p.getCurrTile()]->getTileType() != "FESTIVAL") {
        cout << "Hanya bisa di petak Festival.\n"; 
        return;
    }
    Tile *t = state.board.getTileByCode(code);
    if (!t) { 
        cout << "Kode '" << code << "' tidak ditemukan.\n"; 
        return; 
    }
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop || prop->getTileOwner() != &p) { 
        cout << "Bukan properti Anda.\n"; 
        return; 
    }

    int curMult = prop->getFestivalMultiplier();
    int newMult;
    bool atMax = false;

    if (curMult <= 1) newMult = 2;
    else if (curMult == 2) newMult = 4;
    else if (curMult == 4) newMult = 8;
    else { newMult = 8; atMax = true; }

    int oldRent = RentManager::computeRent(*prop, state.dice.getTotal(), state.config, state.tiles);
    prop->setFestivalState(newMult, 3);
    int newRent = RentManager::computeRent(*prop, state.dice.getTotal(), state.config, state.tiles);

    if (atMax) {
        cout << "Efek Festival di [" << code << "] " << prop->getTileName()
             << " sudah maksimum (x8). Durasi reset menjadi 3.\n";
    } else {
        cout << "Festival di [" << code << "] " << prop->getTileName()
             << "! Sewa M" << oldRent << " → M" << newRent
             << " (x" << newMult << " selama 3 giliran).\n";
    }
    state.addLog(p, "FESTIVAL", code + ": x" + to_string(newMult) + " 3 giliran");
}