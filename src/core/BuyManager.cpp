#include "include/core/BuyManager.hpp"
#include "include/models/tiles/PropertyTile.hpp"

bool BuyManager::buy(Player &buyer, Tile &tile) {
    auto *prop = dynamic_cast<PropertyTile*>(&tile);

    if (!prop) {
        return false;
    }

    if (prop->getTileOwner()) {
        return false;
    }

    if (buyer.getBalance() < prop->getBuyPrice()) {
        return false;
    }

    buyer += -prop->getBuyPrice();
    buyer.addOwnedProperty(prop);

    return true;
}