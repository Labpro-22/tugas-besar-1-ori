#include "include/core/RentCollector.hpp"
#include "include/core/GameState.hpp"
#include "include/core/BankruptcyProcessor.hpp"
#include "include/core/PropertyManager.hpp"
#include "include/core/RentManager.hpp"
#include "include/models/player/Player.hpp"
#include "include/models/player/Bot.hpp"
#include "include/models/tiles/PropertyTile.hpp"

#include <iostream>
#include <limits>
#include <string>

using namespace std;

RentCollector::RentCollector(GameState &s, BankruptcyProcessor &bp)
    : state(s), bankruptcy(bp) {}

void RentCollector::autoPayRent(Player &p, PropertyTile &prop) {
    Player *owner = prop.getTileOwner();
    
    if (!owner || owner == &p) {
        return;
    }

    int rent = RentManager::computeRent(prop, state.dice.getTotal(), state.config, state.tiles);
    
    if (rent <= 0) {
        return;
    }

    if (p.getDiscountActive() > 0.0f) {
        rent = static_cast<int>(rent * (1.0f - p.getDiscountActive()));
    }

    bool isBot = (dynamic_cast<Bot*>(&p) != nullptr);

    if (p.getBalance() < rent) {
        int maxLiq = PropertyManager::calculateMaxLiquidation(p);
        
        if (maxLiq < rent) {
            cout << "Tidak cukup aset untuk membayar sewa. Bangkrut!\n";
            bankruptcy.processBankruptcy(p, owner);
            return;
        }

        if (isBot) {
            if (PropertyManager::autoLiquidate(p, state.board, rent)) {
                p += -rent;
                owner->operator+=(rent);
                
                cout << p.getUsername() << " (bot) membayar sewa M" << rent << " ke " << owner->getUsername() << ".\n";
                state.addLog(p, "SEWA", prop.getTileCode() + " M" + to_string(rent));
            } else {
                bankruptcy.processBankruptcy(p, owner);
            }
        } else {
            cout << "Saldo tidak cukup untuk membayar sewa M" << rent << "!\n";
            recoverForRent(p, prop);
        }
        return;
    }

    p += -rent;
    owner->operator+=(rent);

    cout << p.getUsername() << " membayar sewa M" << rent << " ke " << owner->getUsername() << ".\n";
    cout << "  Saldo " << p.getUsername() << ": M" << p.getBalance()
         << " | Saldo " << owner->getUsername() << ": M" << owner->getBalance() << "\n";

    int lvl = prop.getLevel();
    string lvlStr = (lvl == 0) ? "unimproved" : (lvl >= 5 ? "hotel" : to_string(lvl) + " rumah");
    string rentDetail = "Bayar M" + to_string(rent) + " ke " + owner->getUsername()
                       + " (" + prop.getTileCode() + ", " + lvlStr;

    if (prop.getFestivalDuration() > 0) {
        rentDetail += ", festival x" + to_string(prop.getFestivalMultiplier());
    }
    rentDetail += ")";

    state.addLog(p, "SEWA", rentDetail);
}

void RentCollector::recoverForRent(Player &p, PropertyTile &prop) {
    Player *owner = prop.getTileOwner();
    int rent = RentManager::computeRent(prop, state.dice.getTotal(), state.config, state.tiles);

    if (PropertyManager::calculateMaxLiquidation(p) < rent) {
        cout << "Tidak cukup aset untuk membayar sewa. Bangkrut!\n";
        bankruptcy.processBankruptcy(p, owner);
        return;
    }

    while (p.getBalance() < rent && p.getStatus() != "BANKRUPT") {
        int maxCanPay = PropertyManager::calculateMaxLiquidation(p);
        cout << "Saldo: M" << p.getBalance() << " / Butuh: M" << rent << "\n";

        if (maxCanPay < rent) {
            bankruptcy.processBankruptcy(p, owner);
            return;
        }

        cout << "Gadaikan atau lelang properti Anda dulu:\n";
        cout << "  GADAI <kode> / LELANG <kode> / CETAK_PROPERTI / CETAK_AKTA <kode>\n> ";
        
        string cmd;
        if (!(cin >> cmd)) { 
            bankruptcy.processBankruptcy(p, owner); 
            return; 
        }

        for (auto &c : cmd) {
            c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        }

        if (cmd == "GADAI") {
            string code; cin >> code;
            bankruptcy.cmdGadai(p, code);
        } else if (cmd == "LELANG") {
            string code; cin >> code;
            bankruptcy.cmdLelang(p, code);
        } else if (cmd == "CETAK_PROPERTI") {
            state.formatter.printProperty(p, state.board);
        } else if (cmd == "CETAK_AKTA") {
            string code; cin >> code;
            Tile *t = state.board.getTileByCode(code);
            if (t) { 
                auto *pr = dynamic_cast<PropertyTile*>(t);
                if (pr) {
                    state.formatter.printAkta(*pr, state.board, state.config);
                }
            }
        } else { 
            cout << "Perintah tidak dikenali.\n"; 
        }
    }

    if (p.getStatus() == "BANKRUPT") {
        return;
    }

    if (owner) {
        p += -rent;
        owner->operator+=(rent);
        cout << p.getUsername() << " membayar sewa M" << rent << " ke " << owner->getUsername() << ".\n";
        state.addLog(p, "SEWA", prop.getTileCode() + " M" + to_string(rent));
    }
}

void RentCollector::recoverForDebt(Player &p, int amount, Player *creditor, const string &reason) {
    if (PropertyManager::calculateMaxLiquidation(p) < amount) {
        cout << "Tidak cukup aset untuk membayar " << reason << ". Bangkrut!\n";
        bankruptcy.processBankruptcy(p, creditor);
        return;
    }

    while (p.getBalance() < amount && p.getStatus() != "BANKRUPT") {
        int maxCanPay = PropertyManager::calculateMaxLiquidation(p);
        cout << "Saldo: M" << p.getBalance() << " / Butuh: M" << amount << " (" << reason << ")\n";

        if (maxCanPay < amount) {
            bankruptcy.processBankruptcy(p, creditor);
            return;
        }

        cout << "Gadaikan atau lelang properti Anda dulu:\n> ";
        
        string cmd;
        if (!(cin >> cmd)) { 
            bankruptcy.processBankruptcy(p, creditor); 
            return; 
        }

        for (auto &c : cmd) {
            c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        }

        if (cmd == "GADAI") { 
            string code; cin >> code; 
            bankruptcy.cmdGadai(p, code); 
        } else if (cmd == "LELANG") { 
            string code; cin >> code; 
            bankruptcy.cmdLelang(p, code); 
        } else if (cmd == "CETAK_PROPERTI") { 
            state.formatter.printProperty(p, state.board); 
        } else { 
            cout << "Perintah tidak dikenali.\n"; 
        }
    }

    if (p.getStatus() == "BANKRUPT") {
        return;
    }

    p += -amount;
    if (creditor && creditor != &p) {
        creditor->operator+=(amount);
        cout << p.getUsername() << " membayar M" << amount << " ke " << creditor->getUsername() << " (" << reason << ").\n";
    } else {
        cout << p.getUsername() << " membayar M" << amount << " (" << reason << ").\n";
    }
}