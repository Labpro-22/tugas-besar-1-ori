#include "include/core/EventManager.hpp"
#include <iostream>

void EventManager::win(Player& p) {
    events.push_back(p.getUsername() + " menang!");
}

void EventManager::bankrupt(Player& p) {
    events.push_back(p.getUsername() + " bangkrut!");
}

void EventManager::auctionAll(Player& p, const std::vector<Player*>& participants) {
    // TO-DO
}

void EventManager::transferAsset(Player& p1, Player& p2) {
    // TO-DO
}

void EventManager::processWin(Player &player) {
    win(player);
}

void EventManager::processBankruptcy(Player &debtor, Player *creditor) {
    bankrupt(debtor);
    if (creditor) {
        transferAsset(debtor, *creditor);
    }
}

void EventManager::processBankruptcy(
    Player &debtor, 
    const std::vector<Player*> &participants, 
    Player *creditor
) {
    bankrupt(debtor);
    if (creditor) {
        transferAsset(debtor, *creditor);
    } else {
        auctionAll(debtor, participants);
    }
}

void EventManager::recordAction(Player &player, const std::string &action_name) {
    events.push_back(player.getUsername() + " -> " + action_name);
}

void EventManager::recordSuccess(Player &player, const std::string &detail) {
    events.push_back(player.getUsername() + " sukses: " + detail);
}

void EventManager::recordFailure(Player &player, const std::string &detail) {
    events.push_back(player.getUsername() + " gagal: " + detail);
}

void EventManager::flushEventsTo(std::ostream &out) {
    for (auto &e : events) {
        out << e << "\n";
    }
    events.clear();
}

void EventManager::pushEvent(const std::string &event_text) {
    events.push_back(event_text);
}

bool EventManager::hasEvents() const {
    return !events.empty();
}

std::vector<std::string> EventManager::drainEvents() {
    auto result = events;
    events.clear();
    return result;
}