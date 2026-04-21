#include <iostream>
#include <vector>
#include <string>
#include "views/formatter.hpp"
#include "models/board/Board.hpp"
#include "models/tiles/Tile.hpp"
#include "models/tiles/PropertyTile.hpp"

// color group assigned per tile index for a standard 40-tile board
static std::string colorGroupFor(int idx) {
    switch (idx) {
        case 1: case 3:             return "UNGU";
        case 6: case 8: case 9:    return "CYAN";
        case 11: case 13: case 14: return "MERAH";
        case 16: case 18: case 19: return "KUNING";
        case 21: case 23: case 24: return "MERAH";
        case 26: case 27: case 29: return "KUNING";
        case 31: case 32: case 34: return "HIJAU";
        case 37: case 39:          return "BIRU TUA";
        default:                   return "";
    }
}

static std::vector<Tile*> makeTiles(int count) {
    std::vector<Tile*> tiles;
    for (int i = 0; i < count; i++) {
        std::string idx  = (i < 10 ? "0" : "") + std::to_string(i);
        std::string code = "T" + idx;
        std::string name = "Tile " + idx;
        std::string cg   = colorGroupFor(i);

        if (!cg.empty()) {
            tiles.push_back(new PropertyTile(
                code, code, name, "PROPERTY",
                {10, 30, 90, 160, 250, 300}, // rent_price
                30, 0, false, cg,
                60, 50, 50, 50, 1, 0
            ));
        } else {
            tiles.push_back(new Tile(code, code, name, "GENERIC"));
        }
    }
    return tiles;
}

int main() {
    int tileCount = 40;
    std::vector<Tile*> tiles = makeTiles(tileCount);

    Board board(tiles);
    OutputFormatter fmt;
    fmt.printBoard(board);

    for (auto t : tiles) delete t;
    return 0;
}
