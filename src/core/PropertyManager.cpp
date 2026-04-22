#include "include/core/PropertyManager.hpp"

#include "include/core/RentManager.hpp"
#include "utils/exceptions/UnableBuildException.hpp"
#include "utils/exceptions/UnableCompensateException.hpp"
#include "utils/exceptions/UnablePawnException.hpp"
#include "utils/exceptions/UnablePayBuildException.hpp"

#include <algorithm>
#include <climits>

namespace
{
    bool propertyOwnedBy(const PropertyTile &tile, const Player &owner)
    {
        return tile.getTileOwner() == &owner;
    }

    int buildCostFor(const PropertyTile &tile)
    {
        if (tile.getLevel() >= 3)
        {
            return tile.getHotelCost();
        }

        return tile.getHouseCost();
    }
}

bool PropertyManager::mortgage(Player &owner, PropertyTile &tile)
{
    if (!propertyOwnedBy(tile, owner))
    {
        throw UnablePawnNoPropertyException("Only the owner can mortgage this property.");
    }

    if (tile.getLevel() > 0)
    {
        throw UnablePawnExistBuildingException();
    }

    if (tile.isMortgage())
    {
        throw UnablePawnException("Property is already mortgaged.");
    }

    tile.setMortgageStatus(true);
    owner.addBalance(tile.getMortgageValue());
    return true;
}

bool PropertyManager::build(Player &owner, PropertyTile &tile)
{
    if (!propertyOwnedBy(tile, owner))
    {
        throw UnableBuildException("Only the owner can build on this property.");
    }

    if (tile.isMortgage())
    {
        throw UnableBuildException("Cannot build on a mortgaged property.");
    }

    int cost = buildCostFor(tile);
    if (owner.getBalance() < cost)
    {
        throw UnablePayBuildException();
    }

    owner.addBalance(-cost);
    tile.setLevel(tile.getLevel() + 1);
    return true;
}

bool PropertyManager::compensate(Player &player, Tile &tile)
{
    PropertyTile *property = dynamic_cast<PropertyTile *>(&tile);
    if (property == nullptr)
    {
        throw UnableCompensateException("Target tile is not a property tile.");
    }

    Player *owner = property->getTileOwner();
    if (owner == nullptr || owner == &player)
    {
        throw UnableCompensateException("No valid counterparty for compensation.");
    }

    return RentManager::payRent(player, *owner, tile);
}

bool PropertyManager::liquidate(Player &player, Tile &tile)
{
    PropertyTile *property = dynamic_cast<PropertyTile *>(&tile);
    if (property == nullptr || property->getTileOwner() != &player)
    {
        throw UnablePawnNoPropertyException("Player does not own the selected property.");
    }

    if (property->isMortgage())
    {
        throw UnablePawnException("Property is already mortgaged.");
    }

    if (property->getLevel() > 0)
    {
        throw UnablePawnExistBuildingException();
    }

    property->setMortgageStatus(true);
    player.addBalance(property->getMortgageValue());
    return true;
}

int PropertyManager::calculateMaxLiquidation(Player &player)
{
    long long total = player.getBalance();

    const std::vector<PropertyTile *> properties = player.getOwnedProperties();
    for (PropertyTile *property : properties)
    {
        if (property == nullptr)
        {
            continue;
        }

        if (!property->isMortgage())
        {
            total += property->getMortgageValue();
        }
    }

    if (total < 0)
    {
        return 0;
    }

    return static_cast<int>(std::min<long long>(total, INT_MAX));
}