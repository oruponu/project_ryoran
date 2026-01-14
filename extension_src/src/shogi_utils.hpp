#ifndef SHOGI_UTILS_HPP
#define SHOGI_UTILS_HPP

namespace Shogi {

enum PieceType { KING = 0, ROOK = 1, BISHOP = 2, GOLD = 3, SILVER = 4, KNIGHT = 5, LANCE = 6, PAWN = 7, EMPTY = 255 };

enum Side { PLAYER = 0, ENEMY = 1 };

// 盤面のサイズ
const int BOARD_COLS = 9;
const int BOARD_ROWS = 9;
const int BOARD_SIZE = 81;

// 駒の種類数
const int PIECE_TYPE_COUNT = 8;

struct Move {
    uint8_t from_col;
    uint8_t from_row;
    uint8_t to_col;
    uint8_t to_row;
    uint8_t piece_type;
    bool is_promotion;
    bool is_drop;
    bool is_capture;

    Move()
        : from_col(0), from_row(0), to_col(0), to_row(0), piece_type(EMPTY), is_promotion(false), is_drop(false),
          is_capture(false) {}

    Move(int fc, int fr, int tc, int tr, int pt, bool promo, bool drop, bool capture)
        : from_col((uint8_t)fc), from_row((uint8_t)fr), to_col((uint8_t)tc), to_row((uint8_t)tr),
          piece_type((uint8_t)pt), is_promotion(promo), is_drop(drop), is_capture(capture) {}
};

} // namespace Shogi

#endif
