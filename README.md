[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/b842RB8g)

# Nimonspoli

Tugas Besar 1 IF2010 Pemrograman Berorientasi Objek.

Nimonspoli adalah game turn-based bertema Monopoly yang menyediakan dua mode:
- CLI mode untuk interaksi berbasis command
- GUI mode berbasis raylib untuk pengalaman visual interaktif

## Contributors

| Nama                    | NIM      | Github                                          |
| ----------------------- | -------- | ----------------------------------------------- |
| Kevin Wirya Valerian    | 13524019 | [kevin-wirya](https://github.com/kevin-wirya)  |
| Stevanus Agustaf Wongso | 13524020 | [Nusss0](https://github.com/Nusss0)            |
| Jonathan Kris Wicaksono | 13524023 | [Joji0](https://github.com/Joji0)              |
| Emilio Justin           | 13524043 | [Valz0504](https://github.com/Valz0504)        |
| Philipp Hamara          | 13524101 | [philipphqiwu](https://github.com/philipphqiwu) |

## Fitur Utama

- Gameplay Monopoly-style dengan tile properti, pajak, kartu, dan event
- Dukungan Human Player dan Bot Player
- Sistem kartu:
	- Chance
	- Community Chest
	- Special Power Card
- Mekanisme ekonomi:
	- Beli properti
	- Bayar sewa
	- Gadai dan tebus
	- Lelang
	- Pajak PPH/PBM
- Save/Load game state
- Event log transaksi dan aksi permainan
- GUI lengkap dengan:
	- Animasi dadu
	- Popup aksi
	- Tab info pemain/aset/log
	- Interaksi skill card

## Arsitektur Singkat

Desain kelas dipisah berdasarkan separation of concerns:
- Data/State layer:
	- Player, Board, Tile dan turunannya
	- Card dan turunannya
	- Dice, LogEntry, GameConfig
- Service/Logic layer:
	- Manager spesifik domain (Property, Rent, Tax, Card, Skill, Festival, Auction, dll)
- Orchestration layer:
	- GameLoop sebagai pengendali alur utama
	- CommandHandler dan BotController untuk eksekusi turn

Diagram Mermaid per modul tersedia di folder diagrams.

## Struktur Folder Penting

```text
tugas-besar-1-ori/
├── src/        # implementasi C++
├── include/    # header C++
├── config/     # file konfigurasi board/rule
├── assets/     # aset GUI
├── save/       # save file game
├── docs/       # dokumentasi tambahan
└── makefile    # script build CLI dan GUI
```

## Requirements

### Wajib

- C++17 compatible compiler (contoh: g++)
- make

### Untuk GUI Mode

- raylib

Linux biasanya membutuhkan:
- libraylib-dev
- OpenGL/X11 related libs

Pada makefile, dependency GUI Linux sudah di-handle lewat pkg-config raylib (jika tersedia).

## Cara Build

### 1) Build CLI

```bash
make all
```

Output binary:
- bin/game

### 2) Build GUI

```bash
make gui
```

Output binary:
- bin/game_gui

### 3) Clean

```bash
make clean
```

## Cara Menjalankan

### CLI

```bash
make run
```

Atau jalankan langsung:

```bash
./bin/game
```

### GUI

```bash
make run-gui
```

Atau jalankan langsung:

```bash
./bin/game_gui
```

## Alur Main (CLI)

Saat program dimulai:
1. Input path direktori konfigurasi (contoh: config/)
2. Pilih mode:
	 - New Game
	 - Load Game
3. Ikuti prompt command sampai game selesai
4. Gunakan fitur >HELP untuk mendapatkan list of commands.

## Alur Main (GUI)

Saat GUI berjalan:
1. Masuk Home Screen
2. Pilih New Game atau Load Game
3. Main dari Game Screen dengan tombol aksi yang tersedia
4. Gunakan tombol SAVE untuk menyimpan game
