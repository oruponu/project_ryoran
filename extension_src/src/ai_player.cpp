#include "ai_player.hpp"
#include <algorithm>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

using namespace godot;
using Shogi::Coord;
using Shogi::Move;
using Shogi::PieceType;
using Shogi::Turn;

namespace {

constexpr std::array HAND_PIECE_TYPES = {
    PieceType::PAWN, PieceType::LANCE,  PieceType::KNIGHT, PieceType::SILVER,
    PieceType::GOLD, PieceType::BISHOP, PieceType::ROOK,
};

} // namespace

int AIPlayer::get_move_ordering_score(const BoardState &board, const Shogi::Move &move) {
    int score = 0;

    // 駒を取る手
    if (move.is_capture) {
        const Cell &target_cell = board.get_cell({move.to_col, move.to_row});
        if (!target_cell.is_empty()) {
            int victim_value = Shogi::PIECE_VALUES[static_cast<int>(target_cell.type)][target_cell.is_promoted ? 1 : 0];
            int aggressor_value = Shogi::PIECE_VALUES[static_cast<int>(move.piece_type)][0];
            // 高い駒を安い駒で取るほど高得点
            score = 1000000 + victim_value - aggressor_value;
        }
    }

    // 成る手
    if (move.is_promotion) {
        score += 20000;
    }

    return score;
}

int AIPlayer::alpha_beta(BoardState &board, int depth, int alpha, int beta, Turn turn, uint64_t end_time, bool &timeout,
                         uint64_t &node_count) {
    ++node_count;

    if (Time::get_singleton()->get_ticks_usec() > end_time) {
        timeout = true;
        return 0;
    }

    uint64_t hash = board.get_zobrist_hash();
    int original_alpha = alpha;
    Move tt_best_move{};
    bool has_tt_move = false;

    TTEntry *tt_entry = probe_tt(hash);
    if (tt_entry != nullptr && tt_entry->depth >= depth) {
        if (tt_entry->flag == TTFlag::EXACT) {
            return tt_entry->score;
        } else if (tt_entry->flag == TTFlag::LOWER_BOUND) {
            alpha = std::max(alpha, tt_entry->score);
        } else if (tt_entry->flag == TTFlag::UPPER_BOUND) {
            beta = std::min(beta, tt_entry->score);
        }

        if (alpha >= beta) {
            return tt_entry->score;
        }
    }

    // TTから最善手を取得
    if (tt_entry != nullptr) {
        tt_best_move = tt_entry->best_move;
        has_tt_move = true;
    }

    if (depth <= 0) {
        // 静止探索を実行
        return quiescence_search(board, alpha, beta, turn, node_count);
    }

    Turn turn_to_move = board.get_turn_to_move();
    std::vector<Move> moves = board.get_legal_moves();
    if (moves.empty()) {
        // 投了
        return (turn == turn_to_move) ? -999999 : 999999;
    }

    if (has_tt_move) {
        auto it = std::find(moves.begin(), moves.end(), tt_best_move);
        if (it != moves.end()) {
            std::rotate(moves.begin(), it, it + 1);
        }
    }

    auto sort_start = has_tt_move ? moves.begin() + 1 : moves.begin();
    std::sort(sort_start, moves.end(), [&](const Move &a, const Move &b) {
        return get_move_ordering_score(board, a) > get_move_ordering_score(board, b);
    });

    Turn next_side = (turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
    Move best_move = moves[0];

    if (turn == Turn::SENTE) {
        int max_eval = -99999999;
        for (const Move &move : moves) {
            Shogi::UndoInfo undo = board.apply_move(move);
            int eval = alpha_beta(board, depth - 1, alpha, beta, next_side, end_time, timeout, node_count);
            board.undo_move(undo);

            if (timeout) {
                return 0;
            }

            if (eval > max_eval) {
                max_eval = eval;
                best_move = move;
            }
            alpha = std::max(alpha, eval);
            if (beta <= alpha) {
                break; // βカット
            }
        }

        TTFlag flag;
        if (max_eval <= original_alpha) {
            flag = TTFlag::UPPER_BOUND;
        } else if (max_eval >= beta) {
            flag = TTFlag::LOWER_BOUND;
        } else {
            flag = TTFlag::EXACT;
        }
        store_tt(hash, max_eval, depth, flag, best_move);

        return max_eval;
    } else {
        int min_eval = 99999999;
        for (const Move &move : moves) {
            Shogi::UndoInfo undo = board.apply_move(move);
            int eval = alpha_beta(board, depth - 1, alpha, beta, next_side, end_time, timeout, node_count);
            board.undo_move(undo);

            if (timeout) {
                return 0;
            }

            if (eval < min_eval) {
                min_eval = eval;
                best_move = move;
            }
            beta = std::min(beta, eval);
            if (beta <= alpha) {
                break; // αカット
            }
        }

        TTFlag flag;
        if (min_eval >= beta) {
            flag = TTFlag::LOWER_BOUND;
        } else if (min_eval <= original_alpha) {
            flag = TTFlag::UPPER_BOUND;
        } else {
            flag = TTFlag::EXACT;
        }
        store_tt(hash, min_eval, depth, flag, best_move);

        return min_eval;
    }
}

int AIPlayer::quiescence_search(BoardState &board, int alpha, int beta, Turn turn, uint64_t &node_count) {
    ++node_count;

    int stand_pat = board.get_score();

    if (turn == Turn::SENTE) {
        if (stand_pat >= beta) {
            return beta;
        }

        if (stand_pat > alpha) {
            alpha = stand_pat;
        }

        std::vector<Move> moves = board.get_legal_moves(true);
        std::sort(moves.begin(), moves.end(), [&](const Move &a, const Move &b) {
            return get_move_ordering_score(board, a) > get_move_ordering_score(board, b);
        });

        int max_eval = stand_pat;

        for (const auto &move : moves) {
            Shogi::UndoInfo undo = board.apply_move(move);
            int eval = quiescence_search(board, alpha, beta, Turn::GOTE, node_count);
            board.undo_move(undo);

            max_eval = std::max(max_eval, eval);
            alpha = std::max(alpha, eval);

            if (alpha >= beta) {
                return beta; // βカット
            }
        }

        return max_eval;
    } else {
        if (stand_pat <= alpha) {
            return alpha;
        }

        if (stand_pat < beta) {
            beta = stand_pat;
        }

        std::vector<Move> moves = board.get_legal_moves(true);
        std::sort(moves.begin(), moves.end(), [&](const Move &a, const Move &b) {
            return get_move_ordering_score(board, a) > get_move_ordering_score(board, b);
        });

        int min_eval = stand_pat;

        for (const auto &move : moves) {
            Shogi::UndoInfo undo = board.apply_move(move);
            int eval = quiescence_search(board, alpha, beta, Turn::SENTE, node_count);
            board.undo_move(undo);

            min_eval = std::min(min_eval, eval);
            beta = std::min(beta, eval);

            if (beta <= alpha) {
                return alpha; // αカット
            }
        }

        return min_eval;
    }
}

double AIPlayer::calculate_win_probability(int score) {
    const double SCALING_FACTOR = 3333.0;
    return 1.0 / (1.0 + std::pow(10.0, -static_cast<double>(score) / SCALING_FACTOR));
}

TTEntry *AIPlayer::probe_tt(uint64_t hash) {
    auto it = transposition_table_.find(hash);
    if (it != transposition_table_.end() && it->second.hash == hash) {
        return &it->second;
    }
    return nullptr;
}

void AIPlayer::store_tt(uint64_t hash, int score, int depth, TTFlag flag, const Move &best_move) {
    auto it = transposition_table_.find(hash);
    if (it == transposition_table_.end() || it->second.depth <= depth) {
        transposition_table_[hash] = TTEntry{hash, score, depth, flag, best_move};
    }
}

void AIPlayer::clear_tt() { transposition_table_.clear(); }

Dictionary AIPlayer::search_best_move(BoardState board) {
    Turn root_side = board.get_turn_to_move();
    std::vector<Move> moves = board.get_legal_moves();

    if (moves.empty()) {
        // 投了
        Dictionary result;
        result["win_rate"] = 0.0;
        return result;
    }

    if (transposition_table_.size() > TT_SIZE) {
        UtilityFunctions::print("TT size exceeded limit, clearing. Size was: ",
                                static_cast<int64_t>(transposition_table_.size()));
        clear_tt();
    }

    uint64_t start_time = Time::get_singleton()->get_ticks_usec();
    uint64_t strict_limit_time = start_time + TIME_LIMIT_USEC;

    int max_depth_limit = 10;

    Move global_best_move = moves[0];
    int global_best_score = (root_side == Turn::SENTE) ? -99999999 : 99999999;

    // TTから最善手を取得
    uint64_t root_hash = board.get_zobrist_hash();
    TTEntry *root_tt = probe_tt(root_hash);
    Move best_move_prev_iter = (root_tt != nullptr) ? root_tt->best_move : moves[0];
    bool has_prev_best = (root_tt != nullptr);

    uint64_t total_node_count = 0;

    for (int depth = 1; depth <= max_depth_limit; ++depth) {
        if (depth > 1 && Time::get_singleton()->get_ticks_usec() > strict_limit_time) {
            UtilityFunctions::print("Time limit reached before depth ", depth);
            break;
        }

        std::sort(moves.begin(), moves.end(), [&](const Move &a, const Move &b) {
            return get_move_ordering_score(board, a) > get_move_ordering_score(board, b);
        });

        if (has_prev_best) {
            auto it = std::find(moves.begin(), moves.end(), best_move_prev_iter);
            if (it != moves.end()) {
                std::rotate(moves.begin(), it, it + 1);
            }
        }

        int alpha = -99999999;
        int beta = 99999999;
        Move current_depth_best_move = moves[0];
        int current_depth_best_score = (root_side == Turn::SENTE) ? -99999999 : 99999999;
        Turn next_turn_side = (root_side == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;

        uint64_t search_cutoff_time = (depth == 1) ? UINT64_MAX : strict_limit_time;
        bool timeout = false;

        for (const auto &move : moves) {
            if (depth > 1 && Time::get_singleton()->get_ticks_usec() > strict_limit_time) {
                timeout = true;
                break;
            }

            Shogi::UndoInfo undo = board.apply_move(move);
            int score = alpha_beta(board, depth - 1, alpha, beta, next_turn_side, search_cutoff_time, timeout,
                                   total_node_count);
            board.undo_move(undo);

            if (timeout) {
                break;
            }

            bool update_best = false;
            if (root_side == Turn::SENTE) {
                if (score > current_depth_best_score) {
                    current_depth_best_score = score;
                    update_best = true;
                }

                alpha = std::max(alpha, score);
            } else {
                if (score < current_depth_best_score) {
                    current_depth_best_score = score;
                    update_best = true;
                }

                beta = std::min(beta, score);
            }

            if (update_best) {
                current_depth_best_move = move;
            }
        }

        if (timeout) {
            UtilityFunctions::print("Time limit reached before depth ", depth);
            break;
        }

        global_best_move = current_depth_best_move;
        global_best_score = current_depth_best_score;

        best_move_prev_iter = global_best_move;
        has_prev_best = true;

        int display_score = (root_side == Turn::SENTE) ? global_best_score : -global_best_score;
        double win_prob = calculate_win_probability(display_score);
        UtilityFunctions::print("Depth ", depth, " completed. BestScore: ", global_best_score,
                                ", WinRate: ", String::num(win_prob * 100.0, 1), "%");

        // 詰み筋を見つけたら打ち切り
        if (global_best_score >= 999999 || global_best_score <= -999999) {
            UtilityFunctions::print("Checkmate found at depth ", depth);
            break;
        }
    }

    UtilityFunctions::print("Total nodes searched: ", total_node_count,
                            ", TT size: ", static_cast<int64_t>(transposition_table_.size()));

    const auto &best_move = global_best_move;
    int final_score = (root_side == Turn::SENTE) ? global_best_score : -global_best_score;
    float win_rate = calculate_win_probability(final_score);

    Dictionary result;
    result["from_col"] = best_move.from_col;
    result["from_row"] = best_move.from_row;
    result["to_col"] = best_move.to_col;
    result["to_row"] = best_move.to_row;
    result["piece_type"] = static_cast<int>(best_move.piece_type);
    result["is_promotion"] = best_move.is_promotion;
    result["is_drop"] = best_move.is_drop;
    result["win_rate"] = win_rate;

    return result;
}
