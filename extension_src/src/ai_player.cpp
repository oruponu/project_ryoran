#include "ai_player.hpp"
#include <algorithm>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

using namespace godot;

std::vector<Shogi::Move> AIPlayer::get_legal_moves(const BoardState &board, int side) {
    std::vector<Shogi::Move> moves;
    bool is_enemy_turn = (side == Shogi::ENEMY);

    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            const Cell &cell = board.get_cell(col, row);

            // 自駒でないならスキップ
            if (cell.is_empty() || cell.side != side) {
                continue;
            }

            for (int t_col = 0; t_col < Shogi::BOARD_COLS; ++t_col) {
                for (int t_row = 0; t_row < Shogi::BOARD_ROWS; ++t_row) {
                    if (board.is_legal_move(col, row, t_col, t_row)) {
                        bool is_capture = !board.get_cell(t_col, t_row).is_empty();
                        bool can_promote = false;
                        bool must_promote = false;

                        if (!cell.is_promoted && cell.type != Shogi::KING && cell.type != Shogi::GOLD) {
                            int zone_min = is_enemy_turn ? 6 : 0;
                            int zone_max = is_enemy_turn ? 8 : 2;
                            bool from_in_zone = (row >= zone_min && row <= zone_max);
                            bool to_in_zone = (t_row >= zone_min && t_row <= zone_max);

                            if (from_in_zone || to_in_zone) {
                                can_promote = true;
                            }
                        }

                        if (board.is_dead_end(cell.type, is_enemy_turn, t_row)) {
                            must_promote = true;
                        }

                        if (!must_promote) {
                            moves.emplace_back(col, row, t_col, t_row, cell.type, false, false, is_capture);
                        }

                        if (can_promote) {
                            moves.emplace_back(col, row, t_col, t_row, cell.type, true, false, is_capture);
                        }
                    }
                }
            }
        }
    }

    for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
        if (board.get_hand_count(side, piece_type) > 0) {
            for (int t_col = 0; t_col < Shogi::BOARD_COLS; ++t_col) {
                for (int t_row = 0; t_row < Shogi::BOARD_ROWS; ++t_row) {
                    if (board.is_legal_drop(piece_type, is_enemy_turn, t_col, t_row)) {
                        moves.emplace_back(0, 0, t_col, t_row, piece_type, false, true, false);
                    }
                }
            }
        }
    }

    return moves;
}

int AIPlayer::evaluate(const BoardState &board) {
    int score = 0;
    int my_side = is_enemy_side ? Shogi::ENEMY : Shogi::PLAYER;

    // 盤上の駒
    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            const Cell &cell = board.get_cell(col, row);
            if (cell.is_empty()) {
                continue;
            }

            int piece_value = 0;
            switch (cell.type) {
            case Shogi::PAWN:
                piece_value = cell.is_promoted ? VAL_PRO_PAWN : VAL_PAWN;
                break;
            case Shogi::LANCE:
                piece_value = cell.is_promoted ? VAL_PRO_LANCE : VAL_LANCE;
                break;
            case Shogi::KNIGHT:
                piece_value = cell.is_promoted ? VAL_PRO_KNIGHT : VAL_KNIGHT;
                break;
            case Shogi::SILVER:
                piece_value = cell.is_promoted ? VAL_PRO_SILVER : VAL_SILVER;
                break;
            case Shogi::GOLD:
                piece_value = VAL_GOLD;
                break;
            case Shogi::BISHOP:
                piece_value = cell.is_promoted ? VAL_PRO_BISHOP : VAL_BISHOP;
                break;
            case Shogi::ROOK:
                piece_value = cell.is_promoted ? VAL_PRO_ROOK : VAL_ROOK;
                break;
            case Shogi::KING:
                piece_value = VAL_KING;
                break;
            default:
                break;
            }

            if (cell.side == my_side) {
                score += piece_value;
            } else {
                score -= piece_value;
            }
        }
    }

    // 持ち駒
    for (int side = 0; side < 2; ++side) {
        int sign = (side == my_side) ? 1 : -1;
        score += board.get_hand_count(side, Shogi::PAWN) * VAL_PAWN * sign;
        score += board.get_hand_count(side, Shogi::LANCE) * VAL_LANCE * sign;
        score += board.get_hand_count(side, Shogi::KNIGHT) * VAL_KNIGHT * sign;
        score += board.get_hand_count(side, Shogi::SILVER) * VAL_SILVER * sign;
        score += board.get_hand_count(side, Shogi::GOLD) * VAL_GOLD * sign;
        score += board.get_hand_count(side, Shogi::BISHOP) * VAL_BISHOP * sign;
        score += board.get_hand_count(side, Shogi::ROOK) * VAL_ROOK * sign;
    }

    return score;
}

int AIPlayer::alpha_beta(BoardState board, int depth, int alpha, int beta, int side, uint64_t end_time) {
    if (Time::get_singleton()->get_ticks_usec() > end_time) {
        throw SearchTimeoutException();
    }

    if (depth == 0) {
        return evaluate(board);
    }

    std::vector<Shogi::Move> moves = get_legal_moves(board, side);
    int my_side = is_enemy_side ? Shogi::ENEMY : Shogi::PLAYER;

    if (moves.empty()) {
        // 投了
        return (side == my_side) ? -999999 : 999999;
    }

    // 取る手を優先
    std::sort(moves.begin(), moves.end(),
              [](const Shogi::Move &a, const Shogi::Move &b) { return a.is_capture > b.is_capture; });

    int next_side = (side == Shogi::PLAYER) ? Shogi::ENEMY : Shogi::PLAYER;

    if (side == my_side) {
        int max_eval = -99999999;
        for (const Shogi::Move &move : moves) {
            BoardState next_board = board;
            next_board.apply_move(move, side);
            int eval = alpha_beta(next_board, depth - 1, alpha, beta, next_side, end_time);
            max_eval = std::max(max_eval, eval);
            alpha = std::max(alpha, eval);
            if (beta <= alpha) {
                break; // βカット
            }
        }

        return max_eval;
    } else {
        int min_eval = 99999999;
        for (const Shogi::Move &move : moves) {
            BoardState next_board = board;
            next_board.apply_move(move, side);
            int eval = alpha_beta(next_board, depth - 1, alpha, beta, next_side, end_time);
            min_eval = std::min(min_eval, eval);
            beta = std::min(beta, eval);
            if (beta <= alpha) {
                break; // αカット
            }
        }

        return min_eval;
    }
}

double AIPlayer::calculate_win_probability(int score) {
    const double SCALING_FACTOR = 3333.0;
    return 1.0 / (1.0 + std::pow(10.0, -static_cast<double>(score) / SCALING_FACTOR));
}

Dictionary AIPlayer::search_best_move(BoardState board) {
    int my_side = is_enemy_side ? Shogi::ENEMY : Shogi::PLAYER;
    std::vector<Shogi::Move> moves = get_legal_moves(board, my_side);

    if (moves.empty()) {
        // 投了
        return Dictionary();
    }

    uint64_t start_time = Time::get_singleton()->get_ticks_usec();
    uint64_t end_time = start_time + TIME_LIMIT_USEC;

    int max_depth_limit = 10;

    Shogi::Move global_best_move = moves[0];
    int global_best_score = -99999999;

    Shogi::Move best_move_prev_iter = moves[0];
    bool has_prev_best = false;

    for (int depth = 1; depth <= max_depth_limit; ++depth) {
        if (has_prev_best) {
            auto it = std::find_if(moves.begin(), moves.end(), [&](const Shogi::Move &m) {
                return m.from_col == best_move_prev_iter.from_col && m.from_row == best_move_prev_iter.from_row &&
                       m.to_col == best_move_prev_iter.to_col && m.to_row == best_move_prev_iter.to_row &&
                       m.piece_type == best_move_prev_iter.piece_type &&
                       m.is_promotion == best_move_prev_iter.is_promotion && m.is_drop == best_move_prev_iter.is_drop;
            });
            if (it != moves.end()) {
                std::rotate(moves.begin(), it, it + 1);
            }
        } else {
            std::sort(moves.begin(), moves.end(),
                      [](const Shogi::Move &a, const Shogi::Move &b) { return a.is_capture > b.is_capture; });
        }

        try {
            int alpha = -99999999;
            int beta = 99999999;
            Shogi::Move current_depth_best_move = moves[0];
            int current_depth_best_score = -99999999;
            int next_turn_side = (my_side == Shogi::PLAYER) ? Shogi::ENEMY : Shogi::PLAYER;

            for (const auto &move : moves) {
                if (Time::get_singleton()->get_ticks_usec() > end_time) {
                    throw SearchTimeoutException();
                }

                BoardState next_board = board;
                next_board.apply_move(move, my_side);

                int score = alpha_beta(next_board, depth - 1, alpha, beta, next_turn_side, end_time);

                if (score > current_depth_best_score) {
                    current_depth_best_score = score;
                    current_depth_best_move = move;
                }

                alpha = std::max(alpha, score);
            }

            global_best_move = current_depth_best_move;
            global_best_score = current_depth_best_score;

            best_move_prev_iter = global_best_move;
            has_prev_best = true;

            double win_prob = calculate_win_probability(global_best_score);
            UtilityFunctions::print("Depth ", depth, " completed. BestScore: ", global_best_score,
                                    ", WinRate: ", String::num(win_prob * 100.0, 1), "%");

            // 詰み筋を見つけたら打ち切り
            if (global_best_score >= 999999 || global_best_score <= -999999) {
                UtilityFunctions::print("Checkmate found at depth ", depth);
                break;
            }
        } catch (SearchTimeoutException) {
            UtilityFunctions::print("Time limit reached at depth ", depth);
            break;
        }
    }

    const auto &best_move = global_best_move;
    float win_rate = calculate_win_probability(global_best_score);

    Dictionary result;
    result["from_col"] = best_move.from_col;
    result["from_row"] = best_move.from_row;
    result["to_col"] = best_move.to_col;
    result["to_row"] = best_move.to_row;
    result["piece_type"] = best_move.piece_type;
    result["is_promotion"] = best_move.is_promotion;
    result["is_drop"] = best_move.is_drop;
    result["win_rate"] = win_rate;

    return result;
}
