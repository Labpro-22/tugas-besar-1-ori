#include "include/core/AuctionManager.hpp"
#include "include/models/player/Bot.hpp"
#include "include/utils/exceptions/NoPropertyException.hpp"
#include <algorithm>
#include <iostream>
#include <set>
#include <limits>

namespace {
    int readInt(const std::string &prompt, int lo, int hi) {
        int v;
        while (true) {
            std::cout << prompt;
            if (std::cin >> v && v >= lo && v <= hi) {
                return v;
            }
            std::cout << "  Input tidak valid.\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }
}

AuctionManager::AuctionResult AuctionManager::auctionProperty(
    Player &seller, 
    PropertyTile &tile,
    const std::vector<Player *> &participants, 
    int minimum_bid
) {
    int opening_bid = (minimum_bid > 0) ? minimum_bid : std::max(1, tile.getMortgageValue());
    std::vector<Player *> eligible;

    for (auto *p : participants) {
        if (p && p != &seller && p->getStatus() != "BANKRUPT" && p->getBalance() >= opening_bid) {
            eligible.push_back(p);
        }
    }

    seller.removeOwnedProperty(tile.getTileCode());
    tile.setMortgageStatus(false);
    tile.setMonopolized(false);

    if (eligible.empty()) {
        return AuctionResult{};
    }

    std::sort(eligible.begin(), eligible.end(), [](Player *a, Player *b) {
        return a->getBalance() > b->getBalance();
    });

    Player *winner = eligible.front();
    int final_bid = opening_bid;

    if (eligible.size() > 1) {
        final_bid = std::min(eligible[1]->getBalance() + 1, winner->getBalance());
    }

    final_bid = std::max(final_bid, opening_bid);
    winner->operator+=(-final_bid);
    winner->addOwnedProperty(&tile);

    AuctionResult result;
    result.sold = true;
    result.winner = winner;
    result.final_bid = final_bid;

    return result;
}

AuctionManager::AuctionResult AuctionManager::interactiveAuction(
    Player *seller, 
    PropertyTile &tile,
    const std::vector<Player *> &turn_order, 
    Player *trigger
) {
    std::cout << "Properti: [" << tile.getTileCode() << "] " << tile.getTileName() << "\n";
    
    if (!trigger) {
        trigger = seller;
    }

    std::vector<Player *> bidders;
    int sz = static_cast<int>(turn_order.size());
    int triggerIdx = 0;

    for (int i = 0; i < sz; i++) {
        if (turn_order[i] == trigger) {
            triggerIdx = i;
            break;
        }
    }

    for (int i = 1; i <= sz; i++) {
        Player *pl = turn_order[(triggerIdx + i) % sz];
        if (pl == trigger) {
            continue;
        }
        if (pl->getStatus() == "BANKRUPT") {
            continue;
        }
        bidders.push_back(pl);
    }

    if (!seller && trigger && trigger->getStatus() != "BANKRUPT") {
        bidders.push_back(trigger);
    }

    if (bidders.empty()) {
        std::cout << "Tidak ada peserta lelang.\n";
        return AuctionResult{};
    }

    int needPasses = std::max(1, static_cast<int>(bidders.size()) - 1);
    std::cout << "Peserta: " << bidders.size() << " orang. Minimum bid: M0.\n";
    std::cout << "Aksi: PASS atau BID <jumlah>\n";

    int highestBid = -1;
    Player *highestBidder = nullptr;
    int consecPasses = 0;
    bool anyBid = false;
    int rot = 0;
    std::set<Player *> permanentPass;

    while (true) {
        if (anyBid && consecPasses >= needPasses) {
            break;
        }

        if (!anyBid && consecPasses >= needPasses) {
            Player *last = nullptr;
            for (int j = 0; j < static_cast<int>(bidders.size()); j++) {
                Player *c = bidders[(rot + j) % static_cast<int>(bidders.size())];
                if (!permanentPass.count(c) && c->getStatus() != "BANKRUPT") {
                    last = c;
                    break;
                }
            }

            if (!last) {
                break;
            }

            bool isBotForced = (dynamic_cast<Bot*>(last) != nullptr);
            int forcedBid = std::max(0, highestBid + 1);

            if (isBotForced) {
                forcedBid = std::min(forcedBid, last->getBalance());
            } else {
                std::cout << "Giliran: " << last->getUsername() << " WAJIB BID (min M" << forcedBid << ")\n> ";
                std::string inp;
                std::cin >> inp;
                try {
                    forcedBid = std::stoi(inp);
                } catch (...) {
                    forcedBid = 0;
                }
            }

            if (forcedBid > highestBid && forcedBid <= last->getBalance()) {
                highestBid = forcedBid;
                highestBidder = last;
                anyBid = true;
            } else if (forcedBid == 0 && highestBid < 0) {
                highestBid = 0;
                highestBidder = last;
                anyBid = true;
            }
            break;
        }

        Player *cur = bidders[rot % static_cast<int>(bidders.size())];
        rot++;

        if (permanentPass.count(cur)) {
            continue;
        }

        if (cur->getStatus() == "BANKRUPT") {
            permanentPass.insert(cur);
            consecPasses++;
            continue;
        }

        int minNext = anyBid ? (highestBid + 1) : 0;
        if (cur->getBalance() < minNext) {
            std::cout << cur->getUsername() << " auto-pass (saldo kurang).\n";
            permanentPass.insert(cur);
            consecPasses++;
            continue;
        }

        bool isBot = (dynamic_cast<Bot*>(cur) != nullptr);
        if (isBot) {
            int proposed = anyBid ? highestBid + 5 : std::max(1, tile.getMortgageValue());
            if (proposed > cur->getBalance() * 3 / 5) {
                std::cout << cur->getUsername() << " (bot) PASS\n";
                consecPasses++;
            } else {
                proposed = std::min(proposed, cur->getBalance());
                if (proposed <= highestBid) {
                    proposed = highestBid + 1;
                }
                if (proposed > cur->getBalance()) {
                    std::cout << cur->getUsername() << " (bot) PASS\n";
                    permanentPass.insert(cur);
                    consecPasses++;
                } else {
                    std::cout << cur->getUsername() << " (bot) BID M" << proposed << "\n";
                    highestBid = proposed;
                    highestBidder = cur;
                    anyBid = true;
                    consecPasses = 0;
                }
            }
        } else {
            std::cout << "\nGiliran: " << cur->getUsername() << " (saldo M" << cur->getBalance() << ")\n";
            if (highestBidder) {
                std::cout << "Penawaran tertinggi: M" << highestBid << " (" << highestBidder->getUsername() << ")\n";
            }
            std::cout << "Aksi (PASS / BID <jumlah>)\n> ";
            
            std::string action;
            std::cin >> action;
            for (auto &c : action) {
                c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
            }

            if (action == "PASS") {
                std::cout << cur->getUsername() << " PASS\n";
                consecPasses++;
            } else if (action == "BID") {
                int bid;
                std::cin >> bid;
                if (bid <= highestBid || bid > cur->getBalance()) {
                    std::cout << "Bid tidak valid! Maks M" << cur->getBalance() << ", min M" << (highestBid + 1) << "\n";
                    rot--;
                } else {
                    highestBid = bid;
                    highestBidder = cur;
                    anyBid = true;
                    consecPasses = 0;
                    std::cout << "Penawaran tertinggi: M" << highestBid << " (" << highestBidder->getUsername() << ")\n";
                }
            } else {
                rot--;
            }
        }
    }

    if (highestBidder) {
        highestBidder->operator+=(-highestBid);
        highestBidder->addOwnedProperty(&tile);
        if (seller) {
            seller->removeOwnedProperty(tile.getTileCode());
        }
        std::cout << "Lelang selesai! Pemenang: " << highestBidder->getUsername() << " seharga M" << highestBid << "\n";
    } else {
        std::cout << "Lelang selesai! Tidak ada pemenang.\n";
    }

    AuctionResult result;
    result.sold = (highestBidder != nullptr);
    result.winner = highestBidder;
    result.final_bid = std::max(0, highestBid);
    
    return result;
}