#include "include/core/AuctionManager.hpp"

#include "utils/exceptions/NoPropertyException.hpp"

#include <algorithm>
#include <utility>

namespace
{
    int resolveMinimumBid(const PropertyTile &tile, int minimum_bid)
    {
        if (minimum_bid > 0)
        {
            return minimum_bid;
        }

        return std::max(1, tile.getMortgageValue());
    }

    bool isEligibleBidder(Player *candidate, Player &seller, int minimum_bid)
    {
        if (candidate == nullptr || candidate == &seller)
        {
            return false;
        }

        if (candidate->getStatus() == "BANKRUPT")
        {
            return false;
        }

        return candidate->getBalance() >= minimum_bid;
    }
} // namespace

AuctionManager::AuctionResult AuctionManager::auctionProperty(
    Player &seller,
    PropertyTile &tile,
    const std::vector<Player *> &participants,
    int minimum_bid)
{
    if (tile.getTileOwner() != &seller)
    {
        throw NoPropertyException("Only the owner can auction this property.");
    }

    const int opening_bid = resolveMinimumBid(tile, minimum_bid);

    std::vector<Player *> eligible_bidders;
    eligible_bidders.reserve(participants.size());

    for (Player *participant : participants)
    {
        if (isEligibleBidder(participant, seller, opening_bid))
        {
            eligible_bidders.push_back(participant);
        }
    }

    seller.removeOwnedProperty(tile.getTileCode());
    tile.setMortgageStatus(false);
    tile.setMonopolized(false);

    if (eligible_bidders.empty())
    {
        return AuctionResult{};
    }

    std::sort(
        eligible_bidders.begin(),
        eligible_bidders.end(),
        [](Player *left, Player *right)
        {
            if (left->getBalance() == right->getBalance())
            {
                return left->getUsername() < right->getUsername();
            }

            return left->getBalance() > right->getBalance();
        });

    Player *winner = eligible_bidders.front();
    int second_best_balance = opening_bid - 1;
    if (eligible_bidders.size() > 1)
    {
        second_best_balance = eligible_bidders[1]->getBalance();
    }

    int final_bid = std::max(opening_bid, second_best_balance + 1);
    final_bid = std::min(final_bid, winner->getBalance());

    winner->addBalance(-final_bid);
    winner->addOwnedProperty(&tile);

    AuctionResult result;
    result.sold = true;
    result.winner = winner;
    result.final_bid = final_bid;
    return result;
}