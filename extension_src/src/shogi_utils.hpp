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

} // namespace Shogi

#endif
