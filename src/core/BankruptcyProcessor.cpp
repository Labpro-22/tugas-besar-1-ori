#include "include/utils/exceptions/NoPropertyException.hpp"
#include "include/utils/exceptions/UnablePayBuildException.hpp"

#include "include/core/BankruptcyProcessor.hpp"
#include "include/core/GameState.hpp"
#include "include/core/LandingProcessor.hpp"
#include "include/core/PropertyManager.hpp"
#include "include/core/AuctionManager.hpp"
#include "include/core/JailManager.hpp"
#include "include/models/player/Player.hpp"
#include "include/models/player/Bot.hpp"
#include "include/models/tiles/PropertyTile.hpp"

#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include <tuple>

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

    auto padRight = [](const string &str, int width) -> string {
        if ((int)str.length() >= width) {
            return str.substr(0, width);
        }
        return str + string(width - str.length(), ' ');
    };
}

BankruptcyProcessor::BankruptcyProcessor(GameState &s, LandingProcessor &lp)
    : state(s), landing(lp) {}

void BankruptcyProcessor::processBankruptcy(Player &p, Player *creditor) {
    if (creditor && creditor != &p) {
        // Sell all buildings back to bank at half price (cash goes to debtor)
        auto props = p.getOwnedProperties();
        for (auto *prop : props) {
            if (!prop) continue;
            int lvl = prop->getLevel();
            if (lvl > 0) {
                int refund = (lvl >= 5)
                    ? (prop->getHotelCost() / 2)
                    : ((prop->getHouseCost() * lvl) / 2);
                if (refund > 0) p += refund;
                prop->setLevel(0);
                cout << "Bangunan di " << prop->getTileCode() << " dijual ke Bank seharga M" << refund << ".\n";
            }
        }

        // Transfer all remaining cash to creditor
        int cash = p.getBalance();
        if (cash > 0) {
            p += -cash;
            *creditor += cash;
            cout << "Uang sisa M" << cash << " diserahkan ke " << creditor->getUsername() << ".\n";
        }

        // Transfer property ownership to creditor (keep mortgage status)
        std::vector<std::string> groupsToRecompute;
        auto propsToTransfer = p.getOwnedProperties();
        for (auto *prop : propsToTransfer) {
            if (!prop) continue;
            std::string cg = prop->getColorGroup();
            std::string code = prop->getTileCode();
            prop->setMonopolized(false);
            prop->setFestivalState(1, 0);
            p.removeOwnedProperty(code);
            creditor->addOwnedProperty(prop);
            groupsToRecompute.push_back(cg);
            cout << "Properti " << code << " dialihkan ke " << creditor->getUsername() << ".\n";
        }

        p.setStatus("BANKRUPT");
        state.events.processBankruptcy(p, creditor);
        state.events.flushEventsTo(cout);

        for (const auto &cg : groupsToRecompute) {
            state.recomputeMonopolyForGroup(cg);
        }
    } else {
        p.setStatus("BANKRUPT");
        int cash = p.getBalance();

        if (cash > 0) {
            p += -cash;
            cout << "Uang sisa M" << cash << " diserahkan ke Bank.\n";
        }

        auto props = p.getOwnedProperties();
        for (auto *prop : props) {
            if (!prop) continue;
            prop->setLevel(0);
            prop->setMortgageStatus(false);
            prop->setMonopolized(false);
            prop->setFestivalState(1, 0);
            p.removeOwnedProperty(prop->getTileCode());
            state.recomputeMonopolyForGroup(prop->getColorGroup());

#ifdef GUI_MODE
            state.bankruptAuctionQueue.push_back(prop);
#else
            cout << "→ Melelang " << prop->getTileName() << " (" << prop->getTileCode() << ")...\n";
            AuctionManager::interactiveAuction(nullptr, *prop, state.turn_order, &p);
#endif
        }
    }
    
    cout << p.getUsername() << " bangkrut!\n";
    state.addLog(p, "BANGKRUT", "");
}

void BankruptcyProcessor::cmdGadai(Player &p, const string &code) {
    Tile *t = state.board.getTileByCode(code);
    if (!t) {
        cout << "Kode tidak ditemukan.\n"; 
        return; 
    }
    
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop || prop->getTileOwner() != &p) {
        cout << "Bukan properti Anda.\n"; 
        return; 
    }
    
    if (prop->isMortgage()) {
        cout << "Sudah digadaikan.\n"; 
        return; 
    }
    
    string cg = prop->getColorGroup();
    if (prop->getTileType() == "STREET") {
        auto group = state.board.getPropertiesByColorGroup(cg);
        bool anyBuilding = false;
        
        for (auto *gp : group) {
            if (gp->getLevel() > 0) {
                anyBuilding = true; 
                break; 
            }
        }
        
        if (anyBuilding) {
            cout << prop->getTileName() << " tidak dapat digadaikan!\n";
            cout << "Masih ada bangunan di color group [" << cg << "].\n";
            
            bool isBot = (dynamic_cast<Bot*>(&p) != nullptr);
            char confirm = 'Y';
            if (!isBot) {
                confirm = readChar("Jual semua bangunan color group [" + cg + "]? (y/n): ", "YN");
            }
            
            if (confirm == 'N') {
                cout << "Penggadaian dibatalkan.\n"; 
                return; 
            }
            
            vector<tuple<string,int,int>> details;
            PropertyManager::sellAllBuildingsInGroup(p, state.board, cg, &details);
            
            for (auto &d : details) {
                string c; int refund, lvl; 
                tie(c, refund, lvl) = d;
                cout << "Bangunan " << c << " level " << lvl << " terjual. Diterima M" << refund << ".\n";
                state.addLog(p, "JUAL_BANGUNAN", c + " M" + to_string(refund));
            }
        }
    }
    
    try {
        PropertyManager::mortgage(p, *prop);
    } catch (const exception &e) {
        cout << e.what() << "\n"; 
        return; 
    }
    
    state.recomputeMonopolyForGroup(cg);
    cout << "Properti " << code << " berhasil digadaikan. Diterima M" << prop->getMortgageValue() << ".\n";
    cout << "  Saldo " << p.getUsername() << ": M" << p.getBalance() << "\n";
    state.addLog(p, "GADAI", code + " M" + to_string(prop->getMortgageValue()));
}

void BankruptcyProcessor::cmdTebus(Player &p, const string &code) {
    Tile *t = state.board.getTileByCode(code);
    if (!t) {
        cout << "Kode tidak ditemukan.\n"; 
        return; 
    }
    
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop || prop->getTileOwner() != &p || !prop->isMortgage()) {
        cout << "Properti tidak dapat ditebus.\n"; 
        return;
    }
    
    int cost = prop->getBuyPrice();
    if (cost <= 0) {
        cost = prop->getMortgageValue() + (prop->getMortgageValue() / 2);
    }
    
    if (cost <= 0) {
        cout << "Tidak dapat ditebus.\n"; 
        return; 
    }
    
    if (p.getBalance() < cost) {
        cout << "Saldo tidak cukup (perlu M" << cost << ").\n"; 
        return; 
    }
    
    p += -cost;
    prop->setMortgageStatus(false);
    cout << "Properti " << code << " ditebus seharga M" << cost << ".\n";
    cout << "  Saldo " << p.getUsername() << ": M" << p.getBalance() << "\n";
    state.addLog(p, "TEBUS", code + " M" + to_string(cost));
}

void BankruptcyProcessor::cmdBangun(Player &p, const string &code) {
    if (code.empty()) {
        auto eligibleGroups = PropertyManager::getBuildableOptions(p, state.board);
        if (eligibleGroups.empty()) {
            cout << "Tidak ada color group yang memenuhi syarat untuk dibangun.\n";
            return;
        }
        
        cout << "\n=== Color Group yang Memenuhi Syarat ===\n";
        vector<string> groupNames;
        int idx = 1;
        for (auto &[groupName, props] : eligibleGroups) {
            groupNames.push_back(groupName);
            cout << idx << ". [" << groupName << "]\n";
            for (auto &opt : props) {
                cout << "   - " << padRight(opt.prop->getTileName() + " (" + opt.prop->getTileCode() + ")", 25)
                     << " : " << padRight(opt.levelStr, 12) << " (Harga: M" << opt.cost << ")\n";
            }
            idx++;
        }
        
        cout << "\nUang kamu saat ini: M" << p.getBalance() << "\n";
        int choice = readInt("Pilih nomor color group (0 untuk batal): ", 0, static_cast<int>(groupNames.size()));
        if (choice == 0) return;
        
        string selectedGroup = groupNames[choice - 1];
        auto &groupProps = eligibleGroups[selectedGroup];
        cout << "\nColor group [" << selectedGroup << "]:\n";
        idx = 1;
        vector<PropertyTile*> buildableProps;
        for (auto &opt : groupProps) {
            if (opt.prop->getLevel() >= 5) continue;
            string marker;
            if (opt.prop->getLevel() == 4) marker = " <- siap upgrade ke hotel";
            
            cout << idx << ". " << padRight(opt.prop->getTileName() + " (" + opt.prop->getTileCode() + ")", 25)
                 << " : " << opt.levelStr;
            if (!marker.empty()) cout << marker;
            cout << "\n";
            
            buildableProps.push_back(opt.prop);
            idx++;
        }
        
        if (buildableProps.empty()) {
            cout << "Tidak ada petak yang dapat dibangun.\n"; 
            return; 
        }
        
        int propChoice = readInt("Pilih petak (0 untuk batal): ", 0, static_cast<int>(buildableProps.size()));
        if (propChoice == 0) return;
        
        PropertyTile *selectedProp = buildableProps[propChoice - 1];
        int currentLvl = selectedProp->getLevel();
        int cost = (currentLvl >= 4) ? selectedProp->getHotelCost() : selectedProp->getHouseCost();
        
        if (currentLvl >= 4) {
            char confirm = readChar("Upgrade ke hotel? Biaya: M" + to_string(cost) + " (y/n): ", "YN");
            if (confirm == 'N') return;
        }
        
        try {
            PropertyManager::build(p, *selectedProp);
        } catch (const UnablePayBuildException&) {
            cout << "Saldo tidak cukup.\n"; 
            return; 
        } catch (const exception &e) {
            cout << e.what() << "\n"; 
            return; 
        }
        
        string buildType = (currentLvl >= 4) ? "Hotel" : "1 rumah";
        cout << "Kamu membangun " << buildType << " di " << selectedProp->getTileName()
             << ". Biaya: M" << cost << "\n";
        cout << "Uang tersisa: M" << p.getBalance() << "\n";
        state.addLog(p, "BANGUN", selectedProp->getTileCode());
        return;
    }
    
    Tile *t = state.board.getTileByCode(code);
    if (!t) {
        cout << "Kode tidak ditemukan.\n"; 
        return; 
    }
    
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop || prop->getTileOwner() != &p) {
        cout << "Bukan properti Anda.\n"; 
        return; 
    }
    
    if (prop->getTileType() != "STREET") {
        cout << "Hanya STREET yang dapat dibangun.\n"; 
        return; 
    }
    
    string colorGroup = prop->getColorGroup();
    if (!PropertyManager::hasMonopoly(p, state.board, colorGroup)) {
        cout << "Belum memonopoli color group [" << colorGroup << "].\n"; 
        return; 
    }
    
    if (prop->getLevel() >= 5) {
        cout << "Sudah maksimal (Hotel).\n"; 
        return; 
    }
    
    int currentLvl = prop->getLevel();
    int minLvl = PropertyManager::getMinLevelInGroup(state.board, colorGroup);
    
    if (currentLvl > minLvl) {
        cout << "Bangunan harus merata.\n"; 
        return; 
    }
    
    if (currentLvl == 4 && !PropertyManager::allReadyForHotel(state.board, colorGroup)) {
        cout << "Semua petak harus punya 4 rumah dulu.\n"; 
        return; 
    }
    
    try {
        PropertyManager::build(p, *prop);
    } catch (const UnablePayBuildException&) {
        cout << "Saldo tidak cukup.\n"; 
        return; 
    } catch (const exception &e) {
        cout << e.what() << "\n"; 
        return; 
    }
    
    string buildType = (currentLvl >= 4) ? "Hotel" : "1 rumah";
    cout << "Kamu membangun " << buildType << " di " << prop->getTileName() << ".\n";
    state.addLog(p, "BANGUN", code);
}

void BankruptcyProcessor::cmdLelang(Player &p, const string &code) {
    Tile *t = state.board.getTileByCode(code);
    if (!t) {
        cout << "Kode tidak ditemukan.\n"; 
        return; 
    }
    
    auto *prop = dynamic_cast<PropertyTile*>(t);
    if (!prop) {
        cout << "Bukan properti.\n"; 
        return; 
    }
    
    Player *owner = prop->getTileOwner();
    if (!owner) {
        if (p.getCurrTile() != state.board.getTileIndex(code)) {
            cout << "Hanya bisa melelang properti bank saat berdiri di atasnya.\n"; 
            return; 
        }
        state.formatter.printAuction();
        auto res = AuctionManager::interactiveAuction(nullptr, *prop, state.turn_order, &p);
        if (res.sold) {
            state.recomputeMonopolyForGroup(prop->getColorGroup());
        }
    } else if (owner == &p) {
        state.formatter.printAuction();
        auto res = AuctionManager::interactiveAuction(&p, *prop, state.turn_order, &p);
        if (res.sold) {
            state.recomputeMonopolyForGroup(prop->getColorGroup());
        }
        state.addLog(p, "LELANG", prop->getTileCode() + " -> " + (res.winner ? res.winner->getUsername() : "none") + " M" + to_string(res.final_bid));
    } else {
        cout << "Hanya bisa melelang properti milik sendiri atau bank.\n";
    }
}

void BankruptcyProcessor::handleAutoAuction(Player &p) {
    state.formatter.printAuction();
    auto *prop = dynamic_cast<PropertyTile*>(state.tiles[p.getCurrTile()]);
    if (!prop) {
        return;
    }
    auto res = AuctionManager::interactiveAuction(nullptr, *prop, state.turn_order, &p);
    if (res.sold) {
        state.recomputeMonopolyForGroup(prop->getColorGroup());
    }
    state.addLog(p, "LELANG", prop->getTileCode() + " -> lelang otomatis");
}

void BankruptcyProcessor::cmdBayarDenda(Player &p) {
    if (p.getStatus() != "JAIL") {
        cout << "Anda tidak dalam penjara.\n"; 
        return; 
    }
    
    int fine = state.config.getJailFine();
    if (p.getBalance() < fine) {
        cout << "Saldo tidak cukup (perlu M" << fine << ").\n"; 
        return; 
    }
    
    p += -fine;
    p.setStatus("ACTIVE");
    state.jail_turns.erase(&p);
    cout << p.getUsername() << " membayar denda M" << fine << " dan bebas dari penjara.\n";
    state.addLog(p, "BAYAR_DENDA", to_string(fine));
}

bool BankruptcyProcessor::tryForceJailFine(Player &p) {
    int fine = state.config.getJailFine();
    if (p.getBalance() >= fine) {
        p += -fine;
        p.setStatus("ACTIVE");
        state.jail_turns.erase(&p);
        cout << p.getUsername() << " membayar denda M" << fine << " dan bebas dari penjara.\n";
        state.addLog(p, "BAYAR_DENDA", "Paksa keluar M" + to_string(fine));
        return true;
    }
    
    int maxLiq = PropertyManager::calculateMaxLiquidation(p);
    if (maxLiq + p.getBalance() >= fine) {
        cout << "Likuidasi aset untuk bayar denda penjara.\n";
        PropertyManager::autoLiquidate(p, state.board, fine);
        if (p.getBalance() >= fine) {
            p += -fine;
            p.setStatus("ACTIVE");
            state.jail_turns.erase(&p);
            cout << p.getUsername() << " membayar denda M" << fine << " dan bebas.\n";
            state.addLog(p, "BAYAR_DENDA", "Paksa keluar M" + to_string(fine));
            return true;
        }
    }
    
    cout << "Tidak cukup aset untuk membayar denda. Bangkrut!\n";
    processBankruptcy(p);
    return false;
}