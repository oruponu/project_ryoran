#pragma once

#include "shogi_utils.hpp"
#include <cstdint>

class BoardState;
struct Cell;

class Evaluator {
  private:
    static constexpr int KING_DEFENSE_WEIGHTS[9] = {84992, 42496, 28330, 21248, 16998, 14165, 12141, 10624, 9443};
    static constexpr int KING_THREAT_WEIGHTS[9] = {94208, 47104, 31402, 23552, 18841, 15701, 13458, 11776, 10467};
    static constexpr int KKPEE_PIECE_STATE_COUNT = 40;
    static constexpr int EVAL_SCALE_FACTOR = 32;

    inline static const int KING_POSITION_BONUS[Shogi::BOARD_SIZE] = {
        875, 655, 830, 680, 770, 815, 720, 945, 755, 605, 455, 610, 595, 730, 610, 600, 590, 615, 565, 640, 555,
        525, 635, 565, 440, 600, 575, 520, 515, 580, 420, 640, 535, 565, 500, 510, 220, 355, 240, 375, 340, 335,
        305, 275, 320, 500, 530, 560, 445, 510, 395, 455, 490, 410, 345, 275, 250, 355, 295, 280, 420, 235, 135,
        335, 370, 385, 255, 295, 200, 265, 305, 305, 255, 225, 245, 295, 200, 320, 275, 70,  200};

    inline static const int DEFENSE_DIRECTION_WEIGHT[10] = {1120, 1872, 112, 760, 744, 880, 1320, 600, 904, 1024};
    inline static const int THREAT_DIRECTION_WEIGHT[10] = {1056, 1714, 1688, 1208, 248, 240, 496, 816, 928, 1024};

    inline static bool eval_tables_initialized_;

    inline static int16_t kkpee_table_[Shogi::BOARD_SIZE][Shogi::BOARD_SIZE][Shogi::BOARD_SIZE][3][3]
                                      [KKPEE_PIECE_STATE_COUNT];
    inline static int defense_weight_table_[Shogi::BOARD_SIZE][Shogi::BOARD_SIZE];
    inline static int threat_weight_table_[Shogi::BOARD_SIZE][Shogi::BOARD_SIZE];
    inline static int multi_effect_weight_table_[11];

    static int get_kkpee_piece_index(const Cell &cell);
    static int get_relative_direction(int from_index, int to_index);

    static int distance(int lhs, int rhs) {
        int col1 = lhs / Shogi::BOARD_ROWS;
        int row1 = lhs % Shogi::BOARD_ROWS;
        int col2 = rhs / Shogi::BOARD_ROWS;
        int row2 = rhs % Shogi::BOARD_ROWS;
        return std::max(std::abs(col1 - col2), std::abs(row1 - row2));
    }

  public:
    static void initialize();
    [[nodiscard]] static int calculate_score(const BoardState &board);
    [[nodiscard]] static int calculate_spatial_score(const BoardState &board);
};
