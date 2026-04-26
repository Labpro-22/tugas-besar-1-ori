#include "include/core/LandingProcessor.hpp"
#include "include/core/GameState.hpp"
#include "include/core/PropertyManager.hpp"
#include "include/core/RentManager.hpp"
#include "include/core/RentCollector.hpp"
#include "include/core/TaxManager.hpp"
#include "include/core/JailManager.hpp"
#include "include/core/BankruptcyProcessor.hpp"
#include "include/core/CardManager.hpp"
#include "include/models/player/Player.hpp"
#include "include/models/player/Bot.hpp"
#include "include/models/tiles/PropertyTile.hpp"
#ifdef GUI_MODE
#include "include/utils/exceptions/UnablePayPPHTaxException.hpp"
#include "include/utils/exceptions/UnablePayPBMTaxException.hpp"
#include "include/utils/exceptions/InsufficientMoneyException.hpp"
#endif
#include "include/models/card/ChanceCard.hpp"
#include "include/models/card/CommunityChest.hpp"
#include "include/utils/exceptions/UnablePayBuildException.hpp"

#include <iostream>
#include <limits>
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

    string centerText(const string &t, int w) {
        int pad = w - (int)t.size();
        if (pad <= 0) {
            return t;
        }
        int l = pad / 2, r = pad - l;
        return string(l, ' ') + t + string(r, ' ');
    }

    string leftPad(const string &t, int w) {
        if ((int)t.size() >= w) {
            return t;
        }
        return t + string(w - (int)t.size(), ' ');
    }
}

LandingProcessor::LandingProcessor(GameState &s) : state(s), rentCollector(nullptr), bankruptcyProc(nullptr) {}

void LandingProcessor::setRentCollector(RentCollector *rc) {
    rentCollector = rc;
}

void LandingProcessor::setBankruptcyProcessor(BankruptcyProcessor *bp) {
    bankruptcyProc = bp;
}

bool LandingProcessor::rollAndMove(Player &p, bool manual, int d1, int d2) {
    int boardSize = state.board.getTileCount();

    if (manual) {
        state.dice.setDice(d1, d2);
    } else {
        state.dice.throwDice();
    }

    int oldTile = p.getCurrTile();
    int newTile = (oldTile + state.dice.getTotal()) % boardSize;
    p.setCurrTile(newTile);

    cout << p.getUsername() << " melempar dadu: " << state.dice.getDie1() << " + " << state.dice.getDie2()
         << " = " << state.dice.getTotal()
         << (state.dice.isDouble() ? " (GANDA)" : "") << "\n";

    cout << "Pindah dari petak " << oldTile << " ke petak " << newTile << ".\n";

    applyGoSalary(p, oldTile);
    return state.dice.isDouble();
}

void LandingProcessor::applyLanding(Player &p) {
    int tileIdx = p.getCurrTile();
    Tile *t = state.tiles[tileIdx];

    cout << p.getUsername() << " mendarat di [" << t->getTileCode()
         << "] " << t->getTileName() << "\n";

    string type = t->getTileType();
    if (type == "STREET" || type == "RAILROAD" || type == "UTILITY") {
        auto *prop = dynamic_cast<PropertyTile*>(t);
        if (prop) {
            handlePropertyLanding(p, *prop);
        } else {
            t->onLanded(p, *this);
        }
    } else {
        t->onLanded(p, *this);
    }
}

void LandingProcessor::applyGoSalary(Player &p, int oldTile) {
    int newTile = p.getCurrTile();
    int boardSize = state.board.getTileCount();

    if (state.tiles[newTile]->getTileType() == "GO_JAIL") {
        return;
    }

    // Forward distance: how many tiles forward from oldTile to newTile
    // If forwardDist > half board, player moved BACKWARD (e.g. StepbackCard) → no salary
    int forwardDist = (newTile - oldTile + boardSize) % boardSize;
    if (forwardDist == 0 || forwardDist > boardSize / 2) {
        return;  // backward or no movement — no GO salary
    }

    if (newTile < oldTile) {  // wrapped forward past tile 0 (GO)
        p += state.config.getGoSalary();
        cout << p.getUsername() << " melewati GO! Menerima gaji M"
             << state.config.getGoSalary() << ".\n";
        state.addLog(p, "GAJI_GO", "M" + to_string(state.config.getGoSalary()));
    }
}

void LandingProcessor::sendToJail(Player &p) {
    int jailIdx = 0;
    for (int i = 0; i < state.board.getTileCount(); i++) {
        if (state.tiles[i]->getTileType() == "JAIL") {
            jailIdx = i;
            break;
        }
    }

    JailManager::goToJail(p, jailIdx);
    state.jail_turns[&p] = 0;

    cout << p.getUsername() << " masuk penjara!\n";
    state.addLog(p, "MASUK_PENJARA", "");
}

void LandingProcessor::handlePropertyLanding(Player &p, PropertyTile &prop) {
    if (prop.isMortgage()) {
        cout << "Properti ini sedang digadaikan. Tidak ada sewa.\n";
        return;
    }

    string type = prop.getTileType();
    Player *owner = prop.getTileOwner();

    if (!owner) {
        if (type == "RAILROAD" || type == "UTILITY") {
            p.addOwnedProperty(&prop);
            state.recomputeMonopolyForGroup(prop.getColorGroup());
            if (p.getDiscountActive() > 0.0f) p.setDiscountActive(0.0f);
            cout << "Belum ada yang menginjaknya duluan, "
                 << prop.getTileName() << " kini menjadi milikmu!\n";
            state.addLog(p, "OTOMATIS",
                   prop.getTileName() + " (" + prop.getTileCode() + ") jadi milik " + p.getUsername());
        } else {
            bool isBot = (dynamic_cast<Bot*>(&p) != nullptr);
#ifdef GUI_MODE
            (void)isBot;
            cout << "Properti " << prop.getTileName() << " (" << prop.getTileCode()
                 << ") tersedia. Gunakan BELI untuk membeli atau lewati untuk dilelang.\n";
#else
            if (isBot) {
                cout << "Properti " << prop.getTileName() << " (" << prop.getTileCode()
                     << ") tersedia. Bot akan memutuskan.\n";
            } else {
                state.formatter.printAkta(prop, state.board, state.config);
                int price = prop.getBuyPrice();
                if (p.getDiscountActive() > 0.0f) {
                    price = static_cast<int>(price * (1.0f - p.getDiscountActive()));
                }
                cout << "Uang kamu saat ini: M" << p.getBalance() << "\n";

                bool canAfford = (p.getBalance() >= price);
                if (!canAfford) {
                    cout << "Saldo tidak cukup untuk membeli (perlu M" << price << "). Properti masuk lelang.\n";
                    if (bankruptcyProc) bankruptcyProc->handleAutoAuction(p);
                } else {
                    cout << "Apakah kamu ingin membeli properti ini seharga M" << price << "? (y/n): ";
                    char ans = 0;
                    while (true) {
                        if (!(cin >> ans)) {
                            cin.clear();
                            cin.ignore(numeric_limits<streamsize>::max(), '\n');
                            cout << "  Input tidak valid. (y/n): ";
                            continue;
                        }
                        ans = static_cast<char>(toupper(static_cast<unsigned char>(ans)));
                        if (ans == 'Y' || ans == 'N') break;
                        cout << "  Input tidak valid. (y/n): ";
                    }
                    if (ans == 'Y') {
                        p += -price;
                        p.addOwnedProperty(&prop);
                        state.recomputeMonopolyForGroup(prop.getColorGroup());
                        if (p.getDiscountActive() > 0.0f) p.setDiscountActive(0.0f);
                        cout << prop.getTileName() << " kini menjadi milikmu!\n";
                        cout << "Uang tersisa: M" << p.getBalance() << "\n";
                        state.addLog(p, "BELI", prop.getTileName() + " (" + prop.getTileCode() + ") M" + to_string(price));
                    } else {
                        cout << "Properti ini akan masuk ke sistem lelang...\n";
                        if (bankruptcyProc) bankruptcyProc->handleAutoAuction(p);
                    }
                }
            }
#endif
        }
    } else if (owner == &p) {
        cout << "Ini properti Anda sendiri.\n";
    } else if (owner->getStatus() == "JAIL") {
        cout << owner->getUsername() << " sedang di penjara. Tidak ada sewa.\n";
        state.addLog(p, "SEWA_BEBAS", prop.getTileCode() + " (pemilik di penjara)");
    } else {
        if (p.isShieldActive()) {
            cout << "[SHIELD ACTIVE]: Efek ShieldCard melindungi Anda!\n";
            cout << "Tagihan sewa dibatalkan. Uang Anda tetap: M" << p.getBalance() << ".\n";
        } else {
            if (rentCollector) {
                rentCollector->autoPayRent(p, prop);
            }
        }
    }
}

void LandingProcessor::handleFestivalLanding(Player &p) {
    auto myProps = p.getOwnedProperties();
    bool isBot = (dynamic_cast<Bot*>(&p) != nullptr);

    if (myProps.empty()) {
        cout << "Festival! Tapi " << p.getUsername() << " tidak punya properti.\n";
    } else {
        if (!isBot) {
            cout << "Festival! Pilih properti Anda:\n";
            for (auto *fp : myProps) {
                if (!fp->isMortgage()) {
                    int currentRent = computeRent(*fp, state.dice.getTotal());
                    cout << "  [" << fp->getTileCode() << "] "
                         << fp->getTileName()
                         << " (sewa sekarang M" << currentRent << ")\n";
                }
            }
            cout << "Gunakan: FESTIVAL <KODE>\n";
        } else {
            cout << "Festival! " << p.getUsername() << " (bot) memilih otomatis.\n";
        }
    }
}

void LandingProcessor::handleTax(Player &p, const string &taxType) {
    int amt = 0;
    bool isBot = (dynamic_cast<Bot*>(&p) != nullptr);

    if (taxType == "TAX_PPH") {
        cout << "Petak Pajak Penghasilan (PPH)\n";
        cout << "  1. Bayar flat M" << state.config.getPphFlat() << "\n";
        cout << "  2. Bayar " << state.config.getPphPercentage() << "% dari total kekayaan\n";

        int choice;
        if (isBot) {
            int flatAmt = TaxManager::calculatePPHFlat(state.config);
            int pctAmt = TaxManager::calculatePPHPct(p, state.config);
            choice = (flatAmt <= pctAmt) ? 1 : 2;
            cout << "Bot memilih opsi " << choice << ".\n";
        } else {
#ifdef GUI_MODE
            // In GUI mode, PPH choice is handled by the GUI before applyLanding is called.
            // If we somehow reach here, default to the cheaper option.
            int flatAmt2 = TaxManager::calculatePPHFlat(state.config);
            int pctAmt2  = TaxManager::calculatePPHPct(p, state.config);
            choice = (flatAmt2 <= pctAmt2) ? 1 : 2;
#else
            choice = readInt("Pilihan (1/2): ", 1, 2);
#endif
        }

        if (choice == 1) {
            amt = TaxManager::calculatePPHFlat(state.config);
        } else {
            amt = TaxManager::calculatePPHPct(p, state.config);
            cout << "Total kekayaan: M" << p.getNetWorth() << ", pajak: M" << amt << "\n";
        }
    } else if (taxType == "TAX_PBM") {
        amt = TaxManager::getPBMFlat(state.config);
    } else {
        cout << "Bukan petak pajak.\n";
        return;
    }

    if (p.getBalance() >= amt) {
        p += -amt;
        cout << p.getUsername() << " membayar pajak M" << amt << ".\n";
        state.addLog(p, "BAYAR_PAJAK", taxType + " M" + to_string(amt));
    } else {
        int maxLiq = PropertyManager::calculateMaxLiquidation(p);
        if (maxLiq < amt) {
            if (isBot || !bankruptcyProc) {
                cout << "Tidak cukup aset untuk membayar pajak M" << amt << ". Bangkrut ke Bank!\n";
                if (bankruptcyProc) bankruptcyProc->processBankruptcy(p);
            } else {
                bankruptcyProc->processBankruptcy(p);
            }
        } else if (isBot) {
            PropertyManager::autoLiquidate(p, state.board, amt);
            p += -amt;
            cout << p.getUsername() << " membayar pajak M" << amt << ".\n";
            state.addLog(p, "BAYAR_PAJAK", taxType + " M" + to_string(amt));
        } else {
#ifdef GUI_MODE
            // Throw so GUI can show debt recovery UI instead of blocking on stdin
            if (taxType == "TAX_PPH") throw UnablePayPPHTaxException("Pajak PPH M" + to_string(amt));
            if (taxType == "TAX_PBM") throw UnablePayPBMTaxException("Pajak PBM M" + to_string(amt));
#else
            if (rentCollector) {
                rentCollector->recoverForDebt(p, amt, nullptr, "pajak");
            }
            if (p.getStatus() != "BANKRUPT" && p.getBalance() >= amt) {
                p += -amt;
                cout << p.getUsername() << " membayar pajak M" << amt << ".\n";
                state.addLog(p, "BAYAR_PAJAK", taxType + " M" + to_string(amt));
            }
#endif
        }
    }
}

int LandingProcessor::computeRent(PropertyTile &prop, int diceTotal) const {
    return RentManager::computeRent(prop, diceTotal, state.config, state.tiles);
}

void LandingProcessor::drawAndResolveChance(Player &p) {
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

        if (p.getCurrTile() != before && p.getStatus() != "JAIL" && p.getStatus() != "BANKRUPT") {
            applyGoSalary(p, before);
            applyLanding(p);
        }
        state.addLog(p, "KARTU", "Kesempatan: " + card->describe());

#ifdef GUI_MODE
        // If card deducted enough to push balance negative but player can still recover
        if (p.getBalance() < 0 && p.getStatus() != "BANKRUPT") {
            int maxLiq = PropertyManager::calculateMaxLiquidation(p);
            if (maxLiq >= 0)
                throw InsufficientMoneyException("CARD:" + to_string(-p.getBalance()));
            else if (bankruptcyProc)
                bankruptcyProc->processBankruptcy(p);
        }
#endif
    }
}

void LandingProcessor::drawAndResolveCommunityChest(Player &p) {
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
                int maxLiq = PropertyManager::calculateMaxLiquidation(p);
                if (maxLiq < 0) {
                    cout << p.getUsername() << " tidak mampu membayar tagihan kartu Dana Umum! Bangkrut!\n";
                    if (bankruptcyProc) bankruptcyProc->processBankruptcy(p);
                } else {
#ifdef GUI_MODE
                    state.community_deck.discardCard(card);
                    state.addLog(p, "KARTU", "Dana Umum: " + card->describe());
                    throw InsufficientMoneyException("CARD:" + to_string(-p.getBalance()));
#else
                    int debt = -p.getBalance();
                    cout << "Saldo negatif M" << debt << ". Likuidasi paksa untuk menutup tagihan kartu.\n";
                    PropertyManager::autoLiquidate(p, state.board, 0);
                    if (p.getBalance() < 0) {
                        cout << p.getUsername() << " tetap tidak mampu membayar setelah likuidasi. Bangkrut!\n";
                        if (bankruptcyProc) bankruptcyProc->processBankruptcy(p);
                    }
#endif
                }
            }
        }
        state.community_deck.discardCard(card);
        state.addLog(p, "KARTU", "Dana Umum: " + card->describe());
    }
}

void LandingProcessor::displayMessage(const std::string &msg) {
    cout << msg << "\n";
}

void LandingProcessor::addLog(Player &p, const std::string &action, const std::string &detail) {
    state.addLog(p, action, detail);
}

int LandingProcessor::getDiceTotal() const {
    return state.dice.getTotal();
}

std::vector<Player*> LandingProcessor::getActivePlayers() const {
    std::vector<Player*> result;
    for (auto *p : state.players) {
        if (p->getStatus() != "BANKRUPT") {
            result.push_back(p);
        }
    }
    return result;
}
