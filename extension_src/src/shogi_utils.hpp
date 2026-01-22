#ifndef SHOGI_UTILS_HPP
#define SHOGI_UTILS_HPP

#include <array>

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

// 駒の価値
constexpr std::array<std::array<int, 2>, PIECE_TYPE_COUNT> PIECE_VALUES = {{
    {99999, 99999}, // KING
    {640, 950},     // ROOK
    {570, 830},     // BISHOP
    {440, 440},     // GOLD
    {370, 500},     // SILVER
    {260, 510},     // KNIGHT
    {230, 490},     // LANCE
    {90, 530},      // PAWN
}};

// Piece-Square Table
constexpr int PST_PAWN[9][9] = {{20, 20, 20, 20, 20, 20, 20, 20, 20},
                                {20, 20, 20, 20, 20, 20, 20, 20, 20},
                                {15, 15, 15, 20, 25, 20, 15, 15, 15},
                                {5, 5, 5, 10, 20, 10, 5, 5, 5},
                                {0, 0, 0, 5, 10, 5, 0, 0, 0},
                                {-5, -5, -5, 0, 5, 0, -5, -5, -5},
                                {-10, -10, -10, -10, 0, -10, -10, -10, -10},
                                {-10, -10, -10, -10, -10, -10, -10, -10, -10},
                                {-10, -5, 0, 5, 5, 5, 0, -5, -10}};

constexpr int PST_SILVER[9][9] = {{10, 10, 10, 10, 10, 10, 10, 10, 10},         {10, 10, 10, 10, 10, 10, 10, 10, 10},
                                  {10, 15, 15, 20, 20, 20, 15, 15, 10},         {5, 10, 15, 25, 30, 25, 15, 10, 5},
                                  {0, 10, 20, 30, 40, 30, 20, 10, 0},           {-5, 5, 10, 20, 30, 20, 10, 5, -5},
                                  {-10, 0, 5, 10, 15, 10, 5, 0, -10},           {-10, -10, 0, 0, 0, 0, 0, -10, -10},
                                  {-10, -10, -10, -10, -10, -10, -10, -10, -10}};

constexpr int PST_GOLD[9][9] = {{10, 10, 10, 10, 10, 10, 10, 10, 10},
                                {5, 5, 5, 5, 5, 5, 5, 5, 5},
                                {0, 0, 0, 0, 0, 0, 0, 0, 0},
                                {-5, -5, -5, -5, -5, -5, -5, -5, -5},
                                {-10, -10, -5, -5, -5, -5, -5, -10, -10},
                                {-10, -5, 0, 0, 0, 0, 0, -5, -10},
                                {-5, 0, 10, 10, 10, 10, 10, 0, -5},
                                {-10, 0, 15, 15, 15, 15, 15, 0, -10},
                                {-10, -10, -5, 0, 0, 0, -5, -10, -10}};

constexpr int PST_BISHOP[9][9] = {{30, 30, 30, 30, 30, 30, 30, 30, 30},
                                  {30, 30, 30, 30, 30, 30, 30, 30, 30},
                                  {20, 20, 20, 20, 20, 20, 20, 20, 20},
                                  {10, 10, 15, 15, 15, 15, 15, 10, 10},
                                  {5, 10, 15, 20, 20, 20, 15, 10, 5},
                                  {0, 5, 10, 10, 10, 10, 10, 5, 0},
                                  {-5, 0, 0, 0, 0, 0, 0, 0, -5},
                                  {-10, 5, 0, 0, 0, 0, 0, 5, -10},
                                  {-10, -10, -10, -10, -10, -10, -10, -10, -10}};

constexpr int PST_ROOK[9][9] = {
    {40, 40, 40, 40, 40, 40, 40, 40, 40}, {40, 40, 40, 40, 40, 40, 40, 40, 40}, {20, 20, 20, 20, 20, 20, 20, 20, 20},
    {10, 10, 10, 10, 10, 10, 10, 10, 10}, {0, 5, 5, 5, 5, 5, 5, 5, 0},          {-5, 0, 0, 0, 0, 0, 0, 0, -5},
    {-10, 0, 0, 0, 0, 0, 0, 0, -10},      {-10, 5, 5, 10, 10, 10, 5, 5, -10},   {-10, 5, 5, 5, 0, 5, 5, 5, -10}};

constexpr int PST_KING[9][9] = {{0, 0, 0, 0, 0, 0, 0, 0, 0},
                                {0, 0, 0, 0, 0, 0, 0, 0, 0},
                                {-10, -10, -10, -10, -10, -10, -10, -10, -10},
                                {-15, -15, -15, -15, -15, -15, -15, -15, -15},
                                {-20, -20, -20, -20, -20, -20, -20, -20, -20},
                                {-20, -15, -15, -10, -10, -10, -15, -15, -20},
                                {-15, -10, 0, 0, -10, 0, 0, -10, -15},
                                {10, 25, 30, 10, -20, 10, 30, 25, 10},
                                {30, 40, 30, 10, -50, 10, 30, 40, 30}};

constexpr const int (*PST_TABLES[PIECE_TYPE_COUNT])[9] = {
    PST_KING,   // KING
    PST_ROOK,   // ROOK
    PST_BISHOP, // BISHOP
    PST_GOLD,   // GOLD
    PST_SILVER, // SILVER
    nullptr,    // KNIGHT
    nullptr,    // LANCE
    PST_PAWN,   // PAWN
};

inline constexpr int get_pst_value(PieceType piece_type, Turn turn, int col, int row) {
    if (col < 0 || col >= BOARD_COLS || row < 0 || row >= BOARD_ROWS) {
        return 0;
    }
    int pt = static_cast<int>(piece_type);
    if (pt < 0 || pt >= PIECE_TYPE_COUNT) {
        return 0;
    }
    const auto *pst = PST_TABLES[pt];
    if (pst == nullptr) {
        return 0;
    }
    int r = (turn == Turn::SENTE) ? row : (BOARD_ROWS - 1 - row);
    int c = (turn == Turn::SENTE) ? col : (BOARD_COLS - 1 - col);
    return pst[r][c];
}

inline constexpr int get_piece_score(PieceType piece_type, bool is_promoted, Turn turn, int col, int row) {
    int pt = static_cast<int>(piece_type);
    int piece_value = PIECE_VALUES[pt][is_promoted ? 1 : 0];

    PieceType lookup_type = piece_type;
    if (is_promoted) {
        switch (piece_type) {
        case PieceType::PAWN:
        case PieceType::LANCE:
        case PieceType::KNIGHT:
        case PieceType::SILVER:
            lookup_type = PieceType::GOLD;
            break;
        default:
            break;
        }
    }

    int pst_bonus = get_pst_value(lookup_type, turn, col, row);
    return piece_value + pst_bonus;
}

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
    int prev_score;

    UndoInfo()
        : captured_type(static_cast<uint8_t>(PieceType::EMPTY)), captured_promoted(false), prev_hash(0), prev_score(0) {
        prev_pawn_cols[0] = 0;
        prev_pawn_cols[1] = 0;
    }
};

} // namespace Shogi

#endif
