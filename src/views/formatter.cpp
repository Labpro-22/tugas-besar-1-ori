#include "formatter.hpp"
#include "../../include/models/board/Board.hpp"
#include "../../include/models/player/Player.hpp"
#include "../../include/models/tiles/Tile.hpp"
#include "../../include/models/log-entry/LogEntry.hpp"
#include <vector>
#include <iostream>
#include <iomanip>
using namespace std;

//PRIVATE : To Print a centered text with padding
string OutputFormatter::centerOut(string str, int width){
    int text_length = str.size();

    //guard
    if(text_length > width){
        cerr << "[ERROR] : Text Length > width !!!\n";
        return;
    }

    int lpad = (width - text_length)/2; //floor
    int rpad = width - text_length - lpad; //handles odd width 
    return string(lpad, ' ') + str + string(rpad, ' ');
}
//PRIVATE : To Print a left text with padding
string OutputFormatter::centerOut(string str, int width){
    int text_length = str.size();

    //guard
    if(text_length > width){
        cerr << "[ERROR] : Text Length > width !!!\n";
        return;
    }

    int rpad = width - text_length;
    return str + string(rpad, ' ');
}

void OutputFormatter::printBoard(Board &b) {
    //nantian
}

void OutputFormatter::printAkta(PropertyTile &t){
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

    //kalau kode tidak ditemukan PETAK XYZ TIDAK DITEMUKAN/BUKAN PROPERTI
}

