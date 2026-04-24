#ifndef AUCTIONMANAGER_HPP
#define AUCTIONMANAGER_HPP
#include "include/models/player/Player.hpp"
#include "include/models/tiles/PropertyTile.hpp"

#include <vector>
class AuctionManager {
public:
    class AuctionResult {
    public:
        bool sold = false;
        Player *winner = nullptr;
        int final_bid = 0;
    };
    static AuctionResult auctionProperty(Player &seller, PropertyTile &tile,
                                          const std::vector<Player *> &participants, int minimum_bid = -1);
    static AuctionResult interactiveAuction(Player *seller, PropertyTile &tile,
                                             const std::vector<Player *> &turn_order, Player *trigger);
};
#endif