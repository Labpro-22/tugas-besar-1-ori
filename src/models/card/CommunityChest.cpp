#include "include/models/card/CommunityChest.hpp"
#include "include/models/player/Player.hpp"
#include <iostream>

// ==================== HappyBirthdayCard ====================
// "Ini adalah hari ulang tahun Anda. Dapatkan M100 dari setiap pemain."
// 
// NOTE: Kartu ini membutuhkan akses ke semua pemain lain.
// Karena signature action(Player&) hanya menerima 1 player,
// implementasi sebenarnya perlu dihandle oleh ActionManager
// yang memanggil addBalance pada semua pemain.
// Untuk saat ini, action() hanya menandai bahwa kartu ini aktif.
// ActionManager.getCard() akan mengurus transfer antar pemain.

HappyBirthdayCard::HappyBirthdayCard() : CommunityChestCard() {}

void HappyBirthdayCard::action(Player& player) {
    // Transfer M100 dari setiap pemain ke player ini akan dihandle oleh ActionManager.
    // ActionManager memiliki akses ke semua player dan bisa melakukan:
    //   - Kurangi M100 dari setiap pemain lain
    //   - Tambah total ke player ini
    // Fungsi ini dipanggil setelah ActionManager selesai transfer.
    std::cout << "Selamat ulang tahun, " << player.getUsername() << "! "
              << "Menerima M100 dari setiap pemain." << std::endl;
}

SickCard::SickCard() : CommunityChestCard() {}

void SickCard::action(Player& player) {
    player.addBalance(-700);

    std::cout << player.getUsername() << " membayar biaya dokter sebesar M700." << std::endl;
}

// ==================== LegislativeCard ====================
// "Anda mau nyaleg. Bayar M200 kepada setiap pemain."
//
// NOTE: Sama seperti HappyBirthdayCard, transfer antar pemain
// dihandle oleh ActionManager.

LegislativeCard::LegislativeCard() : CommunityChestCard() {}

void LegislativeCard::action(Player& player) {
    // Transfer M200 ke setiap pemain lain akan dihandle oleh ActionManager.
    // ActionManager memiliki akses ke semua player dan bisa melakukan:
    //   - Kurangi total dari player ini
    //   - Tambah M200 ke setiap pemain lain
    // Fungsi ini dipanggil setelah ActionManager selesai transfer.
    std::cout << player.getUsername() << " mau nyaleg! "
              << "Membayar M200 kepada setiap pemain." << std::endl;
}