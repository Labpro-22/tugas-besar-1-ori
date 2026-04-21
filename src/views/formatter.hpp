#ifndef FORMATTER_HPP
#define FORMATTER_HPP
#include <string>
#include <vector>
#include "../../include/models/board/Board.hpp"
#include "../../include/models/player/Player.hpp"
#include "../../include/models/log-entry/LogEntry.hpp"
using namespace std;

class OutputFormatter {
    private:
        const string RESET   = "\033[0m";
        const string RED     = "\033[31m";
        const string GREEN   = "\033[32m";
        const string YELLOW  = "\033[33m";
        const string BLUE    = "\033[34m";
        const string MAGENTA = "\033[35m";
        const string WHITE   = "\033[37m";
        string getGroupColor(string colorGroup);
        string getColorCode(string colorGroup, string tileType);
        string centerOut(string str, int width);
        string leftOut(string str, int width);
        string colorText(string str, string color);
        vector<string> renderTile(string line1, string line2, string colorGroup, int w, int h);
    public:
        void printBoard(Board &b, vector<Player*> players = {}, int turn = 0, int maxTurn = 0);
        void printAkta(PropertyTile &t);
        void printProperty(Player &p);
        void printLog(vector<LogEntry> &log, int n = -1);
        void printPlayerStatus(Player &p);
        void printAuction();
        void printWin(vector<Player> &ps, bool isBankruptcy = false);
};

#endif
