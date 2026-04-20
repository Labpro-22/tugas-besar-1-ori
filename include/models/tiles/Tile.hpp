#ifndef TILE_HPP
#define TILE_HPP

#include <string>
#include <vector>

class Tile {
    protected:
        std::string code;
        std::string id;
        std::string name;
        std::string type;

    public:
        Tile(const std::string& code, const std::string& id, const std::string& name, const std::string& type);
        virtual ~Tile() = default;
        std::string getTileCode() const;
        std::string getTileID() const;
        std::string getTileName() const;
        std::string getTileType() const;
};

#endif