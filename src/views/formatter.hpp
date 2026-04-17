#ifndef FORMATTER_HPP
#define FORMATTER_HPP
#include <string>
#include <vector>
using namespace std;

class OutputFormatter {
    private:
        const string RESET     = "\033[0m";
        const string RED       = "\033[31m";
        const string GREEN     = "\033[32m";
        const string YELLOW    = "\033[33m";
        const string BLUE      = "\033[34m";
        const string MAGENTA   = "\033[35m";
        const string CYAN      = "\033[36m";
        const string WHITE     = "\033[37m";
        const string BOLD_RED  = "\033[1;31m";
        const string BOLD_BLUE = "\033[1;34m";

        string centerOut(string str, int width);
        string leftOut(string str, int width);
    public:
        void printBoard(Board &b);
        void printAkta(PropertyTile &t);
        void printProperty(Player &p);
        void printLog(vector<LogEntry> &log);
        void printPlayerStatus(Player &p);
        void printAuction();
        void printWin(vector<Player> &ps);
};

#endif
