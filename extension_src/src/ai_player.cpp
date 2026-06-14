#include "ai_player.hpp"
#include "move_generator.hpp"
#include <algorithm>
#include <cstring>
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

constexpr int MATE_SCORE = 999999;
constexpr int MATE_BOUND = MATE_SCORE - 10000;

// 不詰みの証明はノード数上限まで探索しがちなため、思考時間に収まるよう上限を用途別に分ける
constexpr int OWN_MATE_DEPTH = 21;
constexpr uint64_t OWN_MATE_NODES = 30000;
constexpr int MATE_CHECK_DEPTH = 9;
constexpr uint64_t MATE_CHECK_NODES = 5000;

// 後方の駒を取らない手は最善である可能性が低いため、先頭の手を除いて探索深さを削減する
constexpr int LMR_MOVE_THRESHOLD = 4;
constexpr int LMR_DEEP_MOVE_THRESHOLD = 12;

// Delta Pruningのマージン
constexpr int DELTA_MARGIN = 540;

// 静止探索を打ち切る深さ
constexpr int QS_PLY_LIMIT = 16;

// 詰みの評価値はルートからの手数を含むため、置換表にはその局面からの手数に変換して格納して取得時に戻す
int score_to_tt(int score, int ply) {
    if (score > MATE_BOUND) {
        return score + ply;
    }
    if (score < -MATE_BOUND) {
        return score - ply;
    }
    return score;
}

int score_from_tt(int score, int ply) {
    if (score > MATE_BOUND) {
        return score - ply;
    }
    if (score < -MATE_BOUND) {
        return score + ply;
    }
    return score;
}

} // namespace

void AIPlayer::set_game_history(const std::vector<uint64_t> &hashes, const std::vector<bool> &in_checks) {
    // 境界外参照防止のため短い方に合わせる
    size_t n = hashes.size() < in_checks.size() ? hashes.size() : in_checks.size();
    game_history_hashes_.assign(hashes.begin(), hashes.begin() + n);
    game_history_in_check_.assign(in_checks.begin(), in_checks.begin() + n);
    history_len_ = static_cast<int>(n);
    game_history_hash_set_.clear();
    game_history_hash_set_.insert(game_history_hashes_.begin(), game_history_hashes_.end());
}

std::optional<int> AIPlayer::detect_path_repetition(int ply, uint64_t hash, Shogi::Turn stm) {
    int v = history_len_ + ply;
    for (int p = v - 2; p >= 0; p -= 2) {
        if (p < history_len_ && game_history_hash_set_.find(hash) == game_history_hash_set_.end()) {
            break; // 現局面が履歴に無ければ無駄な走査を打ち切る
        }
        if (hash_at(p) != hash) {
            continue;
        }

        bool stm_perpetual = true; // 手番側が連続王手
        for (int q = p + 1; q <= v - 1; q += 2) {
            if (!in_check_at(q)) {
                stm_perpetual = false;
                break;
            }
        }
        bool opp_perpetual = true; // 相手が連続王手
        for (int q = p + 2; q <= v; q += 2) {
            if (!in_check_at(q)) {
                opp_perpetual = false;
                break;
            }
        }

        int mate = MATE_SCORE - ply;
        if (stm_perpetual) {
            return (stm == Turn::SENTE) ? -mate : mate; // 連続王手の千日手は王手側の負け
        }
        if (opp_perpetual) {
            return (stm == Turn::SENTE) ? mate : -mate;
        }
        return 0;
    }
    return std::nullopt;
}

int AIPlayer::get_move_ordering_score(const BoardState &board, const Shogi::Move &move, int ply) {
    int score = 0;

    if (move.is_capture) {
        // 駒を取る手：MVV-LVA
        const Cell &target_cell = board.get_cell({move.to_col, move.to_row});
        if (!target_cell.is_empty()) {
            int victim_value = Shogi::PIECE_VALUES[static_cast<int>(target_cell.type)][target_cell.is_promoted ? 1 : 0];
            int aggressor_value = Shogi::PIECE_VALUES[static_cast<int>(move.piece_type)][0];
            // 高い駒を安い駒で取るほど高得点
            score = 1000000 + victim_value - aggressor_value;
        }
    } else {
        // 駒を取らない手：Killer / History
        bool is_killer = false;
        if (ply < MAX_PLY) {
            if (killer_valid_[ply][0] && move == killer_moves_[ply][0]) {
                score = 900000;
                is_killer = true;
            } else if (killer_valid_[ply][1] && move == killer_moves_[ply][1]) {
                score = 800000;
                is_killer = true;
            }
        }
        if (!is_killer) {
            int side = static_cast<int>(board.get_turn_to_move());
            int to_sq = move.to_col * Shogi::BOARD_ROWS + move.to_row;
            score = history_[side][static_cast<int>(move.piece_type)][to_sq];
        }
    }

    // 成る手
    if (move.is_promotion) {
        score += 20000;
    }

    return score;
}

void AIPlayer::update_killer(int ply, const Shogi::Move &move) {
    if (ply >= MAX_PLY) {
        return;
    }
    if (killer_valid_[ply][0] && killer_moves_[ply][0] == move) {
        return;
    }
    killer_moves_[ply][1] = killer_moves_[ply][0];
    killer_valid_[ply][1] = killer_valid_[ply][0];
    killer_moves_[ply][0] = move;
    killer_valid_[ply][0] = true;
}

void AIPlayer::update_history(Shogi::Turn turn, const Shogi::Move &move, int depth) {
    int side = static_cast<int>(turn);
    int to_sq = move.to_col * Shogi::BOARD_ROWS + move.to_row;
    int &value = history_[side][static_cast<int>(move.piece_type)][to_sq];
    value += depth * depth;
    if (value > HISTORY_CAP) {
        value = HISTORY_CAP;
    }
}

int AIPlayer::alpha_beta(BoardState &board, int depth, int ply, int alpha, int beta, Turn turn, uint64_t end_time,
                         bool &timeout, uint64_t &node_count, bool can_null) {
    ++node_count;

    if (Time::get_singleton()->get_ticks_usec() > end_time) {
        timeout = true;
        return 0;
    }

    uint64_t hash = board.get_zobrist_hash();
    Turn turn_to_move = board.get_turn_to_move();
    bool in_check = MoveGenerator::is_king_in_check(board, turn_to_move);

    // 千日手（経路反復）：TTより前に判定
    if (ply < MAX_PLY) {
        path_in_check_[ply] = in_check;
        if (auto rep = detect_path_repetition(ply, hash, turn_to_move); rep.has_value()) {
            return rep.value();
        }
        path_hashes_[ply] = hash;
    }

    int original_alpha = alpha;
    int original_beta = beta;
    Move tt_best_move{};
    bool has_tt_move = false;

    // 置換表の上限と下限で探索窓を狭めると保存時のフラグ分類が不正確になるため、カットのみに使う
    TTEntry *tt_entry = probe_tt(hash);
    if (tt_entry != nullptr && tt_entry->depth >= depth) {
        int tt_score = score_from_tt(tt_entry->score, ply);
        if (tt_entry->flag == TTFlag::EXACT) {
            return tt_score;
        } else if (tt_entry->flag == TTFlag::LOWER_BOUND && tt_score >= beta) {
            return tt_score;
        } else if (tt_entry->flag == TTFlag::UPPER_BOUND && tt_score <= alpha) {
            return tt_score;
        }
    }

    // TTから最善手を取得
    if (tt_entry != nullptr) {
        tt_best_move = tt_entry->best_move;
        has_tt_move = true;
    }

    if (depth <= 0) {
        // 静止探索を実行
        return quiescence_search(board, alpha, beta, turn, ply, node_count);
    }

    // 王手延長：王手を受けた側は深さを減らさず読む
    const int extension = (in_check && ply < MAX_PLY) ? 1 : 0;

    Shogi::MoveList move_list;
    MoveGenerator::get_legal_moves(board, move_list);
    if (move_list.is_empty()) {
        // 投了（手番側の負け）
        return (turn_to_move == Turn::SENTE) ? -(MATE_SCORE - ply) : (MATE_SCORE - ply);
    }

    // Null Move Pruning
    constexpr int NULL_MOVE_R = 2;
    if (can_null && depth >= 3 && !in_check) {
        Turn null_next_side = (turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
        int static_eval = board.get_score();

        if (turn == Turn::SENTE ? (static_eval >= beta) : (static_eval <= alpha)) {
            int null_alpha = (turn == Turn::SENTE) ? beta - 1 : alpha;
            int null_beta = (turn == Turn::SENTE) ? beta : alpha + 1;

            uint64_t null_hash = board.make_null_move();
            int null_score = alpha_beta(board, depth - 1 - NULL_MOVE_R, ply + 1, null_alpha, null_beta, null_next_side,
                                        end_time, timeout, node_count, false);
            board.undo_null_move(null_hash);

            if (timeout) {
                return 0;
            }
            if (turn == Turn::SENTE ? (null_score >= beta) : (null_score <= alpha)) {
                return (turn == Turn::SENTE) ? beta : alpha;
            }
        }
    }

    if (has_tt_move) {
        auto it = std::find(move_list.begin(), move_list.end(), tt_best_move);
        if (it != move_list.end()) {
            std::rotate(move_list.begin(), it, it + 1);
        }
    }

    auto sort_start = has_tt_move ? move_list.begin() + 1 : move_list.begin();
    std::sort(sort_start, move_list.end(), [&](const Move &a, const Move &b) {
        return get_move_ordering_score(board, a, ply) > get_move_ordering_score(board, b, ply);
    });

    auto is_killer_move = [&](const Move &m) {
        return ply < MAX_PLY && ((killer_valid_[ply][0] && killer_moves_[ply][0] == m) ||
                                 (killer_valid_[ply][1] && killer_moves_[ply][1] == m));
    };

    Turn next_side = (turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
    Move best_move = move_list[0];

    if (turn == Turn::SENTE) {
        int max_eval = -99999999;
        int move_index = 0;
        for (const Move &move : move_list) {
            // Late Move Reductions
            int reduction = 0;
            if (depth >= 3 && move_index >= LMR_MOVE_THRESHOLD && !in_check && !move.is_capture && !move.is_promotion &&
                !is_killer_move(move)) {
                reduction = (depth >= 6 && move_index >= LMR_DEEP_MOVE_THRESHOLD) ? 2 : 1;
            }
            ++move_index;

            Shogi::UndoInfo undo = board.apply_move(move);

            // 王手をかける手は詰み筋の見逃しを防ぐため削減しない
            if (reduction > 0 && MoveGenerator::is_king_in_check(board, next_side)) {
                reduction = 0;
            }

            int eval = alpha_beta(board, depth - 1 + extension - reduction, ply + 1, alpha, beta, next_side, end_time,
                                  timeout, node_count);

            if (!timeout && reduction > 0 && eval > alpha) {
                eval = alpha_beta(board, depth - 1 + extension, ply + 1, alpha, beta, next_side, end_time, timeout,
                                  node_count);
            }

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
                if (!move.is_capture) {
                    update_killer(ply, move);
                    update_history(turn_to_move, move, depth);
                }
                break; // βカット
            }
        }

        TTFlag flag;
        if (max_eval <= original_alpha) {
            flag = TTFlag::UPPER_BOUND;
        } else if (max_eval >= original_beta) {
            flag = TTFlag::LOWER_BOUND;
        } else {
            flag = TTFlag::EXACT;
        }
        store_tt(hash, score_to_tt(max_eval, ply), depth, flag, best_move);

        return max_eval;
    } else {
        int min_eval = 99999999;
        int move_index = 0;
        for (const Move &move : move_list) {
            // Late Move Reductions
            int reduction = 0;
            if (depth >= 3 && move_index >= LMR_MOVE_THRESHOLD && !in_check && !move.is_capture && !move.is_promotion &&
                !is_killer_move(move)) {
                reduction = (depth >= 6 && move_index >= LMR_DEEP_MOVE_THRESHOLD) ? 2 : 1;
            }
            ++move_index;

            Shogi::UndoInfo undo = board.apply_move(move);

            // 王手をかける手は詰み筋の見逃しを防ぐため削減しない
            if (reduction > 0 && MoveGenerator::is_king_in_check(board, next_side)) {
                reduction = 0;
            }

            int eval = alpha_beta(board, depth - 1 + extension - reduction, ply + 1, alpha, beta, next_side, end_time,
                                  timeout, node_count);

            if (!timeout && reduction > 0 && eval < beta) {
                eval = alpha_beta(board, depth - 1 + extension, ply + 1, alpha, beta, next_side, end_time, timeout,
                                  node_count);
            }

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
                if (!move.is_capture) {
                    update_killer(ply, move);
                    update_history(turn_to_move, move, depth);
                }
                break; // αカット
            }
        }

        TTFlag flag;
        if (min_eval >= original_beta) {
            flag = TTFlag::LOWER_BOUND;
        } else if (min_eval <= original_alpha) {
            flag = TTFlag::UPPER_BOUND;
        } else {
            flag = TTFlag::EXACT;
        }
        store_tt(hash, score_to_tt(min_eval, ply), depth, flag, best_move);

        return min_eval;
    }
}

std::optional<Shogi::Move> AIPlayer::find_mate(BoardState &board, int max_depth, uint64_t max_nodes) {
    dfpn_table_.clear();
    uint64_t node_count = 0;

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

    if (node_count > max_nodes) {
        return;
    }

    // pn/dnが進まないと親が同じ子を選び続けるため、深さ制限に達したノードは不詰み扱いで返す
    if (depth <= 0) {
        pn = INFINITY_PN;
        dn = 0;
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

int AIPlayer::quiescence_search(BoardState &board, int alpha, int beta, Turn turn, int ply, uint64_t &node_count,
                                int qs_ply) {
    ++node_count;

    bool in_check = MoveGenerator::is_king_in_check(board, turn);

    // 千日手（経路反復）：Stand-Patより前に判定
    if (ply < MAX_PLY) {
        uint64_t hash = board.get_zobrist_hash();
        path_in_check_[ply] = in_check;
        if (auto rep = detect_path_repetition(ply, hash, turn); rep.has_value()) {
            return rep.value();
        }
        path_hashes_[ply] = hash;
    }

    int stand_pat = board.get_score();

    // 取り合い連鎖が長すぎる場合は打ち切り
    if (qs_ply >= QS_PLY_LIMIT) {
        return stand_pat;
    }

    if (turn == Turn::SENTE) {
        if (!in_check) {
            if (stand_pat >= beta) {
                return beta;
            }

            if (stand_pat > alpha) {
                alpha = stand_pat;
            }
        }

        Shogi::MoveList move_list;
        MoveGenerator::get_legal_moves(board, move_list, !in_check);
        if (in_check && move_list.is_empty()) {
            // 詰み（手番側の負け）
            return -(MATE_SCORE - ply);
        }
        std::sort(move_list.begin(), move_list.end(), [&](const Move &a, const Move &b) {
            return get_move_ordering_score(board, a, ply) > get_move_ordering_score(board, b, ply);
        });

        int max_eval = in_check ? -99999999 : stand_pat;

        for (const auto &move : move_list) {
            if (!in_check) {
                // Delta Pruning
                const Cell &victim = board.get_cell({move.to_col, move.to_row});
                int victim_value = Shogi::PIECE_VALUES[static_cast<int>(victim.type)][victim.is_promoted ? 1 : 0];
                int promotion_gain = 0;
                if (move.is_promotion) {
                    int pt = static_cast<int>(move.piece_type);
                    promotion_gain = Shogi::PIECE_VALUES[pt][1] - Shogi::PIECE_VALUES[pt][0];
                }
                if (stand_pat + victim_value + promotion_gain + DELTA_MARGIN <= alpha) {
                    continue;
                }
                // Static Exchange Evaluation
                if (MoveGenerator::see(board, move) < 0) {
                    continue;
                }
            }

            Shogi::UndoInfo undo = board.apply_move(move);
            int eval = quiescence_search(board, alpha, beta, Turn::GOTE, ply + 1, node_count, qs_ply + 1);
            board.undo_move(undo);

            max_eval = std::max(max_eval, eval);
            alpha = std::max(alpha, eval);

            if (alpha >= beta) {
                return beta; // βカット
            }
        }

        return max_eval;
    } else {
        if (!in_check) {
            if (stand_pat <= alpha) {
                return alpha;
            }

            if (stand_pat < beta) {
                beta = stand_pat;
            }
        }

        Shogi::MoveList move_list;
        MoveGenerator::get_legal_moves(board, move_list, !in_check);
        if (in_check && move_list.is_empty()) {
            // 詰み（手番側の負け）
            return MATE_SCORE - ply;
        }
        std::sort(move_list.begin(), move_list.end(), [&](const Move &a, const Move &b) {
            return get_move_ordering_score(board, a, ply) > get_move_ordering_score(board, b, ply);
        });

        int min_eval = in_check ? 99999999 : stand_pat;

        for (const auto &move : move_list) {
            if (!in_check) {
                // Delta Pruning
                const Cell &victim = board.get_cell({move.to_col, move.to_row});
                int victim_value = Shogi::PIECE_VALUES[static_cast<int>(victim.type)][victim.is_promoted ? 1 : 0];
                int promotion_gain = 0;
                if (move.is_promotion) {
                    int pt = static_cast<int>(move.piece_type);
                    promotion_gain = Shogi::PIECE_VALUES[pt][1] - Shogi::PIECE_VALUES[pt][0];
                }
                if (stand_pat - victim_value - promotion_gain - DELTA_MARGIN >= beta) {
                    continue;
                }
                // Static Exchange Evaluation
                if (MoveGenerator::see(board, move) < 0) {
                    continue;
                }
            }

            Shogi::UndoInfo undo = board.apply_move(move);
            int eval = quiescence_search(board, alpha, beta, Turn::SENTE, ply + 1, node_count, qs_ply + 1);
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

    auto mate_move = find_mate(board, OWN_MATE_DEPTH, OWN_MATE_NODES);
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

    transposition_table_.reserve(TT_SIZE);

    if (transposition_table_.size() > TT_SIZE) {
        UtilityFunctions::print("TT size exceeded limit, clearing. Size was: ",
                                static_cast<int64_t>(transposition_table_.size()));
        clear_tt();
    }

    // Killerは探索ごとにクリア
    // Historyはエージングして対局内の学習を引き継ぐ
    std::memset(killer_valid_, 0, sizeof(killer_valid_));
    for (auto &side : history_) {
        for (auto &piece : side) {
            for (int &value : piece) {
                value >>= 1;
            }
        }
    }

    uint64_t start_time = Time::get_singleton()->get_ticks_usec();
    uint64_t strict_limit_time = start_time + TIME_LIMIT_USEC;

    int max_depth_limit = 12;

    Move global_best_move = move_list[0];
    int global_best_score = (root_side == Turn::SENTE) ? -99999999 : 99999999;

    // TTから最善手を取得
    uint64_t root_hash = board.get_zobrist_hash();
    TTEntry *root_tt = probe_tt(root_hash);
    Move best_move_prev_iter = (root_tt != nullptr) ? root_tt->best_move : move_list[0];
    bool has_prev_best = (root_tt != nullptr);

    uint64_t total_node_count = 0;

    // ルート局面は反復深化を通じて不変であり、候補手ごとの詰み判定結果は再利用できる
    std::unordered_map<uint64_t, bool> root_mate_cache;

    // 千日手（経路反復）：ルート局面を経路の起点に登録
    path_hashes_[0] = board.get_zobrist_hash();
    path_in_check_[0] = MoveGenerator::is_king_in_check(board, root_side);

    for (int depth = 1; depth <= max_depth_limit; ++depth) {
        if (depth > 1 && Time::get_singleton()->get_ticks_usec() > strict_limit_time) {
            UtilityFunctions::print("Time limit reached before depth ", depth);
            break;
        }

        std::sort(move_list.begin(), move_list.end(), [&](const Move &a, const Move &b) {
            return get_move_ordering_score(board, a, 0) > get_move_ordering_score(board, b, 0);
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

            int score = alpha_beta(board, depth - 1, 1, alpha, beta, next_turn_side, search_cutoff_time, timeout,
                                   total_node_count);

            // 全候補手の詰み検証は合法手の多い終盤で時間がかかりすぎるため、暫定最善を更新する手のみ検証する
            bool candidate =
                (root_side == Turn::SENTE) ? (score > current_depth_best_score) : (score < current_depth_best_score);
            if (!timeout && candidate) {
                uint64_t child_hash = board.get_zobrist_hash();
                auto mate_it = root_mate_cache.find(child_hash);
                bool is_mated;
                if (mate_it != root_mate_cache.end()) {
                    is_mated = mate_it->second;
                } else {
                    is_mated = find_mate(board, MATE_CHECK_DEPTH, MATE_CHECK_NODES).has_value();
                    root_mate_cache.emplace(child_hash, is_mated);
                }

                if (is_mated) {
                    // 詰みまでの手数が不明なため、どの詰みよりも速い扱いにする
                    score = (root_side == Turn::SENTE) ? -(MATE_SCORE - 1) : (MATE_SCORE - 1);
                }
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
        if (global_best_score >= MATE_BOUND || global_best_score <= -MATE_BOUND) {
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
