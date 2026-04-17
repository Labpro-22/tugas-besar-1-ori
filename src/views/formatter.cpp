#include "formatter.hpp"
#include "../../include/models/board/Board.hpp"
#include "../../include/models/player/Player.hpp"
#include "../../include/models/tiles/Tile.hpp"
#include "../../include/models/log-entry/LogEntry.hpp"
#include <vector>
#include <iostream>
#include <iomanip>
using namespace std;

string OutputFormatter::centerOut(string str, int width){
    int text_length = str.size();

    //guard
    if(text_length > width){
        return "[ERROR] : Text Length > width !!!\n";
    }

    int lpad = (width - text_length)/2; //floor
    int rpad = width - text_length - lpad; //handles odd width 
    return string(lpad, ' ') + str + string(rpad, ' ');
}
string OutputFormatter::leftOut(string str, int width){
    int text_length = str.size();

    //guard
    if(text_length > width){
        return "[ERROR] : Text Length > width !!!\n";
    }

    int rpad = width - text_length;
    return str + string(rpad, ' ');
}
string OutputFormatter::colorText(string str,string color){
    return getGroupColor(color) + str + getGroupColor("RESET");
}
string OutputFormatter::getGroupColor(string colorGroup){
    if (colorGroup == "BIRU TUA") return "\033[1;34m";
    if (colorGroup == "MERAH")    return "\033[1;31m";
    if (colorGroup == "HIJAU")    return "\033[1;32m";
    if (colorGroup == "KUNING")   return "\033[1;33m";
    if (colorGroup == "UNGU")     return "\033[1;35m";
    if (colorGroup == "CYAN")     return "\033[1;36m";
    return "\033[0m"; //the undefined color will automatically turn to default to prevent error
}


//------------------------------Public Methods-----------------------------------

void OutputFormatter::printBoard(Board &b) {
    //nantian
}

void OutputFormatter::printAkta(PropertyTile &t){
    cout << getGroupColor("RESET");//TODO: replace with t.getColorGroup();
    cout << "+================================+";
    cout << "|" << centerOut("AKTA KEPEMILIKAN",32) << "|\n";
    string tmp = "[t.getColorGroup()] t.getName() (t.getCode())";
    cout << "|" << centerOut(tmp,32) << "|\n";
    cout << "+================================+";
    cout << "|" << leftOut(" Harga beli",22) << " :" << leftOut("MXXX",8);
    cout << "|" << leftOut(" Nilai Gadar",22) << " :" << leftOut("MXXX",8);
    cout << "+--------------------------------+";
    cout << "|" << leftOut(" Sewa (unimproved)",22) << " :" << leftOut("MXXX",8);
    cout << "|" << leftOut(" Sewa (1 rumah)",22) << " :" << leftOut("MXXX",8);
    cout << "|" << leftOut(" Sewa (2 rumah)",22) << " :" << leftOut("MXXX",8);
    cout << "|" << leftOut(" Sewa (3 rumah)",22) << " :" << leftOut("MXXX",8);
    cout << "|" << leftOut(" Sewa (4 rumah)",22) << " :" << leftOut("MXXX",8);
    cout << "|" << leftOut(" Sewa (Hotel)",22) << " :" << leftOut("MXXX",8);
    cout << "+--------------------------------+";
    cout << "|" << leftOut(" Harga Rumah",22) << " :" << leftOut("MXXX",8);
    cout << "|" << leftOut(" Harga Hotel",22) << " :" << leftOut("MXXX",8);
    cout << "+================================+";
    cout << "|" << leftOut(" Status : STATUS (PEMAIN X)",32);
    cout << "+================================+";
    cout << getGroupColor("RESET");
    //kalau kode tidak ditemukan PETAK XYZ TIDAK DITEMUKAN/BUKAN PROPERTI
}

