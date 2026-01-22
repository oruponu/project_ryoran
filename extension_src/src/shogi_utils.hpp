#ifndef SHOGI_UTILS_HPP
#define SHOGI_UTILS_HPP

namespace Shogi {

enum class PieceType : uint8_t {
    KING = 0,
    ROOK = 1,
    BISHOP = 2,
    GOLD = 3,
    SILVER = 4,
    KNIGHT = 5,
    LANCE = 6,
    PAWN = 7,
    EMPTY = 255
};

enum class Turn : uint8_t { SENTE = 0, GOTE = 1 };

// 盤面のサイズ
constexpr int BOARD_COLS = 9;
constexpr int BOARD_ROWS = 9;
constexpr int BOARD_SIZE = 81;

// 駒の種類数
constexpr int PIECE_TYPE_COUNT = 8;

struct Coord {
    int col;
    int row;

    Coord() : col(-1), row(-1) {}
    Coord(int c, int r) : col(c), row(r) {}

    [[nodiscard]] bool is_valid() const { return col >= 0 && col < BOARD_COLS && row >= 0 && row < BOARD_ROWS; }

    [[nodiscard]] bool operator==(const Coord &other) const { return col == other.col && row == other.row; }
    [[nodiscard]] bool operator!=(const Coord &other) const { return !(*this == other); }
};

struct Move {
    uint8_t from_col;
    uint8_t from_row;
    uint8_t to_col;
    uint8_t to_row;
    PieceType piece_type;
    bool is_promotion;
    bool is_drop;
    bool is_capture;

    Move()
        : from_col(0), from_row(0), to_col(0), to_row(0), piece_type(PieceType::EMPTY), is_promotion(false),
          is_drop(false), is_capture(false) {}

    Move(int fc, int fr, int tc, int tr, PieceType pt, bool promo, bool drop, bool capture)
        : from_col(static_cast<uint8_t>(fc)), from_row(static_cast<uint8_t>(fr)), to_col(static_cast<uint8_t>(tc)),
          to_row(static_cast<uint8_t>(tr)), piece_type(pt), is_promotion(promo), is_drop(drop), is_capture(capture) {}

    [[nodiscard]] bool operator==(const Move &other) const {
        return from_col == other.from_col && from_row == other.from_row && to_col == other.to_col &&
               to_row == other.to_row && piece_type == other.piece_type && is_promotion == other.is_promotion &&
               is_drop == other.is_drop && is_capture == other.is_capture;
    }

    [[nodiscard]] bool operator!=(const Move &other) const { return !(*this == other); }
};

struct UndoInfo {
    Move move;
    uint8_t captured_type;
    bool captured_promoted;
    uint64_t prev_hash;
    uint16_t prev_pawn_cols[2];

    UndoInfo() : captured_type(static_cast<uint8_t>(PieceType::EMPTY)), captured_promoted(false), prev_hash(0) {
        prev_pawn_cols[0] = 0;
        prev_pawn_cols[1] = 0;
    }
};

} // namespace Shogi

#endif
