#include "shogi_engine.hpp"
#include "ai_player.hpp"
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

ShogiEngine::ShogiEngine() {
    std::srand(Time::get_singleton()->get_ticks_usec());

    load_book();
}

void ShogiEngine::_bind_methods() {
    ClassDB::bind_static_method("ShogiEngine",
                                D_METHOD("is_legal_move", "main_node", "piece_obj", "target_col", "target_row"),
                                &ShogiEngine::is_legal_move);
    ClassDB::bind_static_method("ShogiEngine",
                                D_METHOD("is_legal_drop", "main_node", "piece_obj", "target_col", "target_row"),
                                &ShogiEngine::is_legal_drop);
    ClassDB::bind_static_method("ShogiEngine", D_METHOD("get_legal_moves", "main_node", "piece_obj"),
                                &ShogiEngine::get_legal_moves);
    ClassDB::bind_static_method("ShogiEngine", D_METHOD("get_legal_drops", "main_node", "piece_obj"),
                                &ShogiEngine::get_legal_drops);
    ClassDB::bind_static_method(
        "ShogiEngine", D_METHOD("is_king_safe_after_move", "main_node", "piece_obj", "target_col", "target_row"),
        &ShogiEngine::is_king_safe_after_move);
    ClassDB::bind_static_method("ShogiEngine", D_METHOD("is_king_in_check", "main_node", "is_enemy"),
                                &ShogiEngine::is_king_in_check);

    ClassDB::bind_method(D_METHOD("update_state", "main_node"), &ShogiEngine::update_state);
    ClassDB::bind_method(D_METHOD("search_best_move"), &ShogiEngine::search_best_move);

    ClassDB::bind_method(D_METHOD("set_is_enemy_side", "is_enemy"), &ShogiEngine::set_is_enemy_side);
    ClassDB::bind_method(D_METHOD("get_is_enemy_side"), &ShogiEngine::get_is_enemy_side);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_enemy_side"), "set_is_enemy_side", "get_is_enemy_side");
}

void ShogiEngine::set_is_enemy_side(bool is_enemy) { is_enemy_side = is_enemy; }

bool ShogiEngine::get_is_enemy_side() const { return is_enemy_side; }

void ShogiEngine::load_book() {
    BoardState board;
    setup_standard_position(board);
    uint64_t start_hash = board.get_zobrist_hash(Shogi::PLAYER);

    // 定跡を仮実装
    Shogi::Move move7g7f(2, 6, 2, 5, Shogi::PAWN, false, false, false);
    Shogi::Move move2g2f(7, 6, 7, 5, Shogi::PAWN, false, false, false);

    book[start_hash].push_back(move7g7f);
    book[start_hash].push_back(move2g2f);

    BoardState board_after_7g7f = board;
    board_after_7g7f.apply_move(move7g7f, Shogi::PLAYER);

    uint64_t hash_after_7g7f = board_after_7g7f.get_zobrist_hash(Shogi::ENEMY);

    Shogi::Move move_3c3d(6, 2, 6, 3, Shogi::PAWN, false, false, false);
    Shogi::Move move_8c8d(1, 2, 1, 3, Shogi::PAWN, false, false, false);

    book[hash_after_7g7f].push_back(move_3c3d);
    book[hash_after_7g7f].push_back(move_8c8d);
}

void ShogiEngine::setup_standard_position(BoardState &board) {
    for (int col = 0; col < 9; ++col) {
        board.set_cell(col, 6, Shogi::PAWN, Shogi::PLAYER, false);
        board.set_cell(col, 2, Shogi::PAWN, Shogi::ENEMY, false);
    }

    board.set_cell(1, 7, Shogi::BISHOP, Shogi::PLAYER, false);
    board.set_cell(7, 7, Shogi::ROOK, Shogi::PLAYER, false);
    board.set_cell(7, 1, Shogi::BISHOP, Shogi::ENEMY, false);
    board.set_cell(1, 1, Shogi::ROOK, Shogi::ENEMY, false);

    const int placement[] = {Shogi::LANCE, Shogi::KNIGHT, Shogi::SILVER, Shogi::GOLD, Shogi::KING,
                             Shogi::GOLD,  Shogi::SILVER, Shogi::KNIGHT, Shogi::LANCE};

    for (int col = 0; col < 9; ++col) {
        board.set_cell(8 - col, 8, placement[col], Shogi::PLAYER, false);
        board.set_cell(col, 0, placement[col], Shogi::ENEMY, false);
    }
}

bool ShogiEngine::is_legal_move(Node2D *main_node, Object *piece_obj, int target_col, int target_row) {
    if (!piece_obj) {
        return false;
    }

    BoardState board;
    board.init_from_main(main_node);

    int current_col = piece_obj->get("current_col");
    int current_row = piece_obj->get("current_row");

    return board.is_legal_move(current_col, current_row, target_col, target_row);
}

bool ShogiEngine::is_legal_drop(Node2D *main_node, Object *piece_obj, int target_col, int target_row) {
    if (!piece_obj) {
        return false;
    }

    BoardState board;
    board.init_from_main(main_node);

    int piece_type = piece_obj->get("piece_type");
    bool is_enemy = piece_obj->get("is_enemy");

    return board.is_legal_drop(piece_type, is_enemy, target_col, target_row);
}

TypedArray<Vector2i> ShogiEngine::get_legal_moves(Node2D *main_node, Object *piece_obj) {
    TypedArray<Vector2i> result;
    if (!piece_obj) {
        return result;
    }

    BoardState board;
    board.init_from_main(main_node);

    int current_col = piece_obj->get("current_col");
    int current_row = piece_obj->get("current_row");

    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            if (board.is_legal_move(current_col, current_row, col, row)) {
                result.append(Vector2i(col, row));
            }
        }
    }

    return result;
}

TypedArray<Vector2i> ShogiEngine::get_legal_drops(Node2D *main_node, Object *piece_obj) {
    TypedArray<Vector2i> result;
    if (!piece_obj) {
        return result;
    }

    BoardState board;
    board.init_from_main(main_node);

    int piece_type = piece_obj->get("piece_type");
    bool is_enemy = piece_obj->get("is_enemy");

    for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
        for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
            if (board.is_legal_drop(piece_type, is_enemy, col, row)) {
                result.append(Vector2i(col, row));
            }
        }
    }

    return result;
}

bool ShogiEngine::is_king_safe_after_move(Node2D *main_node, Object *piece_obj, int target_col, int target_row) {
    if (!piece_obj) {
        return false;
    }

    BoardState board;
    board.init_from_main(main_node);

    int piece_type = piece_obj->get("piece_type");
    int current_col = piece_obj->get("current_col");
    int current_row = piece_obj->get("current_row");
    bool is_enemy = piece_obj->get("is_enemy");
    bool is_promoted = piece_obj->get("is_promoted");

    int side = is_enemy ? Shogi::ENEMY : Shogi::PLAYER;

    if (current_col != -1 && current_row != -1) {
        board.clear_cell(current_col, current_row);
    }

    board.set_cell(target_col, target_row, piece_type, side, is_promoted);

    return !board.is_king_in_check(side);
}

bool ShogiEngine::is_king_in_check(Node2D *main_node, bool is_enemy) {
    BoardState board;
    board.init_from_main(main_node);

    int side = is_enemy ? Shogi::ENEMY : Shogi::PLAYER;

    return board.is_king_in_check(side);
}

void ShogiEngine::update_state(Node2D *main_node) {
    current_state = BoardState();
    current_state.init_from_main(main_node);
}

Dictionary ShogiEngine::search_best_move() {
    int side = is_enemy_side ? Shogi::ENEMY : Shogi::PLAYER;

    // 定跡を参照
    uint64_t hash = current_state.get_zobrist_hash(side);
    if (book.count(hash) > 0) {
        const std::vector<Shogi::Move> &book_moves = book[hash];
        if (!book_moves.empty()) {
            int idx = std::rand() % book_moves.size();
            Shogi::Move best_move = book_moves[idx];

            UtilityFunctions::print("Using Book Move. Hash: ", String::num_uint64(hash));

            Dictionary result;
            result["from_col"] = best_move.from_col;
            result["from_row"] = best_move.from_row;
            result["to_col"] = best_move.to_col;
            result["to_row"] = best_move.to_row;
            result["piece_type"] = best_move.piece_type;
            result["is_promotion"] = best_move.is_promotion;
            result["is_drop"] = best_move.is_drop;
            result["win_rate"] = 0.5;
            return result;
        }
    }

    AIPlayer ai_player(is_enemy_side);
    return ai_player.search_best_move(current_state);
}
