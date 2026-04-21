#include "include/models/card/SpecialPowerCard.hpp"
#include "include/models/player/Player.hpp"

namespace
{
    int normalizePosition(int position, int board_size)
    {
        if (board_size <= 0)
        {
            return position;
        }

        int normalized_position = position % board_size;
        if (normalized_position < 0)
        {
            normalized_position += board_size;
        }
        return normalized_position;
    }

    float clampDiscountFraction(int value)
    {
        if (value <= 0)
        {
            return 0.0F;
        }
        if (value >= 100)
        {
            return 1.0F;
        }
        return static_cast<float>(value) / 100.0F;
    }
} // namespace

MoveCard::MoveCard(int value, int boardSize) : SpecialPowerCard(), value(value), boardSize(boardSize) {}

int MoveCard::getMoveValue() const { return value; }

void MoveCard::action(Player &player)
{
    int currentTile = player.getCurrTile();
    int newTile = normalizePosition(currentTile + value, boardSize);
    player.setCurrTile(newTile);
    player.setSkillUsed(true);
}

DiscountCard::DiscountCard(int value) : SpecialPowerCard(), value(value) {}

int DiscountCard::getDiscountValue() const { return value; }

void DiscountCard::action(Player &player)
{
    float discountFraction = clampDiscountFraction(value);
    player.setDiscountActive(discountFraction);
    player.setSkillUsed(true);
}

ShieldCard::ShieldCard() : SpecialPowerCard() {}

void ShieldCard::action(Player &player)
{
    player.setShieldActive(true);
    player.setSkillUsed(true);
}

TeleportCard::TeleportCard() : SpecialPowerCard() {}

void TeleportCard::action(Player &player)
{
    player.setSkillUsed(true);
}

LassoCard::LassoCard() : SpecialPowerCard() {}

void LassoCard::action(Player &player)
{
    player.setSkillUsed(true);
}

DemolitionCard::DemolitionCard() : SpecialPowerCard() {}

void DemolitionCard::action(Player &player)
{
    player.setSkillUsed(true);
}