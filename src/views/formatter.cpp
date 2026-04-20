#include "formatter.hpp"
#include "../../include/models/board/Board.hpp"
#include "../../include/models/player/Player.hpp"
#include "../../include/models/tiles/Tile.hpp"
#include "../../include/models/log-entry/LogEntry.hpp"
#include <vector>
#include <map>
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
string OutputFormatter::colorText(string str, string color){
    return color + str + RESET;
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


vector<string> OutputFormatter::renderTile(string code, string name, string colorGroup, int w, int h) {
    string color  = getGroupColor(colorGroup);
    string border = color + "+" + string(w, '-') + "+" + RESET;
    string empty  = color + "|" + string(w, ' ') + "|" + RESET;

    // truncate name to fit (leave 1 space prefix)
    if((int)name.size() > w - 1) name = name.substr(0, w - 1);

    vector<string> lines;
    lines.push_back(border);
    lines.push_back(color + "|" + centerOut(code, w) + "|" + RESET);
    lines.push_back(color + "|" + leftOut(" " + name, w) + "|" + RESET);
    while((int)lines.size() < h - 1) lines.push_back(empty);
    lines.push_back(border);
    return lines;
}

//------------------------------Public Methods-----------------------------------

void OutputFormatter::printBoard(Board &b) {
    const int TW = 10, TH = 4;  // normal tile inner width, height
    const int CW = 14, CH = 6;  // corner tile inner width, height

    // TODO: get tiles from board
    // vector<Tile*> tiles = b.getTiles();
    // Tile index layout (clockwise from GO at bottom-right):
    //   BR=0, bottom[1..sideH], BL=sideH+1
    //   left[sideH+2..sideH+sideV+1], TL=sideH+sideV+2
    //   top[sideH+sideV+3..2*sideH+sideV+2], TR=2*sideH+sideV+3
    //   right[2*sideH+sideV+4..2*sideH+2*sideV+3]
    // nxn   : sideH == sideV
    // nxn+1 : sideH == sideV + 1 (more tiles on top/bottom)
    int sideH = 9, sideV = 9; // TODO: derive from tiles.size()

    // placeholder — replace args with tiles[idx]->getCode(), getName(), getColorGroup()
    auto mkTile = [&](int /*idx*/, int w, int h) {
        return renderTile("???", "TILE", "", w, h);
    };

    // ── TOP ROW ──────────────────────────────────────────────────────────────
    int TL = sideH + sideV + 2;
    int TR = 2*sideH + sideV + 3;

    // all tiles in top row rendered at CH height so rows align
    vector<vector<string>> topRow;
    topRow.push_back(mkTile(TL, CW, CH));
    for(int i = TL+1; i < TR; i++) topRow.push_back(mkTile(i, TW, CH));
    topRow.push_back(mkTile(TR, CW, CH));

    for(int l = 0; l < CH; l++){
        for(auto& t : topRow) cout << t[l];
        cout << "\n";
    }

    // ── MIDDLE ROWS ───────────────────────────────────────────────────────────
    int centerW = sideH * (TW + 2); // total width of center box
    for(int row = 0; row < sideV; row++){
        int leftIdx  = sideH + sideV + 1 - row; // left col top-to-bottom on screen
        int rightIdx = 2*sideH + sideV + 4 + row;
        vector<string> L = mkTile(leftIdx,  TW, TH);
        vector<string> R = mkTile(rightIdx, TW, TH);

        for(int l = 0; l < TH; l++){
            cout << L[l];
            bool firstLine = (row == 0 && l == 0);
            bool lastLine  = (row == sideV-1 && l == TH-1);
            if(firstLine || lastLine)
                cout << "+" + string(centerW - 2, '-') + "+";
            else
                cout << "|" + string(centerW - 2, ' ') + "|";
            cout << R[l] << "\n";
        }
    }

    // ── BOTTOM ROW ────────────────────────────────────────────────────────────
    int BL = sideH + 1;

    // all tiles in bottom row also rendered at CH height
    vector<vector<string>> botRow;
    botRow.push_back(mkTile(BL, CW, CH));
    for(int i = sideH; i >= 1; i--) botRow.push_back(mkTile(i, TW, CH));
    botRow.push_back(mkTile(0, CW, CH)); // BR = GO

    for(int l = 0; l < CH; l++){
        for(auto& t : botRow) cout << t[l];
        cout << "\n";
    }
}

void OutputFormatter::printAkta(PropertyTile &t){
    string color = getGroupColor("RESET"); //TODO: replace with getGroupColor(t.getColorGroup())
    cout << color;
    cout << "+================================+\n";
    cout << "|" << centerOut("AKTA KEPEMILIKAN",32) << "|\n";
    string tmp = "[t.getColorGroup()] t.getName() (t.getCode())"; //TODO: replace with actual calls
    cout << "|" << centerOut(tmp,32) << "|\n";
    cout << "+================================+\n";
    cout << "|" << leftOut(" Harga Beli",  22) << " :" << leftOut(" MXXX", 8) << "|\n";
    cout << "|" << leftOut(" Nilai Gadai", 22) << " :" << leftOut(" MXXX", 8) << "|\n";
    cout << "+--------------------------------+\n";
    cout << "|" << leftOut(" Sewa (unimproved)", 22) << " :" << leftOut(" MXXX", 8) << "|\n";
    cout << "|" << leftOut(" Sewa (1 rumah)",    22) << " :" << leftOut(" MXXX", 8) << "|\n";
    cout << "|" << leftOut(" Sewa (2 rumah)",    22) << " :" << leftOut(" MXXX", 8) << "|\n";
    cout << "|" << leftOut(" Sewa (3 rumah)",    22) << " :" << leftOut(" MXXX", 8) << "|\n";
    cout << "|" << leftOut(" Sewa (4 rumah)",    22) << " :" << leftOut(" MXXX", 8) << "|\n";
    cout << "|" << leftOut(" Sewa (Hotel)",      22) << " :" << leftOut(" MXXX", 8) << "|\n";
    cout << "+--------------------------------+\n";
    cout << "|" << leftOut(" Harga Rumah", 22) << " :" << leftOut(" MXXX", 8) << "|\n";
    cout << "|" << leftOut(" Harga Hotel", 22) << " :" << leftOut(" MXXX", 8) << "|\n";
    cout << "+================================+\n";
    cout << "|" << leftOut(" Status : STATUS (PEMAIN X)", 32) << "|\n";
    cout << "+================================+\n";
    cout << RESET;
}

void OutputFormatter::printProperty(Player &p){
    vector<PropertyTile*> props = p.getOwnedProperties(); //TODO: adjust to actual method name

    if(props.empty()){
        cout << "Kamu belum memiliki properti apapun.\n";
        return;
    }

    cout << "=== Properti Milik: " << p.getUsername() << " ===\n\n"; //TODO: adjust to actual method name

    // group by color group
    map<string, vector<PropertyTile*>> grouped;
    for(auto& tile : props){
        grouped[tile->getColorGroup()].push_back(tile); //TODO: adjust to actual method name
    }

    int total = 0;
    for(auto& [group, tiles] : grouped){
        string color = getGroupColor(group);
        cout << color << "[" << group << "]" << getGroupColor("RESET") << "\n";
        for(auto& tile : tiles){
            string status = tile->isMortgage() ? "MORTGAGED [M]" : "OWNED"; //TODO: adjust to actual method name
            string label = tile->getName() + " (" + tile->getCode() + ")"; //TODO: adjust to actual method name
            cout << "  - " << leftOut(label, 30) << leftOut("MXXX", 8) << status << "\n";
            total += tile->getBuyPrice(); //TODO: adjust to actual method name
        }
        cout << "\n";
    }

    cout << "Total kekayaan properti: M" << total << "\n";
}

