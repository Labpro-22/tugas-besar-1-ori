#ifndef FORMATTER_HPP
#define FORMATTER_HPP
#include <string>
#include <vector>
#include <map>
#include "../../include/models/board/Board.hpp"
#include "../../include/models/player/Player.hpp"
#include "../../include/models/log-entry/LogEntry.hpp"
#include "../../include/core/GameConfig.hpp"
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
        // Predefined color mappings for known color groups
        const map<string, string> known_ansi = {
            {"COKLAT",     "\033[38;5;94m"},   // brown
            {"BIRU_MUDA",  "\033[38;5;45m"},   // light blue
            {"MERAH_MUDA", "\033[38;5;201m"},  // pink
            {"ORANGE",     "\033[38;5;208m"},  // orange
            {"MERAH",      "\033[38;5;196m"},  // red
            {"KUNING",     "\033[38;5;226m"},  // yellow
            {"HIJAU",      "\033[38;5;46m"},   // green
            {"BIRU_TUA",   "\033[38;5;27m"},   // dark blue
        };
        const map<string, string> known_codes = {
            {"COKLAT",     "CK"},
            {"BIRU_MUDA",  "BM"},
            {"MERAH_MUDA", "PK"},
            {"ORANGE",     "OR"},
            {"MERAH",      "MR"},
            {"KUNING",     "KN"},
            {"HIJAU",      "HJ"},
            {"BIRU_TUA",   "BT"},
        };
        vector<string> color_palette = {
            "\033[38;5;196m",  // merah terang
            "\033[38;5;27m",   // biru royal
            "\033[38;5;46m",   // hijau neon
            "\033[38;5;226m",  // kuning terang
            "\033[38;5;201m",  // pink/magenta
            "\033[38;5;51m",   // cyan/aqua
            "\033[38;5;208m",  // oranye
            "\033[38;5;94m",   // coklat/cokelat
            "\033[38;5;129m",  // ungu
            "\033[38;5;82m",   // hijau limau
            "\033[38;5;33m",   // biru langit
            "\033[38;5;166m",  // oranye tua
            "\033[38;5;45m",   // biru muda
            "\033[38;5;124m",  // merah gelap
            "\033[38;5;28m",   // hijau tua
            "\033[38;5;171m",  // lavender
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
        void printAkta(PropertyTile &t, Board &b, const GameConfig &cfg);
        void printProperty(Player &p, Board &b);
        void printLog(vector<LogEntry> &log, int n = -1);
        void printPlayerStatus(Player &p);
        void printAuction();
        void printWin(vector<Player> &ps, bool is_bankruptcy = false);
};

#endif
