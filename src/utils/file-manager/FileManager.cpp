#include "include/utils/file-manager/FileManager.hpp"
#include "include/core/GameConfig.hpp"

#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "include/models/tiles/ChanceTile.hpp"
#include "include/models/tiles/CommunityChestTile.hpp"
#include "include/models/tiles/FestivalTile.hpp"
#include "include/models/tiles/FreeParkingTile.hpp"
#include "include/models/tiles/GoJailTile.hpp"
#include "include/models/tiles/JailTile.hpp"
#include "include/models/tiles/PropertyTile.hpp"
#include "include/models/tiles/StartTile.hpp"
#include "include/models/tiles/TaxTile.hpp"

namespace
{
    std::string trim(const std::string &text)
    {
        const std::string whitespace = " \t\r\n";
        const std::size_t start = text.find_first_not_of(whitespace);
        if (start == std::string::npos)
            return "";
        const std::size_t end = text.find_last_not_of(whitespace);
        return text.substr(start, end - start + 1);
    }

    std::vector<std::string> splitByWhiteSpace(const std::string &line)
    {
        std::vector<std::string> tokens;
        std::istringstream iss(line);
        std::string token;
        while (iss >> token)
            tokens.push_back(token);
        return tokens;
    }

    std::string readNextNonemptyLine(std::ifstream &input, int &line_number)
    {
        std::string line;
        while (std::getline(input, line))
        {
            ++line_number;
            if (!trim(line).empty())
                return line;
        }
        throw SaveLoadException("Format save invalid: data berhenti sebelum selesai dibaca.");
    }

    int parseIntegerToken(const std::string &token, const std::string &field_name, int line_number)
    {
        try
        {
            std::size_t processed = 0;
            const int value = std::stoi(token, &processed);
            if (processed != token.size())
                throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": field " + field_name + " bukan bilangan bulat yang valid.");
            return value;
        }
        catch (const std::invalid_argument &)
        {
            throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": field " + field_name + " bukan bilangan bulat.");
        }
        catch (const std::out_of_range &)
        {
            throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": field " + field_name + " di luar batas integer.");
        }
    }

    int parseNonnegative(const std::string &token, const std::string &field_name, int line_number)
    {
        const int value = parseIntegerToken(token, field_name, line_number);
        if (value < 0) throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": field " + field_name + " tidak boleh negatif.");
        return value;
    }

    void validateSave(const GameStates::SavePermission &permission)
    {
        if (!permission.is_at_start_of_turn || permission.has_rolled_dice || permission.has_executed_action)
        {
            throw SaveLoadException(
                "Save hanya boleh dilakukan di awal giliran saat pemain belum melempar dadu atau melakukan aksi apa pun.");
        }
    }

    void validateLoad(const GameStates::LoadPermission &permission)
    {
        if (!permission.is_before_game_start) throw SaveLoadException("Load hanya boleh dilakukan saat program baru berjalan dan permainan belum dimulai.");
    }

    void writeCardState(std::ofstream &output, const GameStates::CardState &card)
    {
        output << card.type;

        std::string card_value = card.value;
        if (card_value.empty() && !card.remaining_duration.empty()) card_value = "-";
        if (!card_value.empty()) output << " " << card_value;
        if (!card.remaining_duration.empty())
            output << " " << card.remaining_duration;
        output << "\n";
    }
}

void FileManager::saveConfig(
    const std::string &file_path,
    const GameStates::SaveState &state,
    const GameStates::SavePermission &permission)
{
    validateSave(permission);

    std::ofstream output(file_path);
    if (!output.is_open())
        throw SaveLoadException("Gagal membuka file save untuk ditulis: " + file_path);

    output << state.current_turn << " " << state.max_turn << "\n";
    output << state.players.size() << "\n";

    for (const GameStates::PlayerState &player : state.players)
    {
        output << player.username << " " << player.money << " " << player.tile_code << " " << player.status << "\n";
        output << player.hand_cards.size() << "\n";
        for (const GameStates::CardState &card : player.hand_cards)
            writeCardState(output, card);
    }
    for (std::size_t i = 0; i < state.turn_order.size(); ++i)
    {
        if (i > 0) output << " ";
        output << state.turn_order[i];
    }
    output << "\n";

    output << state.active_turn_player << "\n";

    output << state.properties.size() << "\n";
    for (const GameStates::PropertyState &property : state.properties){
        output  << property.tile_code << " "
                << property.type << " "
                << property.owner_username << " "
                << property.status << " "
                << property.festival_multiplier << " "
                << property.festival_duration << " "
                << property.building_count << "\n";
    }

    output << state.deck.ability_deck.size() << "\n";
    for (const std::string &card_type : state.deck.ability_deck){
        output << card_type << "\n";
    }

    output << state.logs.size() << "\n";
    for (const GameStates::LogState &log : state.logs){
        output << log.turn << " " << log.username << " " << log.action_type;
        if (!log.detail.empty()) output << " " << log.detail;
        output << "\n";
    }

    // Optional appendix (extension): bot flags + jail_turns. Spec parsers
    // that stop after the log section will simply ignore this block.
    int botCount = 0;
    for (const auto &p : state.players) if (p.is_bot || p.jail_turns > 0) botCount++;
    if (botCount > 0) {
        output << "EXT_META\n";
        output << botCount << "\n";
        for (const auto &p : state.players) {
            if (p.is_bot || p.jail_turns > 0) {
                output << p.username << " " << (p.is_bot ? 1 : 0) << " " << p.jail_turns << "\n";
            }
        }
    }
}

GameStates::SaveState FileManager::loadConfig(
    const std::string &file_path,
    const GameStates::LoadPermission &permission)
{
    validateLoad(permission);

    std::ifstream input(file_path);
    if (!input.is_open())
        throw SaveLoadException("Gagal membuka file save untuk dibaca: " + file_path);

    GameStates::SaveState state;
    int line_number = 0;

    std::string line = readNextNonemptyLine(input, line_number);
    std::vector<std::string> tokens = splitByWhiteSpace(line);
    if (tokens.size() != 2) throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": expected '<TURN_SAAT_INI> <MAX_TURN>'.");
    state.current_turn = parseNonnegative(tokens[0], "TURN_SAAT_INI", line_number);
    state.max_turn = parseNonnegative(tokens[1], "MAX_TURN", line_number);

    line = readNextNonemptyLine(input, line_number);
    tokens = splitByWhiteSpace(line);
    if (tokens.size() != 1) throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": expected '<JUMLAH_PEMAIN>'.");
    const int player_count = parseNonnegative(tokens[0], "JUMLAH_PEMAIN", line_number);

    state.players.clear();
    state.players.reserve(static_cast<std::size_t>(player_count));

    for (int i = 0; i < player_count; ++i)
    {
        line = readNextNonemptyLine(input, line_number);
        tokens = splitByWhiteSpace(line);
        // Spec strict format: 4 tokens. Older saves (5-6 tokens) still accepted.
        if (tokens.size() < 4 || tokens.size() > 6) throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": expected '<USERNAME> <UANG> <POSISI_PETAK> <STATUS>'.");

        GameStates::PlayerState player;
        player.username = tokens[0];
        player.money = parseIntegerToken(tokens[1], "UANG", line_number);
        player.tile_code = tokens[2];
        player.status = tokens[3];
        player.is_bot = (tokens.size() >= 5 && tokens[4] == "1");
        player.jail_turns = (tokens.size() >= 6) ? parseIntegerToken(tokens[5], "JAIL_TURNS", line_number) : 0;

        line = readNextNonemptyLine(input, line_number);
        tokens = splitByWhiteSpace(line);
        if (tokens.size() != 1) throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": expected '<JUMLAH_KARTU_TANGAN>'.");

        const int card_count = parseNonnegative(tokens[0], "JUMLAH_KARTU_TANGAN", line_number);
        player.hand_cards.reserve(static_cast<std::size_t>(card_count));

        for (int j = 0; j < card_count; ++j)
        {
            line = readNextNonemptyLine(input, line_number);
            tokens = splitByWhiteSpace(line);
            if (tokens.empty() || tokens.size() > 3) throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": expected '<JENIS_KARTU> [NILAI_KARTU] [SISA_DURASI]'.");

            GameStates::CardState card;
            card.type = tokens[0];
            if (tokens.size() >= 2 && tokens[1] != "-") card.value = tokens[1];
            if (tokens.size() == 3) card.remaining_duration = tokens[2];
            player.hand_cards.push_back(card);
        }

        state.players.push_back(player);
    }

    line = readNextNonemptyLine(input, line_number);
    tokens = splitByWhiteSpace(line);
    if (player_count > 0 && static_cast<int>(tokens.size()) != player_count) throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": jumlah username pada urutan giliran tidak sesuai JUMLAH_PEMAIN.");
    state.turn_order = tokens;

    line = readNextNonemptyLine(input, line_number);
    tokens = splitByWhiteSpace(line);
    if (tokens.size() != 1)throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": expected '<GILIRAN_AKTIF_SAAT_INI>'.");
    state.active_turn_player = tokens[0];

    line = readNextNonemptyLine(input, line_number);
    tokens = splitByWhiteSpace(line);
    if (tokens.size() != 1) throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": expected '<JUMLAH_PROPERTI>'.");
    const int property_count = parseNonnegative(tokens[0], "JUMLAH_PROPERTI", line_number);

    state.properties.clear();
    state.properties.reserve(static_cast<std::size_t>(property_count));

    for (int i = 0; i < property_count; ++i){
        line = readNextNonemptyLine(input, line_number);
        tokens = splitByWhiteSpace(line);
        if (tokens.size() != 7) throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": expected '<KODE_PETAK> <JENIS> <PEMILIK> <STATUS> <FMULT> <FDUR> <N_BANGUNAN>'.");

        GameStates::PropertyState property;
        property.tile_code = tokens[0];
        property.type = tokens[1];
        property.owner_username = tokens[2];
        property.status = tokens[3];
        property.festival_multiplier = parseNonnegative(tokens[4], "FMULT", line_number);
        property.festival_duration = parseNonnegative(tokens[5], "FDUR", line_number);
        property.building_count = tokens[6];

        state.properties.push_back(property);
    }

    line = readNextNonemptyLine(input, line_number);
    tokens = splitByWhiteSpace(line);
    if (tokens.size() != 1) throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": expected '<JUMLAH_KARTU_DECK_KEMAMPUAN>'.");
    const int deck_count = parseNonnegative(tokens[0], "JUMLAH_KARTU_DECK_KEMAMPUAN", line_number);

    state.deck.ability_deck.clear();
    state.deck.ability_deck.reserve(static_cast<std::size_t>(deck_count));

    for (int i = 0; i < deck_count; ++i){
        line = readNextNonemptyLine(input, line_number);
        tokens = splitByWhiteSpace(line);
        if (tokens.size() != 1) throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": expected '<JENIS_KARTU>' pada state deck.");

        state.deck.ability_deck.push_back(tokens[0]);
    }

    line = readNextNonemptyLine(input, line_number);
    tokens = splitByWhiteSpace(line);
    if (tokens.size() != 1) throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": expected '<JUMLAH_ENTRI_LOG>'.");
    
    const int log_count = parseNonnegative(tokens[0], "JUMLAH_ENTRI_LOG", line_number);

    state.logs.clear();
    state.logs.reserve(static_cast<std::size_t>(log_count));

    for (int i = 0; i < log_count; ++i)
    {
        line = readNextNonemptyLine(input, line_number);
        std::istringstream iss(line);
        GameStates::LogState log;
        if (!(iss >> log.turn >> log.username >> log.action_type))throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": expected '<TURN> <USERNAME> <JENIS_AKSI> <DETAIL>'.");
        std::string detail;
        std::getline(iss, detail);
        log.detail = trim(detail);
        state.logs.push_back(log);
    }

    // Optional appendix (extension): bot flags + jail_turns. Absent in
    // strict-spec saves; present in saves produced by this program.
    std::string maybe_marker;
    while (std::getline(input, maybe_marker)) {
        ++line_number;
        std::string trimmed = trim(maybe_marker);
        if (trimmed.empty()) continue;
        if (trimmed != "EXT_META") break;

        std::string countLine;
        if (!std::getline(input, countLine)) break;
        ++line_number;
        std::vector<std::string> ctok = splitByWhiteSpace(countLine);
        if (ctok.size() != 1) break;
        int extCount = parseNonnegative(ctok[0], "EXT_COUNT", line_number);

        for (int i = 0; i < extCount; ++i) {
            std::string entry;
            if (!std::getline(input, entry)) break;
            ++line_number;
            std::vector<std::string> et = splitByWhiteSpace(entry);
            if (et.size() < 3) continue;
            const std::string &uname = et[0];
            bool is_bot = (et[1] == "1");
            int jail_turns = parseIntegerToken(et[2], "JAIL_TURNS", line_number);
            for (auto &p : state.players) {
                if (p.username == uname) {
                    p.is_bot = is_bot;
                    p.jail_turns = jail_turns;
                    break;
                }
            }
        }
        break;
    }

    return state;
}

// ── loadBoard ─────────────────────────────────────────────────────────────────
std::vector<Tile*> FileManager::loadBoard(const std::string &configDir)
{
    // --- 1. Parse property.txt → keyed by tile code ---
    class PropData {
    public:
        std::string name, type, color;
        int price = 0, mortgage = 0, houseCost = 0, hotelCost = 0;
        std::vector<int> rent;
    };

    std::map<std::string, PropData> props;
    {
        std::ifstream pf(configDir + "property.txt");
        std::string line;
        while (std::getline(pf, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            int id;
            std::string code, name, type, color;
            if (!(iss >> id >> code >> name >> type >> color)) continue;
            PropData d;
            d.name = name; d.type = type; d.color = color;
            if (!(iss >> d.price >> d.mortgage)) continue;
            if (type == "STREET") {
                if (!(iss >> d.houseCost >> d.hotelCost)) continue;
                int r;
                while (iss >> r) d.rent.push_back(r);
            }
            props[code] = d;
        }
    }

    auto makeProp = [&](const std::string &code, const std::string &nameHint) -> Tile* {
        auto it = props.find(code);
        if (it == props.end())
            return new Tile(code, code, nameHint, "UNKNOWN");
        const auto &d = it->second;
        std::vector<int> rent = d.rent.empty() ? std::vector<int>{0} : d.rent;
        return new PropertyTile(
            code, code, d.name, d.type,
            rent, d.mortgage, 0,
            false, d.color,
            d.price, d.price,
            d.houseCost, d.hotelCost,
            1, 0
        );
    };

    // --- 2a. Try aksi.txt + property.txt first (Revisi 2 spec). ---
    // aksi.txt columns: ID KODE NAMA JENIS_PETAK WARNA
    // We map JENIS_PETAK → internal type strings.
    class AksiData { public: std::string code, name, jenis; };
    std::map<int, AksiData> aksiByPos;
    {
        std::ifstream af(configDir + "aksi.txt");
        if (af.is_open()) {
            std::string line;
            while (std::getline(af, line)) {
                if (line.empty() || line[0] == '#') continue;
                std::istringstream iss(line);
                int pos; std::string code, name, jenis, warna;
                if (!(iss >> pos >> code >> name >> jenis >> warna)) continue;
                aksiByPos[pos] = {code, name, jenis};
            }
        }
    }

    // Helper: convert JENIS_PETAK + KODE to internal type
    auto aksiInternalType = [](const std::string &jenis, const std::string &kode) -> std::string {
        if (jenis == "SPESIAL") {
            if (kode == "GO")  return "START";
            if (kode == "PEN") return "JAIL";
            if (kode == "BBP") return "FREE_PARKING";
            if (kode == "PPJ") return "GO_JAIL";
        } else if (jenis == "KARTU") {
            if (kode == "DNU") return "COMMUNITY_CHEST";
            if (kode == "KSP") return "CHANCE";
        } else if (jenis == "PAJAK") {
            if (kode == "PPH") return "TAX_PPH";
            if (kode == "PBM") return "TAX_PBM";
        } else if (jenis == "FESTIVAL") {
            return "FESTIVAL";
        }
        return "UNKNOWN";
    };

    std::vector<Tile*> tiles;

    // If aksi.txt was loaded successfully (has entries), build from aksi+property
    if (!aksiByPos.empty()) {
        // Determine total positions from aksi.txt + property.txt
        int maxPos = 0;
        for (const auto &[pos, _] : aksiByPos) if (pos > maxPos) maxPos = pos;
        for (const auto &[code, d] : props) (void)d, (void)code; // props uses code as key; we need positions from property.txt itself

        // Re-parse property.txt to get positions
        class PropPos { public: int pos; std::string code; };
        std::vector<PropPos> propPositions;
        {
            std::ifstream pf(configDir + "property.txt");
            std::string line;
            while (std::getline(pf, line)) {
                if (line.empty() || line[0] == '#') continue;
                std::istringstream iss(line);
                int id; std::string code;
                if (!(iss >> id >> code)) continue;
                propPositions.push_back({id, code});
                if (id > maxPos) maxPos = id;
            }
        }

        // Build tile array indexed 1..maxPos
        std::map<int, Tile*> byPos;
        for (const auto &[pos, ad] : aksiByPos) {
            std::string internalType = aksiInternalType(ad.jenis, ad.code);
            std::string id = ad.code + std::to_string(pos);
            Tile *tile = nullptr;
            if      (internalType == "START")           tile = new StartTile(ad.code, id, ad.name, "START");
            else if (internalType == "JAIL")            tile = new JailTile(ad.code, id, ad.name, "JAIL");
            else if (internalType == "GO_JAIL")         tile = new GoJailTile(ad.code, id, ad.name, "GO_JAIL");
            else if (internalType == "FREE_PARKING")    tile = new FreeParkingTile(ad.code, id, ad.name, "FREE_PARKING");
            else if (internalType == "FESTIVAL")        tile = new FestivalTile(ad.code, id, ad.name, "FESTIVAL");
            else if (internalType == "CHANCE")          tile = new ChanceTile(ad.code, id, ad.name, "CHANCE");
            else if (internalType == "COMMUNITY_CHEST") tile = new CommunityChestTile(ad.code, id, ad.name, "COMMUNITY_CHEST");
            else if (internalType == "TAX_PPH")         tile = new TaxTile(ad.code, id, ad.name, "TAX_PPH");
            else if (internalType == "TAX_PBM")         tile = new TaxTile(ad.code, id, ad.name, "TAX_PBM");
            else                                        tile = new Tile(ad.code, id, ad.name, internalType);
            byPos[pos] = tile;
        }
        for (const auto &pp : propPositions) {
            byPos[pp.pos] = makeProp(pp.code, pp.code);
        }

        for (int i = 1; i <= maxPos; i++) {
            auto it = byPos.find(i);
            if (it != byPos.end()) tiles.push_back(it->second);
        }
        if (!tiles.empty()) {
            const int n = static_cast<int>(tiles.size());
            if (n < 20 || n > 60) {
                throw SaveLoadException("Konfigurasi papan invalid: jumlah petak harus 20-60 (ditemukan " + std::to_string(n) + ").");
            }
            int goCount = 0, jailCount = 0;
            for (auto *t : tiles) {
                if (!t) continue;
                const std::string &type = t->getTileType();
                if (type == "START") goCount++;
                else if (type == "JAIL") jailCount++;
            }
            if (goCount != 1) {
                throw SaveLoadException("Konfigurasi papan invalid: harus tepat 1 Petak Mulai (GO), ditemukan " + std::to_string(goCount) + ".");
            }
            if (jailCount != 1) {
                throw SaveLoadException("Konfigurasi papan invalid: harus tepat 1 Petak Penjara, ditemukan " + std::to_string(jailCount) + ".");
            }
            return tiles;
        }
    }

    // --- 2b. Fallback: board.txt (legacy) ---
    std::ifstream bf(configDir + "board.txt");
    if (!bf.is_open())
        throw SaveLoadException("Gagal membuka aksi.txt atau board.txt di: " + configDir);

    std::string line;
    int expectedPos = 1;
    while (std::getline(bf, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        int pos;
        std::string code, name, type;
        if (!(iss >> pos >> code >> name >> type)) continue;

        if (pos != expectedPos)
            throw SaveLoadException("board.txt: urutan petak tidak berurutan (expected " +
                                    std::to_string(expectedPos) + ", got " +
                                    std::to_string(pos) + ")");

        std::string id = code + std::to_string(pos);

        Tile* tile = nullptr;
        if      (type == "START")           tile = new StartTile(code, id, name, "START");
        else if (type == "JAIL")            tile = new JailTile(code, id, name, "JAIL");
        else if (type == "GO_JAIL")         tile = new GoJailTile(code, id, name, "GO_JAIL");
        else if (type == "FREE_PARKING")    tile = new FreeParkingTile(code, id, name, "FREE_PARKING");
        else if (type == "FESTIVAL")        tile = new FestivalTile(code, id, name, "FESTIVAL");
        else if (type == "CHANCE")          tile = new ChanceTile(code, id, name, "CHANCE");
        else if (type == "COMMUNITY_CHEST") tile = new CommunityChestTile(code, id, name, "COMMUNITY_CHEST");
        else if (type == "TAX_PPH")         tile = new TaxTile(code, id, name, "TAX_PPH");
        else if (type == "TAX_PBM")         tile = new TaxTile(code, id, name, "TAX_PBM");
        else if (type == "STREET" || type == "RAILROAD" || type == "UTILITY")
                                            tile = makeProp(code, name);
        else                                tile = new Tile(code, id, name, type);

        tiles.push_back(tile);
        ++expectedPos;
    }

    return tiles;
}

// ── loadMiscConfig ────────────────────────────────────────────────────────────
void FileManager::loadMiscConfig(const std::string &configDir,
                                 int &outMaxTurn, int &outInitBalance)
{
    outMaxTurn     = 15;
    outInitBalance = 1000;

    std::ifstream file(configDir + "misc.txt");
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        iss >> outMaxTurn >> outInitBalance;
        return;
    }
}

// ── loadGameConfig ────────────────────────────────────────────────────────────
GameConfig FileManager::loadGameConfig(const std::string &configDir)
{
    int go_salary = 200, jail_fine = 50;
    int pph_flat = 150, pph_percentage = 10, pbm_flat = 200;
    int max_turn = 15, initial_balance = 1000;
    std::map<int, int> railroad_rent;
    std::map<int, int> utility_multiplier;

    auto readFile = [&](const std::string &name) -> std::ifstream {
        return std::ifstream(configDir + name);
    };

    // special.txt: GO_SALARY  JAIL_FINE
    {
        auto f = readFile("special.txt");
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            iss >> go_salary >> jail_fine;
            break;
        }
    }
    // railroad.txt: COUNT  RENT
    {
        auto f = readFile("railroad.txt");
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            int cnt, rent;
            if (iss >> cnt >> rent) railroad_rent[cnt] = rent;
        }
    }
    // utility.txt: COUNT  MULTIPLIER
    {
        auto f = readFile("utility.txt");
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            int cnt, mult;
            if (iss >> cnt >> mult) utility_multiplier[cnt] = mult;
        }
    }
    // tax.txt: PPH_FLAT  PPH_PERCENTAGE  PBM_FLAT
    {
        auto f = readFile("tax.txt");
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            iss >> pph_flat >> pph_percentage >> pbm_flat;
            break;
        }
    }
    // misc.txt: MAX_TURN  SALDO_AWAL
    {
        auto f = readFile("misc.txt");
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            iss >> max_turn >> initial_balance;
            break;
        }
    }

    return GameConfig(go_salary, jail_fine, pph_flat, pph_percentage, pbm_flat,
                      max_turn, initial_balance, railroad_rent, utility_multiplier);
}
