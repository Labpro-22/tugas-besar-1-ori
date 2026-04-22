#include "OutputFormatter.hpp"
#include "../../include/models/tiles/PropertyTile.hpp"
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
    if (colorGroup == "BIRU TUA" || colorGroup == "BT") return "\033[1;34m";
    if (colorGroup == "MERAH"    || colorGroup == "MR") return "\033[1;31m";
    if (colorGroup == "HIJAU"    || colorGroup == "HJ") return "\033[1;32m";
    if (colorGroup == "KUNING"   || colorGroup == "KN") return "\033[1;33m";
    if (colorGroup == "UNGU"     || colorGroup == "PK" || colorGroup == "PINK") return "\033[1;35m";
    if (colorGroup == "CYAN"     || colorGroup == "BM" || colorGroup == "BIRU MUDA") return "\033[1;36m";
    if (colorGroup == "COKLAT"   || colorGroup == "CK") return "\033[0;33m";
    if (colorGroup == "ORANGE"   || colorGroup == "OR") return "\033[38;5;208m";
    return "\033[0m";
}
string OutputFormatter::getColorCode(string colorGroup, string tileType){
    if (colorGroup == "BIRU TUA" || colorGroup == "BT") return "BT";
    if (colorGroup == "MERAH"    || colorGroup == "MR")  return "MR";
    if (colorGroup == "HIJAU"    || colorGroup == "HJ")  return "HJ";
    if (colorGroup == "KUNING"   || colorGroup == "KN")  return "KN";
    if (colorGroup == "UNGU"     || colorGroup == "PK" || colorGroup == "PINK") return "PK";
    if (colorGroup == "CYAN"     || colorGroup == "BM" || colorGroup == "BIRU MUDA") return "BM";
    if (colorGroup == "COKLAT"   || colorGroup == "CK")  return "CK";
    if (colorGroup == "ORANGE"   || colorGroup == "OR") return "OR";
    if (tileType == "UTILITY"    || tileType == "AB")  return "AB";
    return "DF";
}


vector<string> OutputFormatter::renderTile(string line1, string line2, string colorGroup, int w, int h) {
    string color  = getGroupColor(colorGroup);
    string border = color + "+" + string(w, '-') + "+" + RESET;
    string empty  = color + "|" + string(w, ' ') + "|" + RESET;

    if((int)line1.size() > w) line1 = line1.substr(0, w);
    if((int)line2.size() > w) line2 = line2.substr(0, w);

    vector<string> lines;
    lines.push_back(border);
    lines.push_back(color + "|" + leftOut(line1, w) + "|" + RESET);
    lines.push_back(color + "|" + leftOut(line2, w) + "|" + RESET);
    while((int)lines.size() < h - 1) lines.push_back(empty);
    lines.push_back(border);
    return lines;
}

//------------------------------Public Methods-----------------------------------

void OutputFormatter::printBoard(Board &b, vector<Player*> players, int turn, int maxTurn) {
    const int TW = 10, TH = 4;

    int total = b.getTileCount();
    int sideH, sideV;
    if ((total - 4) % 4 == 0) {
        sideH = sideV = (total - 4) / 4;
    } else {
        sideV = (total - 6) / 4;
        sideH = sideV + 1;
    }

    int centerW = sideH * (TW + 2);
    int iw = centerW - 2;

    // ── BUILD CENTER LEGEND ───────────────────────────────────────────────────
    const int BOX = min(34, iw - 4);
    string eqLine  = string(BOX, '=');
    string daLine  = string(BOX, '-');

    auto padC = [&](string s) -> string {
        if ((int)s.size() >= iw) return s.substr(0, iw);
        int lp = (iw - (int)s.size()) / 2;
        return string(lp, ' ') + s + string(iw - (int)s.size() - lp, ' ');
    };
    int boxOffset = (iw - BOX) / 2;
    auto padL = [&](string s) -> string {
        string r = string(boxOffset, ' ') + s;
        if ((int)r.size() < iw) r += string(iw - (int)r.size(), ' ');
        return r.substr(0, iw);
    };

    string titleBox = "||" + centerOut("NIMONSPOLI", BOX - 4) + "||";
    string turnStr  = (turn > 0 && maxTurn > 0)
                      ? "TURN " + to_string(turn) + " / " + to_string(maxTurn) : "";

    vector<string> centerContent = {
        string(iw, ' '),
        padC(eqLine),
        padC(titleBox),
        padC(eqLine),
        string(iw, ' '),
        padC(turnStr),
        string(iw, ' '),
        padC(daLine),
        padL("LEGENDA KEPEMILIKAN & STATUS"),
        padL("P1-P4 : Properti milik Pemain 1-4"),
        padL("^     : Rumah Level 1"),
        padL("^^    : Rumah Level 2"),
        padL("^^^   : Rumah Level 3"),
        padL("*     : Hotel (Maksimal)"),
        padL("(1)-(4): Bidak (IN=Tahanan, V=Mampir)"),
        padC(daLine),
        padL("KODE WARNA:"),
        padL("[CK]=Coklat    [MR]=Merah"),
        padL("[BM]=Biru Muda [KN]=Kuning"),
        padL("[PK]=Pink      [HJ]=Hijau"),
        padL("[OR]=Orange    [BT]=Biru Tua"),
        padL("[DF]=Aksi      [AB]=Utilitas"),
        padC(daLine),
    };
    int numInner = sideV * TH - 2;
    while ((int)centerContent.size() < numInner) centerContent.push_back(string(iw, ' '));

    // ── TILE BUILDER ──────────────────────────────────────────────────────────
    auto mkTile = [&](int idx, int w, int h) {
        Tile* t = b.getTileByIndex(idx);
        PropertyTile* pt = dynamic_cast<PropertyTile*>(t);
        string colorGroup = pt ? pt->getColorGroup() : "";
        string colorCode  = getColorCode(colorGroup, t->getTileType());
        string line1 = "[" + colorCode + "] " + t->getTileCode();

        string line2 = "";
        if (pt && pt->getTileOwner() != nullptr) {
            string pNum = "P?";
            for (int i = 0; i < (int)players.size(); i++)
                if (players[i] == pt->getTileOwner()) { pNum = "P" + to_string(i+1); break; }
            int lvl = pt->getLevel();
            string lvlStr = (lvl >= 4) ? "*" : string(lvl, '^');
            line2 = pNum + (lvlStr.empty() ? "" : " " + lvlStr);
        }
        string tokens = "";
        for (int i = 0; i < (int)players.size(); i++)
            if (players[i]->getCurrTile() == idx) tokens += "(" + to_string(i+1) + ")";
        if (!tokens.empty()) line2 += (line2.empty() ? "" : " ") + tokens;

        return renderTile(line1, line2, colorGroup, w, h);
    };

    // ── TOP ROW ──────────────────────────────────────────────────────────────
    int TL = sideH + sideV + 2;
    int TR = 2*sideH + sideV + 3;

    vector<vector<string>> topRow;
    topRow.push_back(mkTile(TL, TW, TH));
    for (int i = TL+1; i < TR; i++) topRow.push_back(mkTile(i, TW, TH));
    topRow.push_back(mkTile(TR, TW, TH));

    for (int l = 0; l < TH; l++) {
        for (auto& t : topRow) cout << t[l];
        cout << "\n";
    }

    // ── MIDDLE ROWS ───────────────────────────────────────────────────────────
    int cl = 0;
    for (int row = 0; row < sideV; row++) {
        int leftIdx  = sideH + sideV + 1 - row;
        int rightIdx = 2*sideH + sideV + 4 + row;
        vector<string> L = mkTile(leftIdx,  TW, TH);
        vector<string> R = mkTile(rightIdx, TW, TH);

        for (int l = 0; l < TH; l++) {
            cout << L[l];
            bool firstLine = (row == 0 && l == 0);
            bool lastLine  = (row == sideV-1 && l == TH-1);
            if (firstLine || lastLine)
                cout << "+" + string(iw, '-') + "+";
            else
                cout << "|" + centerContent[cl++] + "|";
            cout << R[l] << "\n";
        }
    }

    // ── BOTTOM ROW ────────────────────────────────────────────────────────────
    int BL = sideH + 1;

    vector<vector<string>> botRow;
    botRow.push_back(mkTile(BL, TW, TH));
    for (int i = sideH; i >= 1; i--) botRow.push_back(mkTile(i, TW, TH));
    botRow.push_back(mkTile(0, TW, TH));

    for (int l = 0; l < TH; l++) {
        for (auto& t : botRow) cout << t[l];
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
            string label = tile->getTileName() + " (" + tile->getTileCode() + ")";
            cout << "  - " << leftOut(label, 30) << leftOut("MXXX", 8) << status << "\n";
            total += tile->getBuyPrice(); //TODO: adjust to actual method name
        }
        cout << "\n";
    }

    cout << "Total kekayaan properti: M" << total << "\n";
}

void OutputFormatter::printLog(vector<LogEntry> &log, int n){
    int total = (int)log.size();

    // determine range
    int start = 0;
    if(n > 0 && n < total){
        start = total - n;
        cout << "=== Log Transaksi (" << n << " Terakhir) ===\n\n";
    } else {
        cout << "=== Log Transaksi Penuh ===\n\n";
    }

    for(int i = start; i < total; i++){
        cout << log[i].getLogEntry() << "\n";
    }
}

void OutputFormatter::printWin(vector<Player> &ps, bool isBankruptcy){
    const string SEP = "+================================+";

    // ── HEADER ───────────────────────────────────────────────────────────────
    cout << YELLOW << SEP << "\n";
    if(isBankruptcy){
        cout << "|" << centerOut("PERMAINAN SELESAI!", 32) << "|\n";
        cout << "|" << centerOut("Semua pemain kecuali satu bangkrut", 32) << "|\n";
    } else {
        cout << "|" << centerOut("PERMAINAN SELESAI!", 32) << "|\n";
        cout << "|" << centerOut("Batas giliran tercapai", 32) << "|\n";
    }
    cout << SEP << RESET << "\n\n";

    // ── PLAYER RECAP (max turn only) ─────────────────────────────────────────
    if(!isBankruptcy){
        cout << "Rekap pemain:\n\n";
        for(auto& p : ps){
            cout << YELLOW << p.getUsername() << RESET << "\n"; //TODO: adjust method name
            cout << leftOut("Uang", 10)     << ": M" << p.getBalance()              << "\n"; //TODO: adjust
            cout << leftOut("Properti", 10) << ": "  << p.getOwnedProperties().size() << "\n";
            cout << "\n";
        }
    } else {
        cout << "Pemain tersisa:\n";
        for(auto& p : ps)
            cout << "  - " << p.getUsername() << "\n"; //TODO: adjust method name
        cout << "\n";
    }

    // ── DETERMINE WINNER(S) ──────────────────────────────────────────────────
    // winner = player(s) with highest net worth (balance + property values)
    // TODO: replace getBalance() + calculateNetWorth() with actual method names
    int maxWorth = 0;
    for(auto& p : ps){
        int worth = p.getBalance(); //TODO: + p.calculateNetWorth() when available
        if(worth > maxWorth) maxWorth = worth;
    }

    vector<string> winners;
    for(auto& p : ps){
        int worth = p.getBalance(); //TODO: same as above
        if(worth == maxWorth) winners.push_back(p.getUsername()); //TODO: adjust method name
    }

    // ── WINNER DISPLAY ────────────────────────────────────────────────────────
    cout << YELLOW << SEP << "\n";
    if(winners.size() == 1){
        cout << "|" << centerOut("Pemenang: " + winners[0], 32) << "|\n";
    } else {
        cout << "|" << centerOut("SERI!", 32) << "|\n";
        cout << SEP << "\n";
        for(auto& w : winners)
            cout << "|" << centerOut(w, 32) << "|\n";
    }
    cout << SEP << RESET << "\n";
}

