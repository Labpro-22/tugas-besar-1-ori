#ifndef FORMATTER_HPP
#define FORMATTER_HPP
#include <string>
#include <vector>
#include <map>
#include "../../include/models/board/Board.hpp"
#include "../../include/models/player/Player.hpp"
#include "../../include/models/log-entry/LogEntry.hpp"
using namespace std;

class OutputFormatter {
    private:
        const string reset   = "\033[0m";
        const string red     = "\033[31m";
        const string green   = "\033[32m";
        const string yellow  = "\033[33m";
        const string blue    = "\033[34m";
        const string magenta = "\033[35m";
        const string cyan    = "\033[36m";
        const string white   = "\033[37m";

        map<string, string> color_group_ansi;
        map<string, string> color_group_codes;
        vector<string> color_palette = {
            "\033[1;34m", "\033[1;31m", "\033[1;32m", "\033[1;33m",
            "\033[1;35m", "\033[1;36m", "\033[0;33m", "\033[38;5;208m", "\033[1;90m"
        };
        int next_color_idx = 0;

        void initializeColors(Board &b);
        string generateCode(string color_group);

        string getGroupColor(string color_group);
        string getColorCode(string color_group, string tile_type);
        string centerOut(string str, int width);
        string leftOut(string str, int width);
        string colorText(string str, string color);
        vector<string> renderTile(string line_1, string line_2, string color_group, string tile_type, int w, int h);
    public:
        void printBoard(Board &b, vector<Player*> players = {}, int turn = 0, int max_turn = 0);
        void printAkta(PropertyTile &t, Board &b);
        void printProperty(Player &p, Board &b);
        void printLog(vector<LogEntry> &log, int n = -1);
        void printPlayerStatus(Player &p);
        void printAuction();
        void printWin(vector<Player> &ps, bool is_bankruptcy = false);
};

#endif
