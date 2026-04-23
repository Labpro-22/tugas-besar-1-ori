#include "include/models/card/ChanceCard.hpp"
#include "include/models/player/Player.hpp"
#include <climits>

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
} // namespace

StepbackCard::StepbackCard(int boardSize) : ChanceCard(), boardSize(boardSize) {}
std::string StepbackCard::describe() const { return "Mundur 3 petak"; }
std::string NearestStreetCard::describe() const { return "Pindah ke stasiun/rel terdekat"; }
std::string JailCard::describe() const { return "Masuk penjara! Langsung ke penjara, tidak melewati GO"; }

void StepbackCard::action(Player &player)
{
    if (boardSize <= 0)
    {
        return;
    }

    int currentTile = player.getCurrTile();
    int newTile = normalizePosition(currentTile - 3, boardSize);
    player.setCurrTile(newTile);
}

NearestStreetCard::NearestStreetCard(int boardSize, std::vector<int> stationPositions)
    : ChanceCard(), boardSize(boardSize), stationPositions(stationPositions) {}

void NearestStreetCard::action(Player &player)
{
    if (boardSize <= 0 || stationPositions.empty())
    {
        return;
    }

    int currentTile = player.getCurrTile();
    int nearestStation = normalizePosition(stationPositions[0], boardSize);
    int minDistance = INT_MAX;

    for (int stationPosition : stationPositions)
    {
        int normalizedStation = normalizePosition(stationPosition, boardSize);
        int forwardDist = (normalizedStation - currentTile + boardSize) % boardSize;
        if (forwardDist == 0)
            forwardDist = boardSize;
        if (forwardDist < minDistance)
        {
            minDistance = forwardDist;
            nearestStation = normalizedStation;
        }
    }

    player.setCurrTile(nearestStation);
}

JailCard::JailCard(int jailPosition) : ChanceCard(), jailPosition(jailPosition) {}

void JailCard::action(Player &player)
{
    player.setCurrTile(jailPosition);
    player.setStatus("JAIL");
}