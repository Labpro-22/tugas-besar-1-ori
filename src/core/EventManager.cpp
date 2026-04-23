#include "include/core/EventManager.hpp"
#include "include/core/AuctionManager.hpp"

#include <exception>

namespace
{
    void transferAllProperties(Player &from, Player &to)
    {
        const std::vector<PropertyTile *> properties = from.getOwnedProperties();

        for (PropertyTile *property : properties)
        {
            if (property == nullptr)
            {
                continue;
            }

            from.removeOwnedProperty(property->getTileCode());
            to.addOwnedProperty(property);
        }
    }

    void destroyAllBuildingsAndUnmortgage(Player &from)
    {
        const std::vector<PropertyTile *> properties = from.getOwnedProperties();
        for (PropertyTile *property : properties)
        {
            if (property == nullptr) continue;
            property->setLevel(0);
            property->setMortgageStatus(false);
            property->setMonopolized(false);
            property->setFestivalState(1, 0);
        }
    }

    void releasePropertiesToBank(Player &from)
    {
        const std::vector<PropertyTile *> properties = from.getOwnedProperties();
        for (PropertyTile *property : properties)
        {
            if (property == nullptr) continue;
            from.removeOwnedProperty(property->getTileCode());
            property->setOwner(nullptr);
        }
    }
} // namespace

void EventManager::win(Player &p)
{
    p.setStatus("WIN");
    pushEvent(p.getUsername() + " wins the game.");
}

void EventManager::bankrupt(Player &p)
{
    p.setStatus("BANKRUPT");
    pushEvent(p.getUsername() + " is bankrupt.");
}

void EventManager::auctionAll(Player &p, const std::vector<Player *> &participants)
{
    const std::vector<PropertyTile *> properties = p.getOwnedProperties();

    if (properties.empty())
    {
        pushEvent(p.getUsername() + " has no properties to auction.");
        return;
    }

    int auctioned_count = 0;
    int total_revenue = 0;
    for (PropertyTile *property : properties)
    {
        if (property == nullptr)
        {
            continue;
        }

        try
        {
            AuctionManager::AuctionResult result = AuctionManager::auctionProperty(p, *property, participants);
            if (!result.sold || result.winner == nullptr)
            {
                pushEvent("No bidder for " + property->getTileCode() + ". Property returned to bank.");
                continue;
            }

            auctioned_count++;
            total_revenue += result.final_bid;
            pushEvent(
                property->getTileCode() + " sold to " + result.winner->getUsername() +
                " for " + std::to_string(result.final_bid) + ".");
        }
        catch (const std::exception &exception)
        {
            pushEvent("Failed to auction " + property->getTileCode() + ": " + exception.what());
        }
    }

    pushEvent(
        std::to_string(auctioned_count) + " properties owned by " + p.getUsername() +
        " are sold. Total revenue: " + std::to_string(total_revenue) + ".");
}

void EventManager::transferAsset(Player &p1, Player &p2)
{
    if (&p1 == &p2)
    {
        return;
    }

    int remaining_cash = p1.getBalance();
    if (remaining_cash > 0)
    {
        p1.addBalance(-remaining_cash);
        p2.addBalance(remaining_cash);
        pushEvent(p1.getUsername() + " transfers M" + std::to_string(remaining_cash) + " cash to " + p2.getUsername() + ".");
    }

    transferAllProperties(p1, p2);
    pushEvent("Assets from " + p1.getUsername() + " are transferred to " + p2.getUsername() + ".");
}

void EventManager::processWin(Player &player)
{
    win(player);
}

void EventManager::processBankruptcy(Player &debtor, Player *creditor)
{
    const std::vector<Player *> no_participants;
    processBankruptcy(debtor, no_participants, creditor);
}

void EventManager::processBankruptcy(Player &debtor, const std::vector<Player *> &participants, Player *creditor)
{
    bankrupt(debtor);

    if (creditor != nullptr)
    {
        transferAsset(debtor, *creditor);
        return;
    }

    // Bankrupt to bank: forfeit cash, destroy buildings, release properties, then auction.
    int remaining_cash = debtor.getBalance();
    if (remaining_cash > 0)
    {
        debtor.addBalance(-remaining_cash);
        pushEvent(debtor.getUsername() + " forfeits M" + std::to_string(remaining_cash) + " cash to the Bank.");
    }
    destroyAllBuildingsAndUnmortgage(debtor);
    auctionAll(debtor, participants);
    releasePropertiesToBank(debtor);
}

void EventManager::recordAction(Player &player, const std::string &action_name)
{
    pushEvent(player.getUsername() + " executes action: " + action_name + ".");
}

void EventManager::recordSuccess(Player &player, const std::string &detail)
{
    pushEvent(player.getUsername() + " success: " + detail);
}

void EventManager::recordFailure(Player &player, const std::string &detail)
{
    pushEvent(player.getUsername() + " failed: " + detail);
}

void EventManager::flushEventsTo(std::ostream &out)
{
    const std::vector<std::string> drained_events = drainEvents();
    for (const std::string &event_text : drained_events)
    {
        out << event_text << "\n";
    }
}

void EventManager::pushEvent(const std::string &event_text)
{
    if (!event_text.empty())
    {
        events.push_back(event_text);
    }
}

bool EventManager::hasEvents() const
{
    return !events.empty();
}

std::vector<std::string> EventManager::drainEvents()
{
    std::vector<std::string> drained_events = events;
    events.clear();
    return drained_events;
}