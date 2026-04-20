#include "include/models/card/ChanceCard.hpp"
#include "include/models/player/Player.hpp"
#include <climits>
#include <iostream>

StepbackCard::StepbackCard(int boardSize) : ChanceCard(), boardSize(boardSize) {}

void StepbackCard::action(Player& player) {
    int currentTile = player.getCurrTile();
    int newTile = (currentTile - 3 + boardSize) % boardSize;
    player.setCurrTile(newTile);

    std::cout << player.getUsername() << " mundur 3 petak ke petak " << newTile << "." << std::endl;
}

NearestStreetCard::NearestStreetCard(int boardSize, std::vector<int> stationPositions)
    : ChanceCard(), boardSize(boardSize), stationPositions(stationPositions) {}

void NearestStreetCard::action(Player& player) {
    int currentTile = player.getCurrTile();
    int nearestStation = stationPositions[0];
    int minDistance = INT_MAX;

    for (int i = 0; i < static_cast<int>(stationPositions.size()); i++) {
        int forwardDist = (stationPositions[i] - currentTile + boardSize) % boardSize;
        if (forwardDist == 0) forwardDist = boardSize;
        if (forwardDist < minDistance) {
            minDistance = forwardDist;
            nearestStation = stationPositions[i];
        }
    }

    player.setCurrTile(nearestStation);
    std::cout << player.getUsername() << " pergi ke stasiun terdekat di petak " << nearestStation << "." << std::endl;
}

JailCard::JailCard(int jailPosition) : ChanceCard(), jailPosition(jailPosition) {}

void JailCard::action(Player& player) {
    player.setCurrTile(jailPosition);
    player.setStatus("jail");

    std::cout << player.getUsername() << " masuk penjara!" << std::endl;
}