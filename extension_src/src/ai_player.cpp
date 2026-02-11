#include "ai_player.hpp"
#include "move_generator.hpp"
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

    Shogi::MoveList move_list;
    MoveGenerator::get_legal_moves(board, move_list);
    if (move_list.is_empty()) {
        // 投了
        return (turn == turn_to_move) ? -999999 : 999999;
    }

    if (has_tt_move) {
        auto it = std::find(move_list.begin(), move_list.end(), tt_best_move);
        if (it != move_list.end()) {
            std::rotate(move_list.begin(), it, it + 1);
        }
    }

    auto sort_start = has_tt_move ? move_list.begin() + 1 : move_list.begin();
    std::sort(sort_start, move_list.end(), [&](const Move &a, const Move &b) {
        return get_move_ordering_score(board, a) > get_move_ordering_score(board, b);
    });

    Turn next_side = (turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
    Move best_move = move_list[0];

    if (turn == Turn::SENTE) {
        int max_eval = -99999999;
        for (const Move &move : move_list) {
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
        for (const Move &move : move_list) {
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

std::optional<Shogi::Move> AIPlayer::find_mate(BoardState &board, int max_depth) {
    dfpn_table_.clear();
    uint64_t node_count = 0;
    uint64_t max_nodes = 100000;

    int pn = 1;
    int dn = 1;

    dfpn_search(board, board.get_turn_to_move(), INFINITY_PN, INFINITY_PN, pn, dn, max_depth, node_count, max_nodes);

    if (pn == 0) {
        Shogi::MoveList move_list;
        generate_check_moves(board, move_list);
        for (const auto &move : move_list) {
            Shogi::UndoInfo undo = board.apply_move(move);
            uint64_t hash = board.get_zobrist_hash();

            int child_pn = 1;
            if (dfpn_table_.count(hash)) {
                child_pn = dfpn_table_[hash].pn;
            }

            board.undo_move(undo);

            if (child_pn == 0) {
                return move;
            }
        }
    }

    return std::nullopt;
}

void AIPlayer::generate_check_moves(BoardState &board, Shogi::MoveList &move_list) {
    move_list.clear();

    Shogi::MoveList candidates;
    MoveGenerator::get_legal_moves(board, candidates);
    Turn enemy_turn = (board.get_turn_to_move() == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
    for (const auto &move : candidates) {
        Shogi::UndoInfo undo = board.apply_move(move);
        if (MoveGenerator::is_king_in_check(board, enemy_turn)) {
            move_list.push(move);
        }

        board.undo_move(undo);
    }
}

void AIPlayer::dfpn_search(BoardState &board, Turn turn, int threshold_pn, int threshold_dn, int &pn, int &dn,
                           int depth, uint64_t &node_count, const uint64_t max_nodes) {
    ++node_count;
    uint64_t hash = board.get_zobrist_hash();

    if (dfpn_table_.count(hash)) {
        const auto &entry = dfpn_table_[hash];
        if (entry.pn == 0 || entry.dn == 0 || entry.pn >= INFINITY_PN || entry.dn >= INFINITY_PN) {
            pn = entry.pn;
            dn = entry.dn;
            return;
        }

        if (entry.pn >= threshold_pn || entry.dn >= threshold_dn) {
            pn = entry.pn;
            dn = entry.dn;
            return;
        }

        pn = entry.pn;
        dn = entry.dn;
    } else {
        dn = 1;
        pn = 1;
    }

    if (node_count > max_nodes || depth <= 0) {
        return;
    }

    bool is_attacker = (board.get_turn_to_move() == turn);
    Shogi::MoveList move_list;
    if (is_attacker) {
        generate_check_moves(board, move_list);
    } else {
        MoveGenerator::get_legal_moves(board, move_list);
    }

    if (move_list.is_empty()) {
        pn = is_attacker ? INFINITY_PN : 0;
        dn = is_attacker ? 0 : INFINITY_PN;
        dfpn_table_[hash] = {hash, (uint32_t)pn, (uint32_t)dn};
        return;
    }

    std::vector<ChildNode> children;
    children.reserve(move_list.size());

    for (const auto &move : move_list) {
        Shogi::UndoInfo undo = board.apply_move(move);
        uint64_t child_hash = board.get_zobrist_hash();
        int child_pn = 1;
        int child_dn = 1;

        if (dfpn_table_.count(child_hash)) {
            child_pn = dfpn_table_[child_hash].pn;
            child_dn = dfpn_table_[child_hash].dn;
        }

        children.push_back({move, child_hash, child_pn, child_dn});
        board.undo_move(undo);

        if (is_attacker) {
            if (child_pn == 0) {
                pn = 0;
                dn = INFINITY_PN;
                dfpn_table_[hash] = {hash, (uint32_t)pn, (uint32_t)dn};
                return;
            }
        } else {
            if (child_pn >= INFINITY_PN) {
                pn = INFINITY_PN;
                dn = 0;
                dfpn_table_[hash] = {hash, (uint32_t)pn, (uint32_t)dn};
                return;
            }
        }
    }

    while (true) {
        int min_pn = INFINITY_PN;
        int min_dn = INFINITY_PN;
        long long sum_pn = 0;
        long long sum_dn = 0;

        int best_index = -1;
        int second_best_pn = INFINITY_PN;
        int second_best_dn = INFINITY_PN;

        for (int i = 0; i < children.size(); ++i) {
            const auto &child = children[i];
            if (is_attacker) {
                sum_dn += child.dn;
                if (child.pn < min_pn) {
                    second_best_pn = min_pn;
                    min_pn = child.pn;
                    best_index = i;
                } else if (child.pn < second_best_pn) {
                    second_best_pn = child.pn;
                }
            } else {
                sum_pn += child.pn;
                if (child.dn < min_dn) {
                    second_best_dn = min_dn;
                    min_dn = child.dn;
                    best_index = i;
                } else if (child.dn < second_best_dn) {
                    second_best_dn = child.dn;
                }
            }
        }

        if (sum_pn > INFINITY_PN) {
            sum_pn = INFINITY_PN;
        }
        if (sum_dn > INFINITY_PN) {
            sum_dn = INFINITY_PN;
        }

        if (is_attacker) {
            pn = min_pn;
            dn = (int)sum_dn;
        } else {
            pn = (int)sum_pn;
            dn = min_dn;
        }

        if (pn >= threshold_pn || dn >= threshold_dn || pn == 0 || dn == 0) {
            break;
        }
        if (node_count > max_nodes) {
            break;
        }

        ChildNode &best_child = children[best_index];
        int next_threshold_pn;
        int next_threshold_dn;

        if (is_attacker) {
            next_threshold_pn = std::min(threshold_pn, second_best_pn + 1);
            long long dn_others = sum_dn - best_child.dn;
            if (threshold_dn >= INFINITY_PN) {
                next_threshold_dn = INFINITY_PN;
            } else {
                next_threshold_dn = threshold_dn - (int)dn_others;
                if (next_threshold_dn < 0) {
                    next_threshold_dn = 0;
                }
            }
        } else {
            long long pn_others = sum_pn - best_child.pn;
            if (threshold_pn >= INFINITY_PN) {
                next_threshold_pn = INFINITY_PN;
            } else {
                next_threshold_pn = threshold_pn - (int)pn_others;
                if (next_threshold_pn < 0) {
                    next_threshold_pn = 0;
                }
            }

            next_threshold_dn = std::min(threshold_dn, second_best_dn + 1);
        }

        Shogi::UndoInfo undo = board.apply_move(best_child.move);

        dfpn_search(board, turn, next_threshold_pn, next_threshold_dn, best_child.pn, best_child.dn, depth - 1,
                    node_count, max_nodes);

        board.undo_move(undo);
    }

    dfpn_table_[hash] = {hash, (uint32_t)pn, (uint32_t)dn};
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

        Shogi::MoveList move_list;
        MoveGenerator::get_legal_moves(board, move_list, true);
        std::sort(move_list.begin(), move_list.end(), [&](const Move &a, const Move &b) {
            return get_move_ordering_score(board, a) > get_move_ordering_score(board, b);
        });

        int max_eval = stand_pat;

        for (const auto &move : move_list) {
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

        Shogi::MoveList move_list;
        MoveGenerator::get_legal_moves(board, move_list, true);
        std::sort(move_list.begin(), move_list.end(), [&](const Move &a, const Move &b) {
            return get_move_ordering_score(board, a) > get_move_ordering_score(board, b);
        });

        int min_eval = stand_pat;

        for (const auto &move : move_list) {
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

    auto mate_move = find_mate(board, 21);
    if (mate_move.has_value()) {
        UtilityFunctions::print("Checkmate proven.");

        const auto &move = mate_move.value();
        Dictionary result;
        result["from_col"] = move.from_col;
        result["from_row"] = move.from_row;
        result["to_col"] = move.to_col;
        result["to_row"] = move.to_row;
        result["piece_type"] = static_cast<int>(move.piece_type);
        result["is_promotion"] = move.is_promotion;
        result["is_drop"] = move.is_drop;
        result["win_rate"] = 1.0;
        return result;
    }

    Shogi::MoveList move_list;
    MoveGenerator::get_legal_moves(board, move_list);

    if (move_list.is_empty()) {
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

    Move global_best_move = move_list[0];
    int global_best_score = (root_side == Turn::SENTE) ? -99999999 : 99999999;

    // TTから最善手を取得
    uint64_t root_hash = board.get_zobrist_hash();
    TTEntry *root_tt = probe_tt(root_hash);
    Move best_move_prev_iter = (root_tt != nullptr) ? root_tt->best_move : move_list[0];
    bool has_prev_best = (root_tt != nullptr);

    uint64_t total_node_count = 0;

    for (int depth = 1; depth <= max_depth_limit; ++depth) {
        if (depth > 1 && Time::get_singleton()->get_ticks_usec() > strict_limit_time) {
            UtilityFunctions::print("Time limit reached before depth ", depth);
            break;
        }

        std::sort(move_list.begin(), move_list.end(), [&](const Move &a, const Move &b) {
            return get_move_ordering_score(board, a) > get_move_ordering_score(board, b);
        });

        if (has_prev_best) {
            auto it = std::find(move_list.begin(), move_list.end(), best_move_prev_iter);
            if (it != move_list.end()) {
                std::rotate(move_list.begin(), it, it + 1);
            }
        }

        int alpha = -99999999;
        int beta = 99999999;
        Move current_depth_best_move = move_list[0];
        int current_depth_best_score = (root_side == Turn::SENTE) ? -99999999 : 99999999;
        Turn next_turn_side = (root_side == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;

        uint64_t search_cutoff_time = (depth == 1) ? UINT64_MAX : strict_limit_time;
        bool timeout = false;

        for (const auto &move : move_list) {
            if (depth > 1 && Time::get_singleton()->get_ticks_usec() > strict_limit_time) {
                timeout = true;
                break;
            }

            Shogi::UndoInfo undo = board.apply_move(move);

            bool is_mated = false;
            if (find_mate(board, 5).has_value()) {
                is_mated = true;
            }

            int score;
            if (is_mated) {
                score = (root_side == Turn::SENTE) ? -999999 : 999999;
            } else {
                score = alpha_beta(board, depth - 1, alpha, beta, next_turn_side, search_cutoff_time, timeout,
                                   total_node_count);
            }

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
