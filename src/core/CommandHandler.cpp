#include "include/core/CommandHandler.hpp"
#include "include/core/GameState.hpp"
#include "include/core/LandingProcessor.hpp"
#include "include/core/BankruptcyProcessor.hpp"
#include "include/core/CardProcessor.hpp"
#include "include/core/RentCollector.hpp"
#include "include/core/GameLoop.hpp"
#include "include/core/GameStates.hpp"
#include "include/core/PropertyManager.hpp"
#include "include/core/JailManager.hpp"
#include "include/core/BuyManager.hpp"
#include "include/core/RentManager.hpp"
#include "include/models/player/Player.hpp"
#include "include/models/player/Bot.hpp"
#include "include/models/tiles/PropertyTile.hpp"
#include "include/utils/file-manager/FileManager.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

using namespace std;

namespace {
    int readInt(const string &prompt, int lo, int hi) {
        int v;
        while (true) {
            cout << prompt;
            if (cin >> v && v >= lo && v <= hi) {
                return v;
            }
            cout << "  Input tidak valid.\n";
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }
}

CommandHandler::CommandHandler(
    GameState &s, 
    LandingProcessor &lp,
    BankruptcyProcessor &bp, 
    CardProcessor &cp,
    RentCollector &rc, 
    GameLoop &gl
) : state(s), landing(lp), bankruptcy(bp), cardProc(cp), rentCollector(rc), gameLoop(gl) {}

void CommandHandler::cmdBeli(Player &p) {
    Tile *t = state.tiles[p.getCurrTile()];
    auto *prop = dynamic_cast<PropertyTile*>(t);

    if (!prop || prop->getTileOwner()) { 
        cout << "Tidak bisa membeli properti ini.\n"; 
        return; 
    }

    if (prop->getTileType() != "STREET") {
        cout << "Railroad/Utility didapat otomatis saat mendarat.\n"; 
        return;
    }

    int price = prop->getBuyPrice();
    if (p.getDiscountActive() > 0.0f) {
        price = static_cast<int>(price * (1.0f - p.getDiscountActive()));
    }

    if (p.getBalance() < price) {
        cout << "Saldo tidak cukup. Properti masuk lelang.\n";
        bankruptcy.handleAutoAuction(p);
        return;
    }

    p += -price;
    p.addOwnedProperty(prop);
    state.recomputeMonopolyForGroup(prop->getColorGroup());
    if (p.getDiscountActive() > 0.0f) p.setDiscountActive(0.0f);

    cout << p.getUsername() << " membeli " << t->getTileName() << " seharga M" << price << ".\n";
    state.addLog(p, "BELI", t->getTileName() + " (" + t->getTileCode() + ") M" + to_string(price));
}

void CommandHandler::processJailHuman(
    Player &p, 
    bool manual, 
    int d1, 
    int d2,
    bool &has_rolled, 
    bool &turn_ended
) {
    if (state.jail_turns.find(&p) == state.jail_turns.end()) {
        state.jail_turns[&p] = 0;
    }

    if (state.jail_turns[&p] >= 3) {
        if (!bankruptcy.tryForceJailFine(p)) { 
            turn_ended = true; 
            return; 
        }

        bool isDouble = landing.rollAndMove(p, manual, d1, d2);
        string landName = state.tiles[p.getCurrTile()]->getTileName();
        string landCode = state.tiles[p.getCurrTile()]->getTileCode();

        string diceDetail = "Lempar: " + to_string(state.dice.getDie1()) + "+"
                          + to_string(state.dice.getDie2()) + "=" + to_string(state.dice.getTotal())
                          + (isDouble ? " (double)" : "") + " → " + landName + " (" + landCode + ")";
        
        state.addLog(p, "DADU", diceDetail);
        state.formatter.printBoard(state.board, state.players, state.current_turn, state.max_turn);
        landing.applyLanding(p);

        if (p.getStatus() == "BANKRUPT") { 
            turn_ended = true; 
            return; 
        }
        has_rolled = true;
    } else {
        int jailTile = p.getCurrTile();
        auto jr = JailManager::rollInJail(p, state.dice, state.jail_turns[&p], state.board.getTileCount(), manual, d1, d2);

        cout << p.getUsername() << " melempar dadu di penjara: "
             << state.dice.getDie1() << " + " << state.dice.getDie2()
             << " = " << state.dice.getTotal()
             << (jr.isDouble ? " (GANDA)" : "") << "\n";

        string diceDetail = "Lempar: " + to_string(state.dice.getDie1()) + "+"
                          + to_string(state.dice.getDie2()) + "=" + to_string(state.dice.getTotal())
                          + (jr.isDouble ? " (double)" : "") + " [PENJARA]";

        state.addLog(p, "DADU", diceDetail);

        if (jr.escaped) {
            landing.applyGoSalary(p, jailTile);
            p.setStatus("ACTIVE");
            state.jail_turns.erase(&p);

            cout << "Dadu ganda! Bebas dari penjara!\n";
            state.addLog(p, "PENJARA", "Bebas via double");
            state.formatter.printBoard(state.board, state.players, state.current_turn, state.max_turn);
            landing.applyLanding(p);

            if (p.getStatus() == "BANKRUPT") { 
                turn_ended = true; 
                return; 
            }
            has_rolled = true;
        } else {
            state.jail_turns[&p] = jr.newJailTurns;
            cout << p.getUsername() << " gagal keluar penjara (giliran ke-"
                 << state.jail_turns[&p] << "/3).\n";
            
            state.addLog(p, "PENJARA", "Gagal keluar giliran " + to_string(state.jail_turns[&p]) + "/3");
            has_rolled = true;
            turn_ended = true;
        }
    }
}

void CommandHandler::processNormalRollHuman(
    Player &p, 
    bool manual, 
    int d1, 
    int d2,
    bool &has_rolled, 
    bool &turn_ended,
    int &consec_doubles, 
    bool &has_bonus_turn
) {
    bool isDouble = landing.rollAndMove(p, manual, d1, d2);
    has_rolled = true;

    string landName = state.tiles[p.getCurrTile()]->getTileName();
    string landCode = state.tiles[p.getCurrTile()]->getTileCode();
    
    string diceDetail = "Lempar: " + to_string(state.dice.getDie1()) + "+"
                      + to_string(state.dice.getDie2()) + "=" + to_string(state.dice.getTotal())
                      + (isDouble ? " (double)" : "") + " → " + landName + " (" + landCode + ")";
    
    state.addLog(p, "DADU", diceDetail);

    if (isDouble) {
        consec_doubles++;
        if (consec_doubles >= 3) {
            landing.sendToJail(p);
            state.addLog(p, "MASUK_PENJARA", "3x double berturut-turut");
            state.formatter.printBoard(state.board, state.players, state.current_turn, state.max_turn);
            turn_ended = true;
        } else {
            state.formatter.printBoard(state.board, state.players, state.current_turn, state.max_turn);
            landing.applyLanding(p);
            
            if (p.getStatus() == "BANKRUPT") { 
                turn_ended = true; 
                return; 
            }
            state.addLog(p, "DOUBLE", "Giliran tambahan ke-" + to_string(consec_doubles));
            cout << "\n[Dadu ganda - giliran tambahan setelah SELESAI]\n";
            has_bonus_turn = true;
        }
    } else {
        state.formatter.printBoard(state.board, state.players, state.current_turn, state.max_turn);
        landing.applyLanding(p);
        if (p.getStatus() == "BANKRUPT") {
            turn_ended = true;
        }
    }
}

void CommandHandler::executeTurn(Player &p) {
    bool has_rolled = false, turn_ended = false, has_bonus_turn = false;
    bool has_executed_action = false, has_festival_used = false, has_auction_done = false, moved_via_card = false;
    int consec_doubles = 0;
    int pnum = state.getPlayerNum(p);

    cardProc.drawSkillCardAtTurnStart(p);

    cout << "\n=== Giliran " << p.getUsername() << " (P" << pnum << ") "
         << "(Turn " << state.current_turn << "/" << state.max_turn << ") ===\n";
    state.formatter.printPlayerStatus(p);

    cout << "Pemain: ";
    for (int i = 0; i < static_cast<int>(state.players.size()); i++) {
        cout << "P" << (i + 1) << "=" << state.players[i]->getUsername()
             << (state.players[i]->getStatus() == "BANKRUPT" ? "[BANGKRUT]" : "") << "  ";
    }
    cout << "\n\n[Tip: ketik SIMPAN <file> sekarang untuk menyimpan]\n";

    while (!turn_ended && !state.game_over) {
        if (p.getStatus() == "BANKRUPT") { 
            turn_ended = true; 
            break; 
        }

        string cmd;
        cout << "> ";
        if (!(cin >> cmd)) { 
            state.game_over = true; 
            break; 
        }
        for (auto &c : cmd) {
            c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        }

        if (cmd == "BAYAR_DENDA") {
            if (p.getStatus() == "JAIL" && !has_rolled) {
                bankruptcy.cmdBayarDenda(p);
            } else {
                cout << "Hanya bisa di awal giliran dalam penjara.\n";
            }
        } else if (cmd == "CETAK_PAPAN") {
            state.formatter.printBoard(state.board, state.players, state.current_turn, state.max_turn);
        } else if (cmd == "CETAK_AKTA") {
            string code; cin >> code;
            Tile *t = state.board.getTileByCode(code);
            if (t) {
                auto *prop = dynamic_cast<PropertyTile*>(t);
                if (prop) {
                    state.formatter.printAkta(*prop, state.board, state.config);
                } else {
                    cout << "Petak \"" << code << "\" tidak ditemukan atau bukan properti.\n";
                }
            } else {
                cout << "Petak \"" << code << "\" tidak ditemukan atau bukan properti.\n";
            }
        } else if (cmd == "CETAK_PROPERTI") {
            state.formatter.printProperty(p, state.board);
        } else if (cmd == "CETAK_PROFIL") {
            state.formatter.printPlayerStatus(p);
        } else if (cmd == "CETAK_LOG") {
            int n = -1;
            if (cin.peek() != '\n') { 
                int tmp; if (cin >> tmp) n = tmp; 
            }
            state.formatter.printLog(state.transaction_log, n);
        } else if (cmd == "SIMPAN") {
            string slot; cin >> slot;
            if (has_executed_action || has_rolled) {
                cout << "SIMPAN hanya boleh di awal giliran.\n";
            } else {
                try {
                    GameStates::SavePermission perm;
                    perm.has_rolled_dice = false;
                    perm.is_at_start_of_turn = true;
                    
                    string fullpath = slot;
                    if (slot.find('/') == string::npos && slot.find('\\') == string::npos) {
                        fullpath = "save/" + slot;
                    }

                    std::filesystem::create_directories(std::filesystem::path(fullpath).parent_path());
                    FileManager::saveConfig(fullpath, gameLoop.buildSaveState(), perm);
                    cout << "Game disimpan ke " << fullpath << ".\n";
                    state.addLog(p, "SIMPAN", fullpath);
                } catch (const exception &e) { 
                    cout << "Gagal menyimpan: " << e.what() << "\n"; 
                }
            }
        } else if (cmd == "GADAI") {
            string code; cin >> code;
            bankruptcy.cmdGadai(p, code);
            has_executed_action = true;
        } else if (cmd == "TEBUS") {
            string code; cin >> code;
            bankruptcy.cmdTebus(p, code);
            has_executed_action = true;
        } else if (cmd == "BANGUN") {
            bankruptcy.cmdBangun(p, "");
            has_executed_action = true;
        } else if (cmd == "GUNAKAN_KEMAMPUAN") {
            if (has_rolled) {
                cout << "Kartu kemampuan hanya bisa digunakan SEBELUM melempar dadu.\n";
            } else if (p.getHandSize() == 0) {
                cout << "Anda tidak memiliki kartu kemampuan.\n";
            } else {
                cout << "\nDaftar Kartu Kemampuan Spesial Anda:\n";
                for (int i = 0; i < p.getHandSize(); i++) {
                    auto *c = p.getHandCard(i);
                    cout << "  " << (i + 1) << ". " << (c ? c->describe() : "?") << "\n";
                }
                cout << "  0. Batal\n";
                int idx = readInt("Pilih kartu (0-" + to_string(p.getHandSize()) + "): ", 0, p.getHandSize());
                if (idx == 0) {
                    cout << "Dibatalkan.\n";
                } else {
                    int tileBefore = p.getCurrTile();
                    cardProc.cmdGunakanKemampuan(p, idx - 1);
                    has_executed_action = true;
                    if (p.getCurrTile() != tileBefore) {
                        moved_via_card = true;
                        if (state.tiles[p.getCurrTile()]->getTileType() != "GO_JAIL") {
                            if (state.tiles[p.getCurrTile()]->getTileType() == "START" || p.getCurrTile() < tileBefore) {
                                p += state.config.getGoSalary();
                                cout << p.getUsername() << " melewati GO! Menerima gaji M"
                                     << state.config.getGoSalary() << ".\n";
                                state.addLog(p, "GAJI_GO", "M" + to_string(state.config.getGoSalary()));
                            }
                        }
                        if (p.getStatus() != "JAIL" && p.getStatus() != "BANKRUPT") {
                            landing.applyLanding(p);
                        }
                    }
                }
            }
        } else if (cmd == "DROP_KEMAMPUAN") {
            int idx; cin >> idx;
            if (p.getHandCard(idx - 1)) { 
                p.removeHandCard(idx - 1); 
                cout << "Kartu dibuang.\n"; 
            }
            has_executed_action = true;
        } else if ((cmd == "LEMPAR_DADU" || cmd == "ATUR_DADU") && !has_rolled) {
            bool manual = (cmd == "ATUR_DADU");
            int d1 = 0, d2 = 0;
            if (manual) {
                cin >> d1 >> d2;
                if (d1 < 1 || d1 > 6 || d2 < 1 || d2 > 6) { 
                    cout << "Nilai dadu harus 1-6.\n"; 
                    continue; 
                }
            }
            has_executed_action = true;
            if (p.getStatus() == "JAIL") {
                processJailHuman(p, manual, d1, d2, has_rolled, turn_ended);
            } else {
                processNormalRollHuman(p, manual, d1, d2, has_rolled, turn_ended, consec_doubles, has_bonus_turn);
            }
        } else if (cmd == "BELI") {
            if (!has_rolled && !moved_via_card) {
                cout << "Lempar dadu terlebih dahulu.\n";
            } else { 
                cmdBeli(p); 
                has_auction_done = true; 
            }
        } else if (cmd == "BAYAR_SEWA") {
            cout << "Bayar sewa dilakukan otomatis saat mendarat di properti.\n";
        } else if (cmd == "BAYAR_PAJAK") {
            cout << "Pajak dibayar otomatis saat mendarat.\n";
        } else if (cmd == "LELANG") {
            if (!has_rolled && !moved_via_card) {
                cout << "Lempar dadu terlebih dahulu.\n";
            } else { 
                string code; cin >> code; 
                bankruptcy.cmdLelang(p, code); 
                has_auction_done = true; 
            }
        } else if (cmd == "FESTIVAL") {
            if (!has_rolled && !moved_via_card) {
                cout << "Lempar dadu terlebih dahulu.\n";
            } else if (has_festival_used) {
                cout << "Festival sudah digunakan pada giliran ini.\n";
            } else if (state.tiles[p.getCurrTile()]->getTileType() != "FESTIVAL") {
                cout << "Hanya di petak Festival.\n";
            } else if (p.getOwnedProperties().empty()) {
                cout << "Tidak punya properti.\n";
            } else { 
                string code; cin >> code; 
                if (cardProc.cmdFestival(p, code)) {
                    has_festival_used = true; 
                }
            }
        } else if (cmd == "BANTUAN" || cmd == "HELP") {
            cout << "\n=== Daftar Perintah ===\n";
            cout << "  CETAK_PAPAN  LEMPAR_DADU  ATUR_DADU <d1> <d2>\n";
            cout << "  CETAK_AKTA <kode>  CETAK_PROPERTI  CETAK_PROFIL  CETAK_LOG [n]\n";
            cout << "  BELI  GADAI <kode>  TEBUS <kode>  BANGUN\n";
            cout << "  LELANG <kode>  FESTIVAL <kode>\n";
            cout << "  GUNAKAN_KEMAMPUAN  DROP_KEMAMPUAN <idx>\n";
            cout << "  BAYAR_DENDA  SIMPAN <file>  SELESAI  BANTUAN\n\n";
        } else if (cmd == "BANGKRUT") {
            bankruptcy.processBankruptcy(p);
            turn_ended = true;
        } else if (cmd == "SELESAI") {
            if (!has_rolled) {
                if (p.getStatus() == "JAIL") {
                    if (state.jail_turns.find(&p) == state.jail_turns.end()) {
                        state.jail_turns[&p] = 0;
                    }
                    if (state.jail_turns[&p] >= 3) {
                        if (!bankruptcy.tryForceJailFine(p)) {
                            turn_ended = true;
                        }
                    } else {
                        state.jail_turns[&p]++;
                        cout << p.getUsername() << " melewati giliran di penjara ("
                             << state.jail_turns[&p] << "/3).\n";
                        state.addLog(p, "PENJARA", "Melewati giliran " + to_string(state.jail_turns[&p]) + "/3");
                        turn_ended = true;
                    }
                } else {
                    cout << "Harus melempar dadu sebelum selesai.\n";
                }
            } else if ((has_rolled || moved_via_card) && state.hasPendingAction(p) && !has_auction_done && !has_festival_used) {
                Tile *t = state.tiles[p.getCurrTile()];
                auto *prop = dynamic_cast<PropertyTile*>(t);
                
                if (prop && prop->getTileType() == "STREET" && !prop->getTileOwner()) {
                    cout << "Anda tidak membeli properti. Lelang otomatis...\n";
                    bankruptcy.handleAutoAuction(p);
                    has_auction_done = true;
                    turn_ended = true;
                    
                    if (has_bonus_turn && p.getStatus() != "JAIL" && p.getStatus() != "BANKRUPT") {
                        has_rolled = has_bonus_turn = turn_ended = has_festival_used = has_auction_done = false;
                        cout << "\n=== Giliran Tambahan " << p.getUsername() << " (P" << pnum << ") ===\n";
                        state.formatter.printPlayerStatus(p);
                    }
                } else {
                    cout << "Masih ada kewajiban belum diselesaikan.\n";
                }
            } else {
                turn_ended = true;
                if (has_bonus_turn && p.getStatus() != "JAIL" && p.getStatus() != "BANKRUPT") {
                    has_rolled = has_bonus_turn = turn_ended = has_festival_used = has_auction_done = false;
                    cout << "\n=== Giliran Tambahan " << p.getUsername() << " (P" << pnum << ") ===\n";
                    state.formatter.printPlayerStatus(p);
                }
            }
        } else {
            cout << "Perintah tidak dikenal: " << cmd << "\n";
        }
    }
}