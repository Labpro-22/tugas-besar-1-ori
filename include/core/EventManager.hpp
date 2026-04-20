#ifndef EVENTMANAGER_HPP
#define EVENTMANAGER_HPP

#include "include/models/player/Player.hpp"
#include <ostream>
#include <string>
#include <vector>

class EventManager
{
private:
    std::vector<std::string> events;
    void win(Player& p);
    void bankrupt(Player& p);
    void auctionAll(Player& p, const std::vector<Player *> &participants);
    void transferAsset(Player& p1, Player& p2);

public:
    void processWin(Player &player);
    void processBankruptcy(Player &debtor, Player *creditor = nullptr);
    void processBankruptcy(Player &debtor, const std::vector<Player *> &participants, Player *creditor = nullptr);
    void recordAction(Player &player, const std::string &action_name);
    void recordSuccess(Player &player, const std::string &detail);
    void recordFailure(Player &player, const std::string &detail);

    void flushEventsTo(std::ostream &out);
    void pushEvent(const std::string &event_text);
    bool hasEvents() const;
    std::vector<std::string> drainEvents();
};

#endif
