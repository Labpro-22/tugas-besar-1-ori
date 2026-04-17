#ifndef FORMATTER_HPP
#define FORMATTER_HPP
#include <string>
#include <vector>
using namespace std;

class OutputFormatter {
    private:
        string getGroupColor(string colorGroup);
        //To Print a centered text with padding
        string centerOut(string str, int width);
        //To Print a left text with padding
        string leftOut(string str, int width);
        //To print a colored text
        string colorText(string str,string color);
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
