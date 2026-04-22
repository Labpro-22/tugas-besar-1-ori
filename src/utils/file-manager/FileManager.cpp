#include "include/utils/file-manager/FileManager.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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
        if (tokens.size() != 4) throw SaveLoadException("Format save invalid di baris " + std::to_string(line_number) + ": expected '<USERNAME> <UANG> <POSISI_PETAK> <STATUS>'.");

        GameStates::PlayerState player;
        player.username = tokens[0];
        player.money = parseIntegerToken(tokens[1], "UANG", line_number);
        player.tile_code = tokens[2];
        player.status = tokens[3];

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

    return state;
}
