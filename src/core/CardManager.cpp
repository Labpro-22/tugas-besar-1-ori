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
            if (p->getBalance() >= amount_per_player) {
                transfer(*p, receiver, amount_per_player);
                std::cout << p->getUsername() << " membayar M" << amount_per_player << " ke " << receiver.getUsername() << ".\n";
            } else {
                transfer(*p, receiver, p->getBalance());
                std::cout << p->getUsername() << " tidak mampu membayar penuh. Membayar sisa M" << p->getBalance() << ".\n";
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
        if (p && p != &payer && p->getStatus() != "BANKRUPT") {
            transfer(payer, *p, amount_per_player);
        }
    }
}

void CardManager::transfer(Player &from, Player &to, int amount) {
    from += -amount;
    to += amount;
}