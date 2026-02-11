#include "evaluator.hpp"
#include "board_state.hpp"
#include <cmath>
#include <vector>

using Shogi::PieceType;
using Shogi::Turn;

void Evaluator::initialize() {
    if (eval_tables_initialized_) {
        return;
    }

    for (int king_index = 0; king_index < Shogi::BOARD_SIZE; ++king_index) {
        for (int index = 0; index < Shogi::BOARD_SIZE; ++index) {
            int dist = distance(king_index, index);
            defense_weight_table_[king_index][index] = KING_DEFENSE_WEIGHTS[dist];
            threat_weight_table_[king_index][index] = KING_THREAT_WEIGHTS[dist];
        }
    }

    for (int count = 0; count < 11; ++count) {
        multi_effect_weight_table_[count] =
            count == 0 ? 0 : static_cast<int>(6365 - std::pow(0.8525, count - 1) * 5341);
    }

    std::vector<std::vector<std::vector<long long>>> defense_effect_value(
        Shogi::BOARD_SIZE, std::vector<std::vector<long long>>(Shogi::BOARD_SIZE, std::vector<long long>(3)));
    std::vector<std::vector<std::vector<long long>>> threat_effect_value(
        Shogi::BOARD_SIZE, std::vector<std::vector<long long>>(Shogi::BOARD_SIZE, std::vector<long long>(3)));
    for (int king_index = 0; king_index < Shogi::BOARD_SIZE; ++king_index) {
        for (int index = 0; index < Shogi::BOARD_SIZE; ++index) {
            int direction = get_relative_direction(king_index, index);
            for (int effect_count = 0; effect_count < 3; ++effect_count) {
                long long multi_effect = multi_effect_weight_table_[effect_count];
                defense_effect_value[king_index][index][effect_count] =
                    (multi_effect * defense_weight_table_[king_index][index] * DEFENSE_DIRECTION_WEIGHT[direction]) /
                    (1024LL * 1024 * 1024);
                threat_effect_value[king_index][index][effect_count] =
                    (multi_effect * threat_weight_table_[king_index][index] * THREAT_DIRECTION_WEIGHT[direction]) /
                    (1024LL * 1024 * 1024);
            }
        }
    }

    const int support_weights[3] = {0, 33, 43};
    const int attack_weights[3] = {0, 113, 122};

    for (int king_black_index = 0; king_black_index < Shogi::BOARD_SIZE; ++king_black_index) {
        for (int king_white_index = 0; king_white_index < Shogi::BOARD_SIZE; ++king_white_index) {
            for (int index = 0; index < Shogi::BOARD_SIZE; ++index) {
                bool is_near_black_king = (distance(index, king_black_index) == 1);
                bool is_near_white_king = (distance(index, king_white_index) == 1);

                int col = index / Shogi::BOARD_ROWS;
                int row = index % Shogi::BOARD_ROWS;
                int position_bonus_index_black = (8 - col) + row * 9;

                int inv_col = 8 - col;
                int inv_row = 8 - row;
                int position_bonus_index_white = (8 - inv_col) + inv_row * 9;

                int inv_king_white_index = Shogi::BOARD_SIZE - 1 - king_white_index;
                int inv_index = Shogi::BOARD_SIZE - 1 - index;

                for (int black_effect_count = 0; black_effect_count < 3; ++black_effect_count) {
                    for (int white_effect_count = 0; white_effect_count < 3; ++white_effect_count) {
                        double base_score = 0;
                        base_score += defense_effect_value[king_black_index][index][black_effect_count];
                        base_score -= threat_effect_value[king_black_index][index][white_effect_count];
                        base_score -= defense_effect_value[inv_king_white_index][inv_index][white_effect_count];
                        base_score += threat_effect_value[inv_king_white_index][inv_index][black_effect_count];
                        for (int piece_index = 0; piece_index < KKPEE_PIECE_STATE_COUNT; ++piece_index) {
                            double final_score = base_score;
                            bool is_empty = (piece_index == 0);
                            bool is_white = false;
                            bool is_promoted = false;
                            Shogi::PieceType piece_type = Shogi::PieceType::EMPTY;
                            if (!is_empty) {
                                int piece_state = piece_index - 1;
                                is_white = (piece_state >= 16);
                                piece_state %= 16;
                                is_promoted = (piece_state >= 8);
                                piece_type = static_cast<Shogi::PieceType>(piece_state % 8);
                            }

                            // 先手玉の周辺
                            if (is_near_black_king) {
                                if (black_effect_count <= 1) {
                                    if (is_empty || is_white) {
                                        final_score -= 11.0;
                                    } else {
                                        final_score += 20.0;
                                    }
                                } else {
                                    if (!is_empty && !is_white) {
                                        final_score += 11.0;
                                    }
                                }
                            }

                            // 後手玉の周辺
                            if (is_near_white_king) {
                                if (white_effect_count <= 1) {
                                    if (is_empty || !is_white) {
                                        final_score += 11.0;
                                    } else {
                                        final_score -= 20.0;
                                    }
                                } else {
                                    if (!is_empty && is_white) {
                                        final_score -= 11.0;
                                    }
                                }
                            }

                            if (!is_empty) {
                                if (piece_type == Shogi::PieceType::KING) {
                                    if (!is_white) {
                                        final_score += KING_POSITION_BONUS[position_bonus_index_black];
                                    } else {
                                        final_score -= KING_POSITION_BONUS[position_bonus_index_white];
                                    }
                                } else {
                                    int piece_value =
                                        Shogi::PIECE_VALUES[static_cast<int>(piece_type)][is_promoted ? 1 : 0];
                                    if (!is_white) {
                                        final_score +=
                                            (double)piece_value * support_weights[black_effect_count] / 4096.0;
                                        final_score -=
                                            (double)piece_value * attack_weights[white_effect_count] / 4096.0;
                                    } else {
                                        final_score -=
                                            (double)piece_value * support_weights[white_effect_count] / 4096.0;
                                        final_score +=
                                            (double)piece_value * attack_weights[black_effect_count] / 4096.0;
                                    }
                                }
                            }

                            kkpee_table_[king_black_index][king_white_index][index][black_effect_count]
                                        [white_effect_count][piece_index] =
                                            static_cast<int16_t>(final_score * EVAL_SCALE_FACTOR);
                        }
                    }
                }
            }
        }
    }

    eval_tables_initialized_ = true;
}

int Evaluator::get_kkpee_piece_index(const Cell &cell) {
    if (cell.is_empty()) {
        return 0;
    }

    int type_index = static_cast<int>(cell.type);
    int promoted_index = cell.is_promoted ? 8 : 0;
    int turn_index = (cell.turn == Turn::GOTE) ? 16 : 0;
    return 1 + type_index + promoted_index + turn_index;
}

int Evaluator::get_relative_direction(int from_index, int to_index) {
    int col_from = from_index / Shogi::BOARD_ROWS;
    int row_from = from_index % Shogi::BOARD_ROWS;
    int col_to = to_index / Shogi::BOARD_ROWS;
    int row_to = to_index % Shogi::BOARD_ROWS;

    int diff_col = col_to - col_from;
    int diff_row = row_to - row_from;

    if (diff_col > 0) {
        diff_col = -diff_col;
    }

    if (diff_col == 0 && diff_row == 0) {
        // 同じマス
        return 9;
    }
    if (diff_col == 0 && diff_row < 0) {
        // 北
        return 0;
    }
    if (diff_col > diff_row && diff_row < 0) {
        // 北北東
        return 1;
    }
    if (diff_col == diff_row && diff_row < 0) {
        // 北東
        return 2;
    }
    if (diff_col < diff_row && diff_row < 0) {
        // 東北東
        return 3;
    }
    if (diff_col < 0 && diff_row == 0) {
        // 東
        return 4;
    }
    if (diff_col < -diff_row && diff_row > 0) {
        // 東南東
        return 5;
    }
    if (diff_col == -diff_row && diff_row > 0) {
        // 南東
        return 6;
    }
    if (diff_col > -diff_row && diff_row > 0) {
        // 南南東
        return 7;
    }
    if (diff_col == 0 && diff_row > 0) {
        // 南
        return 8;
    }

    return 9;
}

int Evaluator::calculate_score(const BoardState &board) {
    int score = 0;

    // 盤上の駒
    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            const Cell &cell = board.get_cell({col, row});
            if (cell.is_empty()) {
                continue;
            }

            int piece_score = Shogi::get_piece_score(cell.type, cell.is_promoted, cell.turn, col, row);
            piece_score = Shogi::apply_board_discount(piece_score);

            if (cell.turn == Turn::SENTE) {
                score += piece_score;
            } else {
                score -= piece_score;
            }
        }
    }

    // 持ち駒
    for (Turn turn : {Turn::SENTE, Turn::GOTE}) {
        int sign = (turn == Turn::SENTE) ? 1 : -1;
        for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
            int count = board.hand_[static_cast<int>(turn)][piece_type];
            if (count > 0) {
                int value = Shogi::PIECE_VALUES[piece_type][0];
                score += count * value * sign;
            }
        }
    }

    return score;
}

int Evaluator::calculate_spatial_score(const BoardState &board) {
    auto king_position_black = board.get_king_position(Turn::SENTE);
    auto king_position_white = board.get_king_position(Turn::GOTE);

    if (!king_position_black.has_value() || !king_position_white.has_value()) {
        return 0;
    }

    int king_index_black = king_position_black->col * Shogi::BOARD_ROWS + king_position_black->row;
    int king_index_white = king_position_white->col * Shogi::BOARD_ROWS + king_position_white->row;

    Bitboard attacks_any[2];
    Bitboard attacks_multi[2];

    const Bitboard &occupancy = board.bitboard_all_;

    for (int side = 0; side < 2; ++side) {
        Shogi::Turn turn = static_cast<Shogi::Turn>(side);
        const Bitboard promoted_pieces = board.bitboard_promoted_[side];
        for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
            Shogi::PieceType type = static_cast<Shogi::PieceType>(piece_type);
            Bitboard pieces = board.bitboard_piece_[side][piece_type];
            while (!pieces.is_empty()) {
                int from_index = pieces.lsb();
                pieces.clear(from_index);

                bool is_promoted = promoted_pieces.is_set(from_index);
                Bitboard attacks;
                if (is_promoted) {
                    switch (type) {
                    case PieceType::BISHOP:
                        attacks = board.get_promoted_bishop_attacks(from_index, occupancy);
                        break;
                    case PieceType::ROOK:
                        attacks = board.get_promoted_rook_attacks(from_index, occupancy);
                        break;
                    default:
                        attacks = BoardState::get_gold_attacks(from_index, turn);
                        break;
                    }
                } else {
                    switch (type) {
                    case PieceType::PAWN:
                        attacks = BoardState::get_pawn_attacks(from_index, turn);
                        break;
                    case PieceType::LANCE:
                        attacks = board.get_lance_attacks(from_index, turn, occupancy);
                        break;
                    case PieceType::KNIGHT:
                        attacks = BoardState::get_knight_attacks(from_index, turn);
                        break;
                    case PieceType::SILVER:
                        attacks = BoardState::get_silver_attacks(from_index, turn);
                        break;
                    case PieceType::GOLD:
                        attacks = BoardState::get_gold_attacks(from_index, turn);
                        break;
                    case PieceType::BISHOP:
                        attacks = board.get_bishop_attacks(from_index, occupancy);
                        break;
                    case PieceType::ROOK:
                        attacks = board.get_rook_attacks(from_index, occupancy);
                        break;
                    case PieceType::KING:
                        attacks = BoardState::get_king_attacks(from_index);
                        break;
                    default:
                        break;
                    }
                }

                attacks_multi[side] |= (attacks_any[side] & attacks);
                attacks_any[side] |= attacks;
            }
        }
    }

    long long score = 0;
    for (int index = 0; index < Shogi::BOARD_SIZE; ++index) {
        auto count_attacks = [&](int side) {
            return attacks_any[side].is_set(index) + attacks_multi[side].is_set(index);
        };
        int count_black = count_attacks(0);
        int count_white = count_attacks(1);

        int piece_index = get_kkpee_piece_index(board.board_[index]);
        score += kkpee_table_[king_index_black][king_index_white][index][count_black][count_white][piece_index];
    }

    return static_cast<int>(score / EVAL_SCALE_FACTOR);
}
