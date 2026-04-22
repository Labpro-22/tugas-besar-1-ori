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
    return color + str + reset;
}

void OutputFormatter::initializeColors(Board &b) {
    if (color_group_ansi.size() > 0) return;

    for (int i = 0; i < b.getTileCount(); i++) {
        Tile *t = b.getTileByIndex(i);
        PropertyTile *pt = dynamic_cast<PropertyTile*>(t);
        if (pt) {
            string group = pt->getColorGroup();
            if (!group.empty() && color_group_ansi.find(group) == color_group_ansi.end()) {
                color_group_ansi[group] = color_palette[next_color_idx % color_palette.size()];
                color_group_codes[group] = generateCode(group);
                next_color_idx++;
            }
        }
    }
}

string OutputFormatter::generateCode(string color_group) {
    if (color_group.length() < 2) return color_group;
    string code = "";
    bool first = true;
    for (char c : color_group) {
        if (isalpha(c)) {
            char upper_c = toupper(c);
            if (first) {
                code += upper_c;
                first = false;
            } else {
                if (upper_c != 'A' && upper_c != 'E' && upper_c != 'I' && upper_c != 'O' && upper_c != 'U') {
                    code += upper_c;
                    break;
                }
            }
        }
    }
    if (code.length() < 2 && color_group.length() >= 2) {
        code = "";
        code += toupper(color_group[0]);
        code += toupper(color_group[1]);
    }
    // Check for collisions
    string final_code = code;
    int suffix = 1;
    bool clash = true;
    while (clash) {
        clash = false;
        for (auto const& [key, val] : color_group_codes) {
            if (val == final_code) {
                clash = true;
                final_code = code.substr(0, 1) + to_string(suffix);
                suffix++;
                break;
            }
        }
    }
    return final_code;
}

string OutputFormatter::getGroupColor(string color_group){
    if (color_group == "ABU_ABU") return "\033[1;90m";
    if (color_group_ansi.find(color_group) != color_group_ansi.end()) {
        return color_group_ansi[color_group];
    }
    return "\033[0m";
}
string OutputFormatter::getColorCode(string color_group, string tile_type){
    if (tile_type == "UTILITY" || color_group == "ABU_ABU") return "AB";
    if (color_group_codes.find(color_group) != color_group_codes.end()) {
        return color_group_codes[color_group];
    }
    return "DF";
}

vector<string> OutputFormatter::renderTile(string line_1, string line_2, string color_group, string tile_type, int w, int h) {
    string color  = getGroupColor(color_group);
    if(color_group == "" && tile_type == "UTILITY") color = "\033[1;90m"; // fallback for pure utility
    string border = color + "+" + string(w, '-') + "+" + reset;
    string empty  = color + "|" + string(w, ' ') + "|" + reset;

    if((int)line_1.size() > w) line_1 = line_1.substr(0, w);
    if((int)line_2.size() > w) line_2 = line_2.substr(0, w);

    vector<string> lines;
    lines.push_back(border);
    lines.push_back(color + "|" + leftOut(line_1, w) + "|" + reset);
    lines.push_back(color + "|" + leftOut(line_2, w) + "|" + reset);
    while((int)lines.size() < h - 1) lines.push_back(empty);
    lines.push_back(border);
    return lines;
}

//------------------------------Public Methods-----------------------------------

void OutputFormatter::printBoard(Board &b, vector<Player*> players, int turn, int max_turn) {
    initializeColors(b);

    const int tw = 10, th = 4;

    int total = b.getTileCount();
    int side_h, side_v;
    if ((total - 4) % 4 == 0) {
        side_h = side_v = (total - 4) / 4;
    } else {
        side_v = (total - 6) / 4;
        side_h = side_v + 1;
    }

    int center_w = side_h * (tw + 2);
    int iw = center_w - 2;

    // ── BUILD CENTER LEGEND ───────────────────────────────────────────────────
    const int box_width = min(34, iw - 4);
    string eq_line  = string(box_width, '=');
    string da_line  = string(box_width, '-');

    auto padC = [&](string s) -> string {
        if ((int)s.size() >= iw) return s.substr(0, iw);
        int lp = (iw - (int)s.size()) / 2;
        return string(lp, ' ') + s + string(iw - (int)s.size() - lp, ' ');
    };
    int box_offset = (iw - box_width) / 2;
    auto padL = [&](string s) -> string {
        string r = string(box_offset, ' ') + s;
        if ((int)r.size() < iw) r += string(iw - (int)r.size(), ' ');
        return r.substr(0, iw);
    };

    string title_box = "||" + centerOut("NIMONSPOLI", box_width - 4) + "||";
    string turn_str  = (turn > 0 && max_turn > 0)
                      ? "TURN " + to_string(turn) + " / " + to_string(max_turn) : "";

    vector<string> center_content = {
        string(iw, ' '),
        padC(eq_line),
        padC(title_box),
        padC(eq_line),
        string(iw, ' '),
        padC(turn_str),
        string(iw, ' '),
        padC(da_line),
        padL("LEGENDA KEPEMILIKAN & STATUS"),
        padL("P1-P4 : Properti milik Pemain 1-4"),
        padL("^     : Rumah Level 1"),
        padL("^^    : Rumah Level 2"),
        padL("^^^   : Rumah Level 3"),
        padL("*     : Hotel (Maksimal)"),
        padL("(1)-(4): Bidak (IN=Tahanan, V=Mampir)"),
        padC(da_line),
        padL("KODE WARNA:")
    };

    // Dynamically build the color codes per their generation
    string current_line = "";
    for (auto& [grp, code] : color_group_codes) {
        string entry = "[" + code + "]=" + grp;
        if (current_line.empty()) {
            current_line = leftOut(entry, 17);
        } else {
            current_line += entry;
            center_content.push_back(padL(current_line));
            current_line = "";
        }
    }
    if (!current_line.empty()) {
        center_content.push_back(padL(current_line));
    }
    center_content.push_back(padL(leftOut("[DF]=Aksi", 17) + "[AB]=Utilitas"));
    center_content.push_back(padC(da_line));

    int num_inner = side_v * th - 2;
    while ((int)center_content.size() < num_inner) center_content.push_back(string(iw, ' '));

    // ── TILE BUILDER ──────────────────────────────────────────────────────────
    auto mkTile = [&](int idx, int w, int h) {
        Tile* t = b.getTileByIndex(idx);
        PropertyTile* pt = dynamic_cast<PropertyTile*>(t);
        string color_group = pt ? pt->getColorGroup() : "";
        string color_code  = getColorCode(color_group, t->getTileType());
        string line_1 = "[" + color_code + "] " + t->getTileCode();

        string line_2 = "";
        if (pt && pt->getTileOwner() != nullptr) {
            string p_num = "P?";
            for (int i = 0; i < (int)players.size(); i++)
                if (players[i] == pt->getTileOwner()) { p_num = "P" + to_string(i+1); break; }
            int lvl = pt->getLevel();
            string lvl_str = (lvl >= 4) ? "*" : string(lvl, '^');
            line_2 = p_num + (lvl_str.empty() ? "" : " " + lvl_str);
        }
        string tokens = "";
        for (int i = 0; i < (int)players.size(); i++)
            if (players[i]->getCurrTile() == idx) tokens += "(" + to_string(i+1) + ")";
        if (!tokens.empty()) line_2 += (line_2.empty() ? "" : " ") + tokens;

        return renderTile(line_1, line_2, color_group, t->getTileType(), w, h);
    };

    // ── TOP ROW ──────────────────────────────────────────────────────────────
    int tl_idx = side_h + side_v + 2;
    int tr_idx = 2*side_h + side_v + 3;

    vector<vector<string>> top_row;
    top_row.push_back(mkTile(tl_idx, tw, th));
    for (int i = tl_idx+1; i < tr_idx; i++) top_row.push_back(mkTile(i, tw, th));
    top_row.push_back(mkTile(tr_idx, tw, th));

    for (int l = 0; l < th; l++) {
        for (auto& t : top_row) cout << t[l];
        cout << "\n";
    }

    // ── MIDDLE ROWS ───────────────────────────────────────────────────────────
    int cl = 0;
    for (int row = 0; row < side_v; row++) {
        int left_idx  = side_h + side_v + 1 - row;
        int right_idx = 2*side_h + side_v + 4 + row;
        vector<string> L = mkTile(left_idx,  tw, th);
        vector<string> R = mkTile(right_idx, tw, th);

        for (int l = 0; l < th; l++) {
            cout << L[l];
            bool first_line = (row == 0 && l == 0);
            bool last_line  = (row == side_v-1 && l == th-1);
            if (first_line || last_line)
                cout << "+" + string(iw, '-') + "+";
            else
                cout << "|" + center_content[cl++] + "|";
            cout << R[l] << "\n";
        }
    }

    // ── BOTTOM ROW ────────────────────────────────────────────────────────────
    int bl_idx = side_h + 1;

    vector<vector<string>> bot_row;
    bot_row.push_back(mkTile(bl_idx, tw, th));
    for (int i = side_h; i >= 1; i--) bot_row.push_back(mkTile(i, tw, th));
    bot_row.push_back(mkTile(0, tw, th));

    for (int l = 0; l < th; l++) {
        for (auto& t : bot_row) cout << t[l];
        cout << "\n";
    }
}

void OutputFormatter::printAkta(PropertyTile &t, Board &b){
    initializeColors(b);
    string color = getGroupColor(t.getColorGroup());
    auto mval = [](int v) { return "M" + to_string(v); };
    auto row = [&](string label, string val) {
        cout << color << "|" << reset
             << leftOut(" " + label, 22) << " :"
             << color << leftOut(" " + val, 8) << "|" << reset << "\n";
    };

    string header = "[" + t.getColorGroup() + "] " + t.getTileName() + " (" + t.getTileCode() + ")";
    if ((int)header.size() > 32) header = header.substr(0, 32);

    cout << color << "+================================+" << reset << "\n";
    cout << color << "|" << reset << centerOut("AKTA KEPEMILIKAN", 32) << color << "|" << reset << "\n";
    cout << color << "|" << reset << centerOut(header, 32)             << color << "|" << reset << "\n";
    cout << color << "+================================+" << reset << "\n";
    row("Harga Beli",  mval(t.getBuyPrice()));
    row("Nilai Gadai", mval(t.getMortgageValue()));
    cout << color << "+--------------------------------+" << reset << "\n";
    vector<string> labels = {"Sewa (unimproved)","Sewa (1 rumah)","Sewa (2 rumah)",
                              "Sewa (3 rumah)","Sewa (4 rumah)","Sewa (Hotel)"};
    vector<int> rents = t.getRentPrice();
    for (int i = 0; i < (int)labels.size() && i < (int)rents.size(); i++)
        row(labels[i], mval(rents[i]));
    cout << color << "+--------------------------------+" << reset << "\n";
    row("Harga Rumah", mval(t.getHouseCost()));
    row("Harga Hotel", mval(t.getHotelCost()));
    cout << color << "+================================+" << reset << "\n";
    string owner_str  = t.getTileOwner() ? t.getTileOwner()->getUsername() : "BANK";
    string status_str = t.isMortgage() ? "DIGADAI" : "AKTIF";
    cout << color << "|" << reset
         << leftOut(" Status: " + status_str + " (" + owner_str + ")", 32)
         << color << "|" << reset << "\n";
    cout << color << "+================================+" << reset << "\n";
}

void OutputFormatter::printProperty(Player &p, Board &b){
    initializeColors(b);
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
        cout << color << "[" << group << "]" << reset << "\n";
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

void OutputFormatter::printWin(vector<Player> &ps, bool is_bankruptcy){
    const string sep = "+================================+";

    // ── HEADER ───────────────────────────────────────────────────────────────
    cout << yellow << sep << "\n";
    if(is_bankruptcy){
        cout << "|" << centerOut("PERMAINAN SELESAI!", 32) << "|\n";
        cout << "|" << centerOut("Semua pemain kecuali satu bangkrut", 32) << "|\n";
    } else {
        cout << "|" << centerOut("PERMAINAN SELESAI!", 32) << "|\n";
        cout << "|" << centerOut("Batas giliran tercapai", 32) << "|\n";
    }
    cout << sep << reset << "\n\n";

    // ── PLAYER RECAP (max turn only) ─────────────────────────────────────────
    if(!is_bankruptcy){
        cout << "Rekap pemain:\n\n";
        for(auto& p : ps){
            cout << yellow << p.getUsername() << reset << "\n";
            cout << leftOut("Uang",     10) << ": M" << p.getBalance()               << "\n";
            cout << leftOut("Properti", 10) << ": "  << p.getOwnedProperties().size() << "\n";
            cout << leftOut("Net Worth",10) << ": M" << p.getNetWorth()               << "\n";
            cout << "\n";
        }
    } else {
        cout << "Pemain tersisa:\n";
        for(auto& p : ps)
            cout << "  - " << p.getUsername() << "\n";
        cout << "\n";
    }

    // ── DETERMINE WINNER(S) ──────────────────────────────────────────────────
    int max_worth = 0;
    for(auto& p : ps){
        int worth = p.getNetWorth();
        if(worth > max_worth) max_worth = worth;
    }

    vector<string> winners;
    for(auto& p : ps){
        if(p.getNetWorth() == max_worth) winners.push_back(p.getUsername());
    }

    // ── WINNER DISPLAY ────────────────────────────────────────────────────────
    cout << yellow << sep << "\n";
    if(winners.size() == 1){
        cout << "|" << centerOut("Pemenang: " + winners[0], 32) << "|\n";
    } else {
        cout << "|" << centerOut("SERI!", 32) << "|\n";
        cout << sep << "\n";
        for(auto& w : winners)
            cout << "|" << centerOut(w, 32) << "|\n";
    }
    cout << sep << reset << "\n";
}

void OutputFormatter::printPlayerStatus(Player &p) {
    cout << yellow << "=== Status Pemain: " << p.getUsername() << " ===" << reset << "\n";
    cout << leftOut("Saldo",    12) << ": M" << p.getBalance() << "\n";
    cout << leftOut("Status",   12) << ": "  << p.getStatus()  << "\n";
    cout << leftOut("Properti", 12) << ": "  << p.getOwnedProperties().size() << " petak\n";
    cout << leftOut("Net Worth", 12) << ": M" << p.getNetWorth() << "\n";
    if (p.getHandSize() > 0) {
        cout << leftOut("Kartu Kemampuan", 12) << ": " << p.getHandSize() << " kartu\n";
    }
    cout << "\n";
}

void OutputFormatter::printAuction() {
    cout << yellow << "+================================+" << reset << "\n";
    cout << yellow << "|" << reset << centerOut("PROSES LELANG", 32) << yellow << "|" << reset << "\n";
    cout << yellow << "+================================+" << reset << "\n";
}
