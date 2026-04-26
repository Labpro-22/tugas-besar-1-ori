#include "include/core/BotController.hpp"
#include "include/core/GameState.hpp"
#include "include/core/LandingProcessor.hpp"
#include "include/core/BankruptcyProcessor.hpp"
#include "include/core/CardProcessor.hpp"
#include "include/core/RentCollector.hpp"
#include "include/core/JailManager.hpp"
#include "include/core/PropertyManager.hpp"
#include "include/core/RentManager.hpp"
#include "include/models/player/Bot.hpp"
#include "include/models/player/Player.hpp"
#include "include/models/tiles/PropertyTile.hpp"

#include <iostream>
#include <string>

using namespace std;

BotController::BotController(
    GameState &s, 
    LandingProcessor &lp,
    BankruptcyProcessor &bp, 
    CardProcessor &cp,
    RentCollector &rc
) : state(s), landing(lp), bankruptcy(bp), cardProc(cp), rentCollector(rc) {}

void BotController::executeTurn(Player &p, int consecDoubles) {
    cout << "\n=== Giliran Bot " << p.getUsername()
         << " (Turn " << state.current_turn << "/" << state.max_turn << ") ===\n";
    
    cardProc.drawSkillCardAtTurnStart(p);

    if (p.getStatus() == "JAIL") {
        if (state.jail_turns.find(&p) == state.jail_turns.end()) {
            state.jail_turns[&p] = 0;
        }

        int fine = state.config.getJailFine();

        if (state.jail_turns[&p] >= 3) {
            if (p.getBalance() >= fine) {
                p += -fine; 
                p.setStatus("ACTIVE"); 
                state.jail_turns.erase(&p);
                
                cout << p.getUsername() << " (bot) WAJIB bayar denda M" << fine << ".\n";
                state.addLog(p, "BAYAR_DENDA", "Paksa keluar M" + to_string(fine));
                
                bool isDouble = landing.rollAndMove(p, false);
                landing.applyLanding(p);
                
                if (p.getStatus() == "BANKRUPT") {
                    return;
                }
                if (isDouble && p.getStatus() != "JAIL" && p.getStatus() != "BANKRUPT") {
                    cout << p.getUsername() << " (bot) mendapat giliran tambahan!\n";
                    executeTurn(p, 0);
                }
            } else {
                int maxLiq = PropertyManager::calculateMaxLiquidation(p);
                if (maxLiq + p.getBalance() >= fine) {
                    PropertyManager::autoLiquidate(p, state.board, fine);
                    if (p.getBalance() >= fine) {
                        p += -fine; 
                        p.setStatus("ACTIVE"); 
                        state.jail_turns.erase(&p);
                        
                        cout << p.getUsername() << " (bot) bayar denda M" << fine << ".\n";
                        state.addLog(p, "BAYAR_DENDA", "Paksa keluar M" + to_string(fine));
                        
                        bool isDouble = landing.rollAndMove(p, false);
                        landing.applyLanding(p);
                        
                        if (p.getStatus() == "BANKRUPT") {
                            return;
                        }
                        if (isDouble && p.getStatus() != "JAIL" && p.getStatus() != "BANKRUPT") {
                            executeTurn(p, 0);
                        }
                    } else { 
                        bankruptcy.processBankruptcy(p); 
                    }
                } else { 
                    bankruptcy.processBankruptcy(p); 
                }
            }
            return;
        }

        state.dice.throwDice();
        bool isDouble = state.dice.isDouble();
        
        cout << p.getUsername() << " (bot) lempar dadu di penjara: "
             << state.dice.getDie1() << " + " << state.dice.getDie2()
             << " = " << state.dice.getTotal() << (isDouble ? " (GANDA)" : "") << "\n";
        
        if (!isDouble) {
            state.jail_turns[&p]++;
            cout << p.getUsername() << " (bot) gagal keluar penjara (" << state.jail_turns[&p] << "/3).\n";
            state.addLog(p, "DADU", "Lempar: " + to_string(state.dice.getDie1()) + "+" + to_string(state.dice.getDie2()) + " [PENJARA] gagal");
        } else {
            int jailTile = p.getCurrTile();
            int newTile = (jailTile + state.dice.getTotal()) % state.board.getTileCount();
            
            p.setCurrTile(newTile);
            landing.applyGoSalary(p, jailTile);
            p.setStatus("ACTIVE"); 
            state.jail_turns.erase(&p);
            
            cout << p.getUsername() << " (bot) keluar penjara (dadu ganda)!\n";
            state.addLog(p, "DADU", "Lempar: " + to_string(state.dice.getDie1()) + "+" + to_string(state.dice.getDie2()) + " [PENJARA] keluar");
            
            landing.applyLanding(p);
            if (p.getStatus() == "BANKRUPT") {
                return;
            }
        }
        return;
    }

    int newConsec = consecDoubles;
    bool isDouble = landing.rollAndMove(p, false);
    
    state.addLog(p, "DADU", "Lempar: " + to_string(state.dice.getDie1()) + "+" + to_string(state.dice.getDie2()) + "=" + to_string(state.dice.getTotal()) + (isDouble ? " (double)" : ""));
    
    if (isDouble) {
        newConsec++;
        if (newConsec >= 3) {
            landing.sendToJail(p);
            state.addLog(p, "MASUK_PENJARA", "3x double berturut-turut");
            return;
        }
    } else {
        newConsec = 0;
    }

    landing.applyLanding(p);
    if (p.getStatus() == "BANKRUPT") {
        return;
    }

    if (state.tiles[p.getCurrTile()]->getTileType() == "FESTIVAL") {
        auto *bestProp = Bot::chooseFestivalProperty(p.getOwnedProperties());
        if (bestProp) {
            cout << p.getUsername() << " (bot) memilih " << bestProp->getTileName() << " untuk Festival.\n";
            cardProc.cmdFestival(p, bestProp->getTileCode());
        }
    }

    auto *prop = dynamic_cast<PropertyTile*>(state.tiles[p.getCurrTile()]);
    if (prop && !prop->getTileOwner() && !prop->isMortgage() && prop->getTileType() == "STREET") {
        if (Bot::shouldBuyStreet(p.getBalance(), prop->getBuyPrice())) {
            int price = prop->getBuyPrice();
            if (p.getDiscountActive() > 0.0f) {
                price = static_cast<int>(price * (1.0f - p.getDiscountActive()));
            }
            p += -price;
            p.addOwnedProperty(prop);
            state.recomputeMonopolyForGroup(prop->getColorGroup());
            if (p.getDiscountActive() > 0.0f) p.setDiscountActive(0.0f);
            
            cout << p.getUsername() << " (bot) membeli " << prop->getTileName() << " seharga M" << price << ".\n";
            state.addLog(p, "BELI", prop->getTileName() + " M" + to_string(price));
        } else {
            cout << p.getUsername() << " (bot) menolak beli. Lelang otomatis.\n";
            bankruptcy.handleAutoAuction(p);
        }
    }

    if (isDouble && p.getStatus() != "JAIL" && p.getStatus() != "BANKRUPT") {
        cout << p.getUsername() << " (bot) mendapat giliran tambahan!\n";
        executeTurn(p, newConsec);
    }
}