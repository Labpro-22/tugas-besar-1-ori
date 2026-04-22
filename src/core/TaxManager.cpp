#include "include/core/TaxManager.hpp"

#include "include/models/player/Player.hpp"
#include "utils/exceptions/BankruptException.hpp"
#include "utils/exceptions/UnablePayPBMTaxException.hpp"

bool TaxManager::payTax(Player &payer, int amount)
{
    if (amount <= 0)
    {
        throw UnablePayPBMTaxException("Tax amount must be positive.");
    }

    if (payer.getBalance() < amount)
    {
        int payment = payer.getBalance();
        payer.setStatus("BANKRUPT");
        payer.addBalance(-payment);
        throw BankruptCauseBankException();
    }

    payer.addBalance(-amount);
    return true;
}