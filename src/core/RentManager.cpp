#include "include/core/RentManager.hpp"

#include "include/models/player/Player.hpp"
#include "include/models/tiles/PropertyTile.hpp"
#include "utils/exceptions/BankruptException.hpp"
#include "utils/exceptions/UnablePayRentException.hpp"

bool RentManager::payRent(Player &buyer, Player &owner, Tile &tile)
{
    PropertyTile *property = dynamic_cast<PropertyTile *>(&tile);
    if (property == nullptr)
    {
        throw UnablePayRentException("Target tile is not rent-bearing property.");
    }

    if (property->getTileOwner() != &owner)
    {
        throw UnablePayRentException("Specified owner does not own this property.");
    }

    if (&buyer == &owner)
    {
        throw UnablePayRentException("Player cannot pay rent to self.");
    }

    int rent = property->calculateRent();
    if (buyer.getBalance() < rent)
    {
        int payment = buyer.getBalance();
        buyer.setStatus("BANKRUPT");
        buyer.addBalance(-payment);
        owner.addBalance(payment);
        throw BankruptCausePlayerException();
    }

    buyer.addBalance(-rent);
    owner.addBalance(rent);
    return true;
}