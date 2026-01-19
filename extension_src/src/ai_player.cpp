#include "ai_player.hpp"
#include <algorithm>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <vector>

using namespace godot;
using Shogi::PieceType;
using Shogi::Turn;

namespace {

constexpr std::array<std::array<int, 2>, Shogi::PIECE_TYPE_COUNT> PIECE_VALUES = {{
    {99999, 99999}, // KING
    {640, 950},     // ROOK
    {570, 830},     // BISHOP
    {440, 440},     // GOLD
    {370, 500},     // SILVER
    {260, 510},     // KNIGHT
    {230, 490},     // LANCE
    {90, 530},      // PAWN
}};

constexpr std::array HAND_PIECE_TYPES = {
    PieceType::PAWN, PieceType::LANCE,  PieceType::KNIGHT, PieceType::SILVER,
    PieceType::GOLD, PieceType::BISHOP, PieceType::ROOK,
};

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

constexpr const int (*PST_TABLES[Shogi::PIECE_TYPE_COUNT])[9] = {
    PST_KING,   // KING
    PST_ROOK,   // ROOK
    PST_BISHOP, // BISHOP
    PST_GOLD,   // GOLD
    PST_SILVER, // SILVER
    nullptr,    // KNIGHT
    nullptr,    // LANCE
    PST_PAWN,   // PAWN
};

} // namespace

std::vector<Shogi::Move> AIPlayer::get_legal_moves(const BoardState &board, Turn turn, bool only_captures) {
    std::vector<Shogi::Move> moves;
    bool is_enemy_turn = (turn == Turn::GOTE);

    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            Shogi::Coord from{col, row};
            const Cell &cell = board.get_cell(from);

            // 自駒でないならスキップ
            if (cell.is_empty() || cell.turn != turn) {
                continue;
            }

            for (int t_col = 0; t_col < Shogi::BOARD_COLS; ++t_col) {
                for (int t_row = 0; t_row < Shogi::BOARD_ROWS; ++t_row) {
                    Shogi::Coord to{t_col, t_row};
                    if (board.is_legal_move(from, to)) {
                        bool is_capture = !board.get_cell(to).is_empty();
                        if (only_captures && !is_capture) {
                            continue;
                        }

                        bool can_promote = false;
                        bool must_promote = false;

                        if (!cell.is_promoted && cell.type != PieceType::KING && cell.type != PieceType::GOLD) {
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

    if (!only_captures) {
        for (PieceType piece_type : HAND_PIECE_TYPES) {
            if (board.get_hand_count(turn, piece_type) > 0) {
                for (int t_col = 0; t_col < Shogi::BOARD_COLS; ++t_col) {
                    for (int t_row = 0; t_row < Shogi::BOARD_ROWS; ++t_row) {
                        Shogi::Coord to{t_col, t_row};
                        if (board.is_legal_drop(piece_type, is_enemy_turn, to)) {
                            moves.emplace_back(0, 0, t_col, t_row, piece_type, false, true, false);
                        }
                    }
                }
            }
        }
    }

    return moves;
}

int AIPlayer::evaluate(const BoardState &board) {
    int score = 0;

    // 盤上の駒
    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            const Cell &cell = board.get_cell({col, row});
            if (cell.is_empty()) {
                continue;
            }

            int piece_value = PIECE_VALUES[static_cast<int>(cell.type)][cell.is_promoted ? 1 : 0];
            PieceType lookup_type = cell.type;
            if (cell.is_promoted) {
                switch (cell.type) {
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

            int pst_bonus = get_pst_value(lookup_type, cell.turn, {col, row});
            if (cell.turn == Turn::SENTE) {
                score += piece_value + pst_bonus;
            } else {
                score -= piece_value + pst_bonus;
            }
        }
    }

    // 持ち駒
    for (Turn turn : {Turn::SENTE, Turn::GOTE}) {
        int sign = (turn == Turn::SENTE) ? 1 : -1;
        for (PieceType piece_type : HAND_PIECE_TYPES) {
            score += board.get_hand_count(turn, piece_type) * PIECE_VALUES[static_cast<int>(piece_type)][0] * sign;
        }
    }

    return score;
}

int AIPlayer::get_pst_value(PieceType piece_type, Turn turn, Shogi::Coord coord) {
    if (!coord.is_valid() || static_cast<int>(piece_type) < 0 ||
        static_cast<int>(piece_type) >= Shogi::PIECE_TYPE_COUNT) {
        return 0;
    }

    const auto *pst = PST_TABLES[static_cast<int>(piece_type)];
    if (pst == nullptr) {
        return 0;
    }

    int row = (turn == Turn::SENTE) ? coord.row : (Shogi::BOARD_ROWS - 1 - coord.row);
    int col = (turn == Turn::SENTE) ? coord.col : (Shogi::BOARD_COLS - 1 - coord.col);
    return pst[row][col];
}

int AIPlayer::alpha_beta(BoardState board, int depth, int alpha, int beta, Turn turn, uint64_t end_time, bool &timeout,
                         uint64_t &node_count) {
    ++node_count;

    if (Time::get_singleton()->get_ticks_usec() > end_time) {
        timeout = true;
        return 0;
    }

    if (depth <= 0) {
        // 静止探索を実行
        return quiescence_search(board, alpha, beta, turn, node_count);
    }

    Turn turn_to_move = board.get_turn_to_move();
    std::vector<Shogi::Move> moves = get_legal_moves(board, turn);
    if (moves.empty()) {
        // 投了
        return (turn == turn_to_move) ? -999999 : 999999;
    }

    // 取る手を優先
    std::sort(moves.begin(), moves.end(),
              [](const Shogi::Move &a, const Shogi::Move &b) { return a.is_capture > b.is_capture; });

    Turn next_side = (turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;

    if (turn == Turn::SENTE) {
        int max_eval = -99999999;
        for (const Shogi::Move &move : moves) {
            BoardState next_board = board;
            next_board.apply_move(move);
            int eval = alpha_beta(next_board, depth - 1, alpha, beta, next_side, end_time, timeout, node_count);
            if (timeout) {
                return 0;
            }

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
            next_board.apply_move(move);
            int eval = alpha_beta(next_board, depth - 1, alpha, beta, next_side, end_time, timeout, node_count);
            if (timeout) {
                return 0;
            }

            min_eval = std::min(min_eval, eval);
            beta = std::min(beta, eval);
            if (beta <= alpha) {
                break; // αカット
            }
        }

        return min_eval;
    }
}

int AIPlayer::quiescence_search(BoardState board, int alpha, int beta, Turn turn, uint64_t &node_count) {
    ++node_count;

    int stand_pat = evaluate(board);

    if (turn == Turn::SENTE) {
        if (stand_pat >= beta) {
            return beta;
        }

        if (stand_pat > alpha) {
            alpha = stand_pat;
        }

        std::vector<Shogi::Move> moves = get_legal_moves(board, turn, true);

        std::sort(moves.begin(), moves.end(),
                  [](const Shogi::Move &a, const Shogi::Move &b) { return a.is_capture > b.is_capture; });

        int max_eval = stand_pat;

        for (const auto &move : moves) {
            BoardState next_board = board;
            next_board.apply_move(move);

            int eval = quiescence_search(next_board, alpha, beta, Turn::GOTE, node_count);

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

        std::vector<Shogi::Move> moves = get_legal_moves(board, turn, true);
        std::sort(moves.begin(), moves.end(),
                  [](const Shogi::Move &a, const Shogi::Move &b) { return a.is_capture > b.is_capture; });
        int min_eval = stand_pat;

        for (const auto &move : moves) {
            BoardState next_board = board;
            next_board.apply_move(move);

            int eval = quiescence_search(next_board, alpha, beta, Turn::SENTE, node_count);

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

Dictionary AIPlayer::search_best_move(BoardState board) {
    Turn root_side = board.get_turn_to_move();
    std::vector<Shogi::Move> moves = get_legal_moves(board, root_side);

    if (moves.empty()) {
        // 投了
        Dictionary result;
        result["win_rate"] = 0.0;
        return result;
    }

    uint64_t start_time = Time::get_singleton()->get_ticks_usec();
    uint64_t strict_limit_time = start_time + TIME_LIMIT_USEC;

    int max_depth_limit = 10;

    Shogi::Move global_best_move = moves[0];
    int global_best_score = (root_side == Turn::SENTE) ? -99999999 : 99999999;

    Shogi::Move best_move_prev_iter = moves[0];
    bool has_prev_best = false;
    uint64_t total_node_count = 0;

    for (int depth = 1; depth <= max_depth_limit; ++depth) {
        if (depth > 1 && Time::get_singleton()->get_ticks_usec() > strict_limit_time) {
            UtilityFunctions::print("Time limit reached before depth ", depth);
            break;
        }

        if (has_prev_best) {
            auto it = std::find(moves.begin(), moves.end(), best_move_prev_iter);
            if (it != moves.end()) {
                std::rotate(moves.begin(), it, it + 1);
            }
        } else {
            std::sort(moves.begin(), moves.end(),
                      [](const Shogi::Move &a, const Shogi::Move &b) { return a.is_capture > b.is_capture; });
        }

        int alpha = -99999999;
        int beta = 99999999;
        Shogi::Move current_depth_best_move = moves[0];
        int current_depth_best_score = (root_side == Turn::SENTE) ? -99999999 : 99999999;
        Turn next_turn_side = (root_side == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;

        uint64_t search_cutoff_time = (depth == 1) ? UINT64_MAX : strict_limit_time;
        bool timeout = false;

        for (const auto &move : moves) {
            if (depth > 1 && Time::get_singleton()->get_ticks_usec() > strict_limit_time) {
                timeout = true;
                break;
            }

            BoardState next_board = board;
            next_board.apply_move(move);

            int score = alpha_beta(next_board, depth - 1, alpha, beta, next_turn_side, search_cutoff_time, timeout,
                                   total_node_count);
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

    UtilityFunctions::print("Total nodes searched: ", total_node_count);

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
