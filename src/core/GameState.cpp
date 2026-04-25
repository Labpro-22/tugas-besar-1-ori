#include "include/core/GameState.hpp"
#include "include/models/player/Bot.hpp"
#include "include/models/tiles/PropertyTile.hpp"
#include <algorithm>

GameState::GameState(
    std::vector<Tile*> t, 
    std::vector<Player*> p,
    std::vector<Player*> order, 
    GameConfig cfg,
    int maxT, 
    int currT, 
    int activeId
) : tiles(std::move(t)), 
    board(tiles), 
    players(std::move(p)),
    active_player_id(activeId), 
    current_turn(currT), 
    max_turn(maxT),
    turn_order(std::move(order)), 
    turn_order_idx(0), 
    transaction_log(),
    config(cfg),
    rentCollector(nullptr),
    game_over(false), 
    game_over_by_bankruptcy(false), 
    jail_turns() {
    
    if (!turn_order.empty() && active_player_id < static_cast<int>(players.size())) {
        Player *active = players[active_player_id];
        for (int i = 0; i < static_cast<int>(turn_order.size()); i++) {
            if (turn_order[i] == active) { 
                turn_order_idx = i; 
                break; 
            }
        }
    }
}

GameState::~GameState() {
    for (auto *t : tiles) {
        delete t;
    }
    for (auto *p : players) {
        delete p;
    }
    for (auto *c : chance_cards) {
        delete c;
    }
    for (auto *c : community_cards) {
        delete c;
    }
    for (auto *c : skill_cards) {
        delete c;
    }
}

int GameState::activePlayerCount() const {
    int cnt = 0;
    for (auto *p : players) {
        if (p->getStatus() != "BANKRUPT") {
            cnt++;
        }
    }
    return cnt;
}

Player* GameState::getCreditorAt(int tileIdx) const {
    auto *prop = dynamic_cast<PropertyTile*>(tiles[tileIdx]);
    if (!prop) {
        return nullptr;
    }
    return prop->getTileOwner();
}

int GameState::getPlayerNum(Player &p) const {
    for (int i = 0; i < static_cast<int>(players.size()); i++) {
        if (players[i] == &p) {
            return i + 1;
        }
    }
    return -1;
}

bool GameState::hasPendingAction(Player &p) const {
    int tileIdx = p.getCurrTile();
    Tile *t = tiles[tileIdx];
    std::string type = t->getTileType();

    if (type == "STREET") {
        auto *prop = dynamic_cast<PropertyTile*>(t);
        if (prop && !prop->getTileOwner() && !prop->isMortgage()) {
            return true;
        }
    }

    if (type == "FESTIVAL") {
        for (auto *prop : p.getOwnedProperties()) {
            if (prop && !prop->isMortgage()) {
                return true;
            }
        }
    }

    return false;
}

void GameState::addLog(Player &p, const std::string &action, const std::string &detail) {
    transaction_log.emplace_back(current_turn, p.getUsername(), action, detail);
}

Player* GameState::currentTurnPlayer() const {
    if (turn_order_idx >= 0 && turn_order_idx < static_cast<int>(turn_order.size())) {
        return turn_order[turn_order_idx];
    }
    return nullptr;
}

void GameState::recomputeMonopolyForGroup(const std::string &colorGroup) {
    auto group = board.getPropertiesByColorGroup(colorGroup);
    if (group.empty()) {
        return;
    }

    Player *owner = group[0]->getTileOwner();
    bool monopoly = (owner != nullptr);

    for (auto *gp : group) {
        if (gp->getTileOwner() != owner) { 
            monopoly = false; 
            break; 
        }
    }

    for (auto *gp : group) {
        gp->setMonopolized(monopoly);
    }
}