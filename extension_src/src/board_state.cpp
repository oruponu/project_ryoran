#include "board_state.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <string>
#include <vector>

using namespace godot;
using Shogi::Coord;
using Shogi::Move;
using Shogi::PieceType;
using Shogi::Turn;

namespace {

uint64_t g_zobrist_board[2][Shogi::PIECE_TYPE_COUNT][2][Shogi::BOARD_COLS][Shogi::BOARD_ROWS];
uint64_t g_zobrist_hand[2][Shogi::PIECE_TYPE_COUNT][20];
uint64_t g_zobrist_turn_enemy;
bool g_zobrist_initialized = false;

std::optional<Shogi::PieceType> piece_type_from_char(char c) {
	switch (std::toupper(static_cast<unsigned char>(c))) {
		case 'K':
			return Shogi::PieceType::KING;
		case 'R':
			return Shogi::PieceType::ROOK;
		case 'B':
			return Shogi::PieceType::BISHOP;
		case 'G':
			return Shogi::PieceType::GOLD;
		case 'S':
			return Shogi::PieceType::SILVER;
		case 'N':
			return Shogi::PieceType::KNIGHT;
		case 'L':
			return Shogi::PieceType::LANCE;
		case 'P':
			return Shogi::PieceType::PAWN;
		default:
			return std::nullopt;
	}
}

} // namespace

BoardState::BoardState(Turn turn_to_move) : turn_to_move_(turn_to_move), score_(0) {
	AttackTable::initialize();
	Evaluator::initialize();

	// 盤面を初期化
	for (int i = 0; i < Shogi::BOARD_SIZE; ++i) {
		board_[i] = Cell();
	}

	// 持ち駒を初期化
	for (Turn turn : { Turn::SENTE, Turn::GOTE }) {
		for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
			hand_[static_cast<int>(turn)][piece_type] = 0;
		}
	}

	king_pos_[static_cast<int>(Turn::SENTE)] = std::nullopt;
	king_pos_[static_cast<int>(Turn::GOTE)] = std::nullopt;
	pawn_columns_[static_cast<int>(Turn::SENTE)] = 0;
	pawn_columns_[static_cast<int>(Turn::GOTE)] = 0;

	zobrist_hash_ = calculate_zobrist_hash();

	build_bitboard();
}

BoardState::BoardState(const std::string &sfen) : BoardState(Turn::SENTE) {
	if (!parse_sfen(sfen)) {
		UtilityFunctions::push_error(("Invalid SFEN: " + sfen).c_str());

		// パースに失敗したときは空の盤面に戻す
		for (int i = 0; i < Shogi::BOARD_SIZE; ++i) {
			board_[i] = Cell();
		}
		for (int side = 0; side < 2; ++side) {
			for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
				hand_[side][piece_type] = 0;
			}
		}
		turn_to_move_ = Turn::SENTE;
	}

	update_king_position_cache();
	update_pawn_columns_cache();
	zobrist_hash_ = calculate_zobrist_hash();
	score_ = Evaluator::calculate_score(*this);

	build_bitboard();
}

bool BoardState::parse_sfen(const std::string &sfen) {
	std::vector<std::string> fields;
	std::string current;
	for (char c : sfen) {
		if (c == ' ') {
			if (!current.empty()) {
				fields.push_back(current);
				current.clear();
			}
		} else {
			current += c;
		}
	}
	if (!current.empty()) {
		fields.push_back(current);
	}

	if (fields.size() != 4) {
		return false;
	}

	// 盤面
	int col = 0;
	int row = 0;
	bool promoted = false;
	for (char c : fields[0]) {
		if (c == '/') {
			if (col != Shogi::BOARD_COLS || promoted) {
				return false;
			}
			++row;
			col = 0;
			if (row >= Shogi::BOARD_ROWS) {
				return false;
			}
		} else if (c >= '1' && c <= '9') {
			if (promoted) {
				return false;
			}
			col += c - '0';
			if (col > Shogi::BOARD_COLS) {
				return false;
			}
		} else if (c == '+') {
			if (promoted) {
				return false;
			}
			promoted = true;
		} else {
			auto type = piece_type_from_char(c);
			if (!type.has_value() || col >= Shogi::BOARD_COLS) {
				return false;
			}
			if (promoted && (*type == PieceType::KING || *type == PieceType::GOLD)) {
				return false;
			}
			Turn turn = std::isupper(static_cast<unsigned char>(c)) ? Turn::SENTE : Turn::GOTE;
			board_[col * Shogi::BOARD_ROWS + row] = Cell(*type, turn, promoted);
			promoted = false;
			++col;
		}
	}
	if (row != Shogi::BOARD_ROWS - 1 || col != Shogi::BOARD_COLS || promoted) {
		return false;
	}

	// 手番
	if (fields[1] == "b") {
		turn_to_move_ = Turn::SENTE;
	} else if (fields[1] == "w") {
		turn_to_move_ = Turn::GOTE;
	} else {
		return false;
	}

	// 持ち駒
	if (fields[2] != "-") {
		int count = 0;
		bool has_digits = false;
		for (char c : fields[2]) {
			if (c >= '0' && c <= '9') {
				count = count * 10 + (c - '0');
				has_digits = true;
				if (count > 18) {
					return false;
				}
			} else {
				auto type = piece_type_from_char(c);
				if (!type.has_value() || *type == PieceType::KING) {
					return false;
				}
				if (has_digits && count == 0) {
					return false;
				}
				Turn turn = std::isupper(static_cast<unsigned char>(c)) ? Turn::SENTE : Turn::GOTE;
				int &slot = hand_[static_cast<int>(turn)][static_cast<int>(*type)];
				slot += has_digits ? count : 1;
				if (slot > 18) {
					return false;
				}
				count = 0;
				has_digits = false;
			}
		}
		if (has_digits) {
			return false;
		}
	}

	// 手数
	if (fields[3].empty() || fields[3][0] == '0') {
		return false;
	}
	for (char c : fields[3]) {
		if (c < '0' || c > '9') {
			return false;
		}
	}

	return true;
}

void BoardState::load_zobrist_params(const String &path) {
	if (g_zobrist_initialized) {
		return;
	}

	if (!FileAccess::file_exists(path)) {
		UtilityFunctions::print("Zobrist params file not found: " + path);
		return;
	}

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file->get_32() != 0x5A4F4252) { // "ZOBR"
		UtilityFunctions::print("Invalid Zobrist params file format.");
		return;
	}

	// 盤上の駒
	for (Turn turn : { Turn::SENTE, Turn::GOTE }) {
		for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
			for (int is_promoted = 0; is_promoted < 2; ++is_promoted) {
				for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
					for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
						g_zobrist_board[static_cast<int>(turn)][piece_type][is_promoted][col][row] = file->get_64();
					}
				}
			}
		}
	}

	// 持ち駒
	for (Turn turn : { Turn::SENTE, Turn::GOTE }) {
		for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
			for (int n = 0; n < 20; ++n) {
				g_zobrist_hand[static_cast<int>(turn)][piece_type][n] = file->get_64();
			}
		}
	}

	g_zobrist_turn_enemy = file->get_64();
	g_zobrist_initialized = true;

	UtilityFunctions::print("Zobrist parameters loaded successfully.");
}

void BoardState::build_bitboard() {
	bitboard_all_ = Bitboard();
	bitboard_side_[0] = Bitboard();
	bitboard_side_[1] = Bitboard();
	bitboard_promoted_[0] = Bitboard();
	bitboard_promoted_[1] = Bitboard();

	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < Shogi::PIECE_TYPE_COUNT; j++) {
			bitboard_piece_[i][j] = Bitboard();
		}
	}

	for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
		for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
			Coord coord{ col, row };
			const Cell &cell = get_cell(coord);
			if (cell.is_empty()) {
				continue;
			}

			int index = col * Shogi::BOARD_ROWS + row;
			add_piece_to_bitboard(index, cell.turn, cell.type, cell.is_promoted);
		}
	}
}

void BoardState::add_piece_to_bitboard(int index, Turn turn, PieceType type, bool is_promoted) {
	int turn_indx = static_cast<int>(turn);
	int piece_type_index = static_cast<int>(type);

	bitboard_piece_[turn_indx][piece_type_index].set(index);
	bitboard_side_[turn_indx].set(index);
	bitboard_all_.set(index);
	if (is_promoted) {
		bitboard_promoted_[turn_indx].set(index);
	}
}

void BoardState::remove_piece_from_bitboard(int index, Turn turn, PieceType type, bool is_promoted) {
	int turn_indx = static_cast<int>(turn);
	int piece_type_index = static_cast<int>(type);

	bitboard_piece_[turn_indx][piece_type_index].clear(index);
	bitboard_side_[turn_indx].clear(index);
	bitboard_all_.clear(index);
	if (is_promoted) {
		bitboard_promoted_[turn_indx].clear(index);
	}
}

uint64_t BoardState::calculate_zobrist_hash() const {
	uint64_t hash = 0;

	// 盤上の駒
	for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
		for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
			const Cell &cell = get_cell({ col, row });
			if (!cell.is_empty()) {
				int is_promoted = cell.is_promoted ? 1 : 0;
				hash ^=
						g_zobrist_board[static_cast<int>(cell.turn)][static_cast<int>(cell.type)][is_promoted][col][row];
			}
		}
	}

	// 持ち駒
	for (Turn turn : { Turn::SENTE, Turn::GOTE }) {
		for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
			int count = hand_[static_cast<int>(turn)][piece_type];
			if (count > 0) {
				int index = (count >= 20) ? 19 : count;
				hash ^= g_zobrist_hand[static_cast<int>(turn)][piece_type][index];
			}
		}
	}

	if (turn_to_move_ == Turn::GOTE) {
		hash ^= g_zobrist_turn_enemy;
	}

	return hash;
}

uint64_t BoardState::get_zobrist_hash() const {
	return zobrist_hash_;
}

std::optional<Coord> BoardState::get_king_position(Turn turn) const {
	return king_pos_[static_cast<int>(turn)];
}

void BoardState::update_king_position_cache() {
	king_pos_[static_cast<int>(Turn::SENTE)] = std::nullopt;
	king_pos_[static_cast<int>(Turn::GOTE)] = std::nullopt;

	for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
		for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
			const Cell &cell = board_[col * Shogi::BOARD_ROWS + row];
			if (cell.type == PieceType::KING) {
				king_pos_[static_cast<int>(cell.turn)] = Coord{ col, row };
			}
		}
	}
}

void BoardState::update_pawn_columns_cache() {
	pawn_columns_[static_cast<int>(Turn::SENTE)] = 0;
	pawn_columns_[static_cast<int>(Turn::GOTE)] = 0;

	for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
		for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
			const Cell &cell = board_[col * Shogi::BOARD_ROWS + row];
			if (cell.type == PieceType::PAWN && !cell.is_promoted) {
				pawn_columns_[static_cast<int>(cell.turn)] |= (1 << col);
			}
		}
	}
}

const Cell &BoardState::get_cell(Coord coord) const {
	return board_[coord.col * Shogi::BOARD_ROWS + coord.row];
}

void BoardState::set_cell(Coord coord, PieceType type, Turn turn, bool is_promoted) {
	if (!coord.is_valid()) {
		return;
	}

	int index = coord.col * Shogi::BOARD_ROWS + coord.row;
	Cell old_cell = get_cell(coord);
	if (!old_cell.is_empty()) {
		int old_is_promoted = old_cell.is_promoted ? 1 : 0;
		zobrist_hash_ ^= g_zobrist_board[static_cast<int>(old_cell.turn)][static_cast<int>(old_cell.type)]
										[old_is_promoted][coord.col][coord.row];
		remove_piece_from_bitboard(index, old_cell.turn, old_cell.type, old_cell.is_promoted);
		if (old_cell.type == PieceType::KING) {
			auto &cached_pos = king_pos_[static_cast<int>(old_cell.turn)];
			if (cached_pos && cached_pos->col == coord.col && cached_pos->row == coord.row) {
				cached_pos = std::nullopt;
			}
		}
	}

	int new_is_promoted = is_promoted ? 1 : 0;
	zobrist_hash_ ^=
			g_zobrist_board[static_cast<int>(turn)][static_cast<int>(type)][new_is_promoted][coord.col][coord.row];

	board_[index] = Cell(type, turn, is_promoted);
	add_piece_to_bitboard(index, turn, type, is_promoted);

	if (type == PieceType::KING) {
		king_pos_[static_cast<int>(turn)] = coord;
	}

	if (type == PieceType::PAWN && !is_promoted) {
		pawn_columns_[static_cast<int>(turn)] |= (1 << coord.col);
	}
}

void BoardState::clear_cell(Coord coord) {
	if (!coord.is_valid()) {
		return;
	}

	int index = coord.col * Shogi::BOARD_ROWS + coord.row;
	Cell old_cell = get_cell(coord);
	if (!old_cell.is_empty()) {
		int old_is_promoted = old_cell.is_promoted ? 1 : 0;
		zobrist_hash_ ^= g_zobrist_board[static_cast<int>(old_cell.turn)][static_cast<int>(old_cell.type)]
										[old_is_promoted][coord.col][coord.row];
		remove_piece_from_bitboard(index, old_cell.turn, old_cell.type, old_cell.is_promoted);
		if (old_cell.type == PieceType::KING) {
			auto &cached_pos = king_pos_[static_cast<int>(old_cell.turn)];
			if (cached_pos && cached_pos->col == coord.col && cached_pos->row == coord.row) {
				cached_pos = std::nullopt;
			}
		}

		if (old_cell.type == PieceType::PAWN && !old_cell.is_promoted) {
			bool has_other_pawn = false;
			for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
				if (row == coord.row) {
					continue;
				}
				const Cell &cell = board_[coord.col * Shogi::BOARD_ROWS + row];
				if (cell.type == PieceType::PAWN && !cell.is_promoted && cell.turn == old_cell.turn) {
					has_other_pawn = true;
					break;
				}
			}
			if (!has_other_pawn) {
				pawn_columns_[static_cast<int>(old_cell.turn)] &= ~(1 << coord.col);
			}
		}
	}

	board_[index] = Cell();
}

int BoardState::get_hand_count(Turn turn, PieceType piece_type) const {
	int side_idx = static_cast<int>(turn);
	int type_idx = static_cast<int>(piece_type);
	if (side_idx < 0 || side_idx >= 2 || type_idx < 0 || type_idx >= Shogi::PIECE_TYPE_COUNT) {
		return 0;
	}
	return hand_[side_idx][type_idx];
}

void BoardState::set_hand_count(Turn turn, PieceType piece_type, int count) {
	int side_idx = static_cast<int>(turn);
	int type_idx = static_cast<int>(piece_type);
	if (side_idx < 0 || side_idx >= 2 || type_idx < 0 || type_idx >= Shogi::PIECE_TYPE_COUNT || count < 0) {
		return;
	}

	int old_count = hand_[side_idx][type_idx];
	if (old_count == count) {
		return;
	}

	int idx_old = std::clamp(old_count, 0, 19);
	int idx_new = std::clamp(count, 0, 19);
	zobrist_hash_ ^= g_zobrist_hand[side_idx][type_idx][idx_old];
	zobrist_hash_ ^= g_zobrist_hand[side_idx][type_idx][idx_new];
	hand_[side_idx][type_idx] = count;
}

Shogi::UndoInfo BoardState::apply_move(const Move &move) {
	Shogi::UndoInfo undo;
	undo.move = move;
	undo.prev_hash = zobrist_hash_;
	undo.prev_pawn_cols[0] = pawn_columns_[0];
	undo.prev_pawn_cols[1] = pawn_columns_[1];
	undo.prev_score = score_;

	Turn current_side = turn_to_move_;
	Turn opponent_side = (turn_to_move_ == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
	int sign = (current_side == Turn::SENTE) ? 1 : -1;

	int from_idx = move.from_col * Shogi::BOARD_ROWS + move.from_row;
	int to_idx = move.to_col * Shogi::BOARD_ROWS + move.to_row;

	Cell target = get_cell({ move.to_col, move.to_row });
	if (!target.is_empty()) {
		undo.captured_type = static_cast<uint8_t>(target.type);
		undo.captured_promoted = target.is_promoted;
	} else {
		undo.captured_type = static_cast<uint8_t>(PieceType::EMPTY);
		undo.captured_promoted = false;
	}

	if (move.is_drop) {
		PieceType piece_type = move.piece_type;
		int count = hand_[static_cast<int>(current_side)][static_cast<int>(piece_type)];

		int hand_value = Shogi::PIECE_VALUES[static_cast<int>(piece_type)][0];
		int board_score = Shogi::get_piece_score(piece_type, false, current_side, move.to_col, move.to_row);
		board_score = Shogi::apply_board_discount(board_score);
		score_ += sign * (board_score - hand_value);

		int idx_old = std::clamp(count, 0, 19);
		int idx_new = std::clamp(count - 1, 0, 19);
		zobrist_hash_ ^= g_zobrist_hand[static_cast<int>(current_side)][static_cast<int>(piece_type)][idx_old];
		zobrist_hash_ ^= g_zobrist_hand[static_cast<int>(current_side)][static_cast<int>(piece_type)][idx_new];
		if (hand_[static_cast<int>(current_side)][static_cast<int>(move.piece_type)] > 0) {
			hand_[static_cast<int>(current_side)][static_cast<int>(move.piece_type)]--;
		}

		zobrist_hash_ ^=
				g_zobrist_board[static_cast<int>(current_side)][static_cast<int>(piece_type)][0][move.to_col][move.to_row];
		board_[to_idx] = Cell(piece_type, current_side, false);
		add_piece_to_bitboard(to_idx, current_side, piece_type, false);

		if (piece_type == PieceType::PAWN) {
			pawn_columns_[static_cast<int>(current_side)] |= (1 << move.to_col);
		}
	} else {
		Cell source = get_cell({ move.from_col, move.from_row });

		int from_score =
				Shogi::get_piece_score(source.type, source.is_promoted, current_side, move.from_col, move.from_row);
		from_score = Shogi::apply_board_discount(from_score);
		score_ -= sign * from_score;

		int src_is_promoted = source.is_promoted ? 1 : 0;
		zobrist_hash_ ^= g_zobrist_board[static_cast<int>(current_side)][static_cast<int>(source.type)][src_is_promoted]
										[move.from_col][move.from_row];
		remove_piece_from_bitboard(from_idx, current_side, source.type, source.is_promoted);

		if (!target.is_empty()) {
			int captured_score =
					Shogi::get_piece_score(target.type, target.is_promoted, opponent_side, move.to_col, move.to_row);
			captured_score = Shogi::apply_board_discount(captured_score);
			score_ += sign * captured_score;
			int hand_value = Shogi::PIECE_VALUES[static_cast<int>(target.type)][0];
			score_ += sign * hand_value;

			int tgt_is_promoted = target.is_promoted ? 1 : 0;
			zobrist_hash_ ^= g_zobrist_board[static_cast<int>(opponent_side)][static_cast<int>(target.type)]
											[tgt_is_promoted][move.to_col][move.to_row];
			remove_piece_from_bitboard(to_idx, opponent_side, target.type, target.is_promoted);

			PieceType captured_type = target.type;
			int count = hand_[static_cast<int>(current_side)][static_cast<int>(captured_type)];
			int idx_old = std::clamp(count, 0, 19);
			int idx_new = std::clamp(count + 1, 0, 19);
			zobrist_hash_ ^= g_zobrist_hand[static_cast<int>(current_side)][static_cast<int>(captured_type)][idx_old];
			zobrist_hash_ ^= g_zobrist_hand[static_cast<int>(current_side)][static_cast<int>(captured_type)][idx_new];
			hand_[static_cast<int>(current_side)][static_cast<int>(captured_type)]++;
		}

		bool is_promoted = move.is_promotion || source.is_promoted;

		int to_score = Shogi::get_piece_score(source.type, is_promoted, current_side, move.to_col, move.to_row);
		to_score = Shogi::apply_board_discount(to_score);
		score_ += sign * to_score;

		int new_is_promoted = is_promoted ? 1 : 0;
		zobrist_hash_ ^= g_zobrist_board[static_cast<int>(current_side)][static_cast<int>(source.type)][new_is_promoted]
										[move.to_col][move.to_row];

		board_[to_idx] = Cell(source.type, current_side, is_promoted);
		board_[from_idx] = Cell();
		add_piece_to_bitboard(to_idx, current_side, source.type, is_promoted);

		if (source.type == PieceType::KING) {
			king_pos_[static_cast<int>(current_side)] = Coord{ move.to_col, move.to_row };
		}

		if (source.type == PieceType::PAWN && !source.is_promoted) {
			pawn_columns_[static_cast<int>(current_side)] &= ~(1 << move.from_col);
			if (!is_promoted) {
				pawn_columns_[static_cast<int>(current_side)] |= (1 << move.to_col);
			}
		}
		if (!target.is_empty() && target.type == PieceType::PAWN && !target.is_promoted) {
			pawn_columns_[static_cast<int>(opponent_side)] &= ~(1 << move.to_col);
		}
	}

	zobrist_hash_ ^= g_zobrist_turn_enemy;
	turn_to_move_ = opponent_side;

	return undo;
}

void BoardState::undo_move(const Shogi::UndoInfo &undo) {
	const Move &move = undo.move;

	Turn original_side = (turn_to_move_ == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
	Turn opponent_side = turn_to_move_;

	int from_idx = move.from_col * Shogi::BOARD_ROWS + move.from_row;
	int to_idx = move.to_col * Shogi::BOARD_ROWS + move.to_row;

	if (move.is_drop) {
		remove_piece_from_bitboard(to_idx, original_side, move.piece_type, false);
		board_[to_idx] = Cell();
		hand_[static_cast<int>(original_side)][static_cast<int>(move.piece_type)]++;
	} else {
		Cell moved_piece = get_cell({ move.to_col, move.to_row });
		remove_piece_from_bitboard(to_idx, original_side, moved_piece.type, moved_piece.is_promoted);

		bool was_promoted_before = moved_piece.is_promoted && !move.is_promotion;
		if (move.is_promotion) {
			was_promoted_before = false;
		} else {
			was_promoted_before = moved_piece.is_promoted;
		}

		board_[from_idx] = Cell(moved_piece.type, original_side, was_promoted_before);
		add_piece_to_bitboard(from_idx, original_side, moved_piece.type, was_promoted_before);

		PieceType captured_type = static_cast<PieceType>(undo.captured_type);
		if (captured_type != PieceType::EMPTY) {
			board_[to_idx] = Cell(captured_type, opponent_side, undo.captured_promoted);
			add_piece_to_bitboard(to_idx, opponent_side, captured_type, undo.captured_promoted);
			if (hand_[static_cast<int>(original_side)][static_cast<int>(captured_type)] > 0) {
				hand_[static_cast<int>(original_side)][static_cast<int>(captured_type)]--;
			}
		} else {
			board_[to_idx] = Cell();
		}

		if (moved_piece.type == PieceType::KING) {
			king_pos_[static_cast<int>(original_side)] = Coord{ move.from_col, move.from_row };
		}
	}

	pawn_columns_[0] = undo.prev_pawn_cols[0];
	pawn_columns_[1] = undo.prev_pawn_cols[1];

	zobrist_hash_ = undo.prev_hash;
	score_ = undo.prev_score;

	turn_to_move_ = original_side;
}

uint64_t BoardState::make_null_move() {
	uint64_t prev_hash = zobrist_hash_;
	zobrist_hash_ ^= g_zobrist_turn_enemy;
	turn_to_move_ = (turn_to_move_ == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
	return prev_hash;
}

void BoardState::undo_null_move(uint64_t prev_hash) {
	zobrist_hash_ = prev_hash;
	turn_to_move_ = (turn_to_move_ == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
}

void BoardState::print_board() const {
	UtilityFunctions::print("--- Board State ---");
	for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
		String line = "";
		for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
			const Cell &cell = get_cell({ col, row });
			if (cell.is_empty()) {
				line += ". ";
			} else {
				String piece_str = String::num_int64(static_cast<int>(cell.type));
				if (cell.is_promoted) {
					piece_str += "+";
				}
				if (cell.turn == Turn::GOTE) {
					piece_str = piece_str.to_upper();
				}
				line += piece_str + " ";
			}
		}
		UtilityFunctions::print(line);
	}

	UtilityFunctions::print("Player Hand:");
	for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
		UtilityFunctions::print("Type " + String::num_int64(piece_type) + ": " +
				String::num_int64(hand_[static_cast<int>(Turn::SENTE)][piece_type]));
	}

	UtilityFunctions::print("Enemy Hand:");
	for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
		UtilityFunctions::print("Type " + String::num_int64(piece_type) + ": " +
				String::num_int64(hand_[static_cast<int>(Turn::GOTE)][piece_type]));
	}
}
