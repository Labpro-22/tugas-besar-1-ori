#include "include/core/CardManager.hpp"
#include <iostream>

void CardManager::resolveCardEffect(
    Card &card, 
    Player &active_player, 
    const std::vector<Player *> &all_players
) {
    card.action(active_player);
    std::string type = card.getCardType();

    if (type == "HAPPY_BIRTHDAY") {
        applyHappyBirthday(active_player, all_players, 100);
    } else if (type == "LEGISLATIVE") {
        applyLegislative(active_player, all_players, 200);
    }
}

void CardManager::applyHappyBirthday(
    Player &receiver, 
    const std::vector<Player *> &all_players, 
    int amount_per_player
) {
    for (auto *p : all_players) {
        if (p && p != &receiver && p->getStatus() != "BANKRUPT") {
            int payAmount = amount_per_player;
            if (p->getBalance() >= payAmount) {
                transfer(*p, receiver, payAmount);
                std::cout << p->getUsername() << " membayar M" << payAmount << " ke " << receiver.getUsername() << ".\n";
            } else {
                int actualPay = p->getBalance();
                if (actualPay > 0) {
                    transfer(*p, receiver, actualPay);
                    std::cout << p->getUsername() << " tidak mampu membayar penuh. Membayar sisa M" << actualPay << ".\n";
                } else {
                    std::cout << p->getUsername() << " tidak memiliki uang untuk membayar.\n";
                }
            }
        }
    }
}

void CardManager::applyLegislative(
    Player &payer,
    const std::vector<Player *> &all_players,
    int amount_per_player
) {
    for (auto *p : all_players) {
        if (!p || p == &payer || p->getStatus() == "BANKRUPT") continue;
        int pay = amount_per_player;
        if (payer.getBalance() >= pay) {
            transfer(payer, *p, pay);
            std::cout << payer.getUsername() << " membayar M" << pay
                      << " ke " << p->getUsername() << ".\n";
        } else {
            int sisa = payer.getBalance();
            if (sisa > 0) {
                transfer(payer, *p, sisa);
                std::cout << payer.getUsername() << " tidak mampu membayar penuh. Membayar M"
                          << sisa << " ke " << p->getUsername() << ".\n";
            } else {
                std::cout << payer.getUsername() << " tidak memiliki uang untuk membayar " << p->getUsername() << ".\n";
            }
        }
    }
}

void CardManager::transfer(Player &from, Player &to, int amount) {
    from += -amount;
    to += amount;
}