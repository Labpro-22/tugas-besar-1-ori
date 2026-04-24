#include "include/core/BuyManager.hpp"

#include "include/models/player/Player.hpp"
#include "include/models/tiles/PropertyTile.hpp"
#include "utils/exceptions/UnablePayPropertyException.hpp"

bool BuyManager::buy(Player &buyer, Tile &tile)
{
    PropertyTile *property = dynamic_cast<PropertyTile *>(&tile);
    if (property == nullptr)
    {
        throw UnablePayPropertyException("Target tile is not a property tile.");
    }

    if (property->getTileOwner() != nullptr)
    {
        throw UnablePayPropertyException("Property already has an owner.");
    }

    int price = property->getBuyPrice();
    if (buyer.getBalance() < price)
    {
        throw UnablePayPropertyException("Insufficient balance to buy this property.");
    }

    buyer += -price;
    buyer.addOwnedProperty(property);
    return true;
}