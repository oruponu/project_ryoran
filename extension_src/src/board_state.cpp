#include "attack_table.hpp"
#include "board_state.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <span>
#include <string>
#include <utility>
#include <vector>

using Shogi::Coord;
using Shogi::Move;
using Shogi::PieceType;
using Shogi::Turn;

namespace {

constexpr int ZOBRIST_HAND_SLOTS = 20;

uint64_t g_zobrist_board[2][Shogi::PIECE_TYPE_COUNT][2][Shogi::BOARD_COLS][Shogi::BOARD_ROWS];
uint64_t g_zobrist_hand[2][Shogi::PIECE_TYPE_COUNT][ZOBRIST_HAND_SLOTS];
uint64_t g_zobrist_gote_to_move;
bool g_zobrist_initialized = false;

uint64_t zobrist_hand(Turn turn, PieceType piece_type, int count) {
	if (count <= 0) {
		return 0;
	}
	return g_zobrist_hand[std::to_underlying(turn)][std::to_underlying(piece_type)][std::min(count, ZOBRIST_HAND_SLOTS - 1)];
}

std::optional<Shogi::PieceType> piece_type_from_char(char c) {
	using enum Shogi::PieceType;
	switch (std::toupper(static_cast<unsigned char>(c))) {
		case 'K':
			return KING;
		case 'R':
			return ROOK;
		case 'B':
			return BISHOP;
		case 'G':
			return GOLD;
		case 'S':
			return SILVER;
		case 'N':
			return KNIGHT;
		case 'L':
			return LANCE;
		case 'P':
			return PAWN;
		default:
			return std::nullopt;
	}
}

constexpr uint32_t ZOBRIST_MAGIC = 0x5A4F4252; // "ZOBR"

uint32_t read_u32_le(const uint8_t *p) {
	return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
			(static_cast<uint32_t>(p[3]) << 24);
}

uint64_t read_u64_le(const uint8_t *p) {
	return static_cast<uint64_t>(read_u32_le(p)) | (static_cast<uint64_t>(read_u32_le(p + 4)) << 32);
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
		for (int piece_type_index = 0; piece_type_index < Shogi::PIECE_TYPE_COUNT; ++piece_type_index) {
			hand_[std::to_underlying(turn)][piece_type_index] = 0;
		}
	}

	king_position_[std::to_underlying(Turn::SENTE)] = std::nullopt;
	king_position_[std::to_underlying(Turn::GOTE)] = std::nullopt;
	pawn_columns_[std::to_underlying(Turn::SENTE)] = 0;
	pawn_columns_[std::to_underlying(Turn::GOTE)] = 0;

	zobrist_hash_ = calculate_zobrist_hash();

	build_bitboard();
}

std::optional<BoardState> BoardState::from_sfen(const std::string &sfen) {
	BoardState board(Turn::SENTE);
	if (!board.parse_sfen(sfen)) {
		return std::nullopt;
	}

	board.update_king_position_cache();
	board.update_pawn_columns_cache();
	board.zobrist_hash_ = board.calculate_zobrist_hash();
	board.score_ = Evaluator::calculate_score(board);

	board.build_bitboard();
	return board;
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
			auto piece_type = piece_type_from_char(c);
			if (!piece_type.has_value() || col >= Shogi::BOARD_COLS) {
				return false;
			}
			if (promoted && (*piece_type == PieceType::KING || *piece_type == PieceType::GOLD)) {
				return false;
			}
			Turn turn = std::isupper(static_cast<unsigned char>(c)) ? Turn::SENTE : Turn::GOTE;
			board_[Shogi::to_index(col, row)] = Cell(*piece_type, turn, promoted);
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
				auto piece_type = piece_type_from_char(c);
				if (!piece_type.has_value() || *piece_type == PieceType::KING) {
					return false;
				}
				if (has_digits && count == 0) {
					return false;
				}
				Turn turn = std::isupper(static_cast<unsigned char>(c)) ? Turn::SENTE : Turn::GOTE;
				int &slot = hand_[std::to_underlying(turn)][std::to_underlying(*piece_type)];
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

bool BoardState::zobrist_initialized() {
	return g_zobrist_initialized;
}

bool BoardState::load_zobrist_params(std::span<const uint8_t> data) {
	if (g_zobrist_initialized) {
		return true;
	}

	constexpr size_t value_count = sizeof(g_zobrist_board) / sizeof(uint64_t) +
			sizeof(g_zobrist_hand) / sizeof(uint64_t) + 1;
	if (data.size() < sizeof(uint32_t) + value_count * sizeof(uint64_t) || read_u32_le(data.data()) != ZOBRIST_MAGIC) {
		return false;
	}

	std::span<const uint8_t> rest = data.subspan(sizeof(uint32_t));
	auto next = [&rest]() {
		uint64_t value = read_u64_le(rest.data());
		rest = rest.subspan(sizeof(uint64_t));
		return value;
	};

	// 盤上の駒
	for (Turn turn : { Turn::SENTE, Turn::GOTE }) {
		for (int piece_type_index = 0; piece_type_index < Shogi::PIECE_TYPE_COUNT; ++piece_type_index) {
			for (int is_promoted = 0; is_promoted < 2; ++is_promoted) {
				for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
					for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
						g_zobrist_board[std::to_underlying(turn)][piece_type_index][is_promoted][col][row] = next();
					}
				}
			}
		}
	}

	// 持ち駒
	for (Turn turn : { Turn::SENTE, Turn::GOTE }) {
		for (int piece_type_index = 0; piece_type_index < Shogi::PIECE_TYPE_COUNT; ++piece_type_index) {
			for (int n = 0; n < ZOBRIST_HAND_SLOTS; ++n) {
				g_zobrist_hand[std::to_underlying(turn)][piece_type_index][n] = next();
			}
		}
	}

	g_zobrist_gote_to_move = next();
	g_zobrist_initialized = true;
	return true;
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

			int index = Shogi::to_index(col, row);
			add_piece_to_bitboard(index, cell.turn, cell.piece_type, cell.is_promoted);
		}
	}
}

void BoardState::add_piece_to_bitboard(int index, Turn turn, PieceType piece_type, bool is_promoted) {
	int turn_index = std::to_underlying(turn);
	int piece_type_index = std::to_underlying(piece_type);

	bitboard_piece_[turn_index][piece_type_index].set(index);
	bitboard_side_[turn_index].set(index);
	bitboard_all_.set(index);
	if (is_promoted) {
		bitboard_promoted_[turn_index].set(index);
	}
}

void BoardState::remove_piece_from_bitboard(int index, Turn turn, PieceType piece_type, bool is_promoted) {
	int turn_index = std::to_underlying(turn);
	int piece_type_index = std::to_underlying(piece_type);

	bitboard_piece_[turn_index][piece_type_index].clear(index);
	bitboard_side_[turn_index].clear(index);
	bitboard_all_.clear(index);
	if (is_promoted) {
		bitboard_promoted_[turn_index].clear(index);
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
						g_zobrist_board[std::to_underlying(cell.turn)][std::to_underlying(cell.piece_type)][is_promoted][col][row];
			}
		}
	}

	// 持ち駒
	for (Turn turn : { Turn::SENTE, Turn::GOTE }) {
		for (int piece_type_index = 0; piece_type_index < Shogi::PIECE_TYPE_COUNT; ++piece_type_index) {
			int count = hand_[std::to_underlying(turn)][piece_type_index];
			hash ^= zobrist_hand(turn, static_cast<PieceType>(piece_type_index), count);
		}
	}

	if (turn_to_move_ == Turn::GOTE) {
		hash ^= g_zobrist_gote_to_move;
	}

	return hash;
}

uint64_t BoardState::get_zobrist_hash() const {
	return zobrist_hash_;
}

std::optional<Coord> BoardState::get_king_position(Turn turn) const {
	return king_position_[std::to_underlying(turn)];
}

void BoardState::update_king_position_cache() {
	king_position_[std::to_underlying(Turn::SENTE)] = std::nullopt;
	king_position_[std::to_underlying(Turn::GOTE)] = std::nullopt;

	for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
		for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
			const Cell &cell = board_[Shogi::to_index(col, row)];
			if (cell.piece_type == PieceType::KING) {
				king_position_[std::to_underlying(cell.turn)] = Coord{ col, row };
			}
		}
	}
}

void BoardState::update_pawn_columns_cache() {
	pawn_columns_[std::to_underlying(Turn::SENTE)] = 0;
	pawn_columns_[std::to_underlying(Turn::GOTE)] = 0;

	for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
		for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
			const Cell &cell = board_[Shogi::to_index(col, row)];
			if (cell.piece_type == PieceType::PAWN && !cell.is_promoted) {
				pawn_columns_[std::to_underlying(cell.turn)] |= (1 << col);
			}
		}
	}
}

void BoardState::set_cell(Coord coord, PieceType piece_type, Turn turn, bool is_promoted) {
	if (!coord.is_valid()) {
		return;
	}

	clear_cell(coord);

	int index = coord.to_index();
	int new_is_promoted = is_promoted ? 1 : 0;
	zobrist_hash_ ^=
			g_zobrist_board[std::to_underlying(turn)][std::to_underlying(piece_type)][new_is_promoted][coord.col][coord.row];

	board_[index] = Cell(piece_type, turn, is_promoted);
	add_piece_to_bitboard(index, turn, piece_type, is_promoted);

	if (piece_type == PieceType::KING) {
		king_position_[std::to_underlying(turn)] = coord;
	}

	if (piece_type == PieceType::PAWN && !is_promoted) {
		pawn_columns_[std::to_underlying(turn)] |= (1 << coord.col);
	}
}

void BoardState::clear_cell(Coord coord) {
	if (!coord.is_valid()) {
		return;
	}

	int index = coord.to_index();
	Cell old_cell = get_cell(coord);
	if (!old_cell.is_empty()) {
		int old_is_promoted = old_cell.is_promoted ? 1 : 0;
		zobrist_hash_ ^= g_zobrist_board[std::to_underlying(old_cell.turn)][std::to_underlying(old_cell.piece_type)]
										[old_is_promoted][coord.col][coord.row];
		remove_piece_from_bitboard(index, old_cell.turn, old_cell.piece_type, old_cell.is_promoted);
		if (old_cell.piece_type == PieceType::KING) {
			auto &cached_position = king_position_[std::to_underlying(old_cell.turn)];
			if (cached_position && cached_position->col == coord.col && cached_position->row == coord.row) {
				cached_position = std::nullopt;
			}
		}

		if (old_cell.piece_type == PieceType::PAWN && !old_cell.is_promoted) {
			bool has_other_pawn = false;
			for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
				if (row == coord.row) {
					continue;
				}
				const Cell &cell = board_[Shogi::to_index(coord.col, row)];
				if (cell.piece_type == PieceType::PAWN && !cell.is_promoted && cell.turn == old_cell.turn) {
					has_other_pawn = true;
					break;
				}
			}
			if (!has_other_pawn) {
				pawn_columns_[std::to_underlying(old_cell.turn)] &= ~(1 << coord.col);
			}
		}
	}

	board_[index] = Cell();
}

int BoardState::get_hand_count(Turn turn, PieceType piece_type) const {
	int turn_index = std::to_underlying(turn);
	int piece_type_index = std::to_underlying(piece_type);
	if (turn_index >= 2 || piece_type_index >= Shogi::PIECE_TYPE_COUNT) {
		return 0;
	}
	return hand_[turn_index][piece_type_index];
}

void BoardState::set_hand_count(Turn turn, PieceType piece_type, int count) {
	int turn_index = std::to_underlying(turn);
	int piece_type_index = std::to_underlying(piece_type);
	if (turn_index >= 2 || piece_type_index >= Shogi::PIECE_TYPE_COUNT || count < 0) {
		return;
	}

	int old_count = hand_[turn_index][piece_type_index];
	if (old_count == count) {
		return;
	}

	zobrist_hash_ ^= zobrist_hand(turn, piece_type, old_count);
	zobrist_hash_ ^= zobrist_hand(turn, piece_type, count);
	hand_[turn_index][piece_type_index] = count;
}

Shogi::UndoInfo BoardState::apply_move(const Move &move) {
	Shogi::UndoInfo undo;
	undo.move = move;
	undo.prev_hash = zobrist_hash_;
	undo.prev_pawn_columns[0] = pawn_columns_[0];
	undo.prev_pawn_columns[1] = pawn_columns_[1];
	undo.prev_score = score_;

	Turn turn = turn_to_move_;
	Turn opponent_turn = Shogi::opponent(turn_to_move_);
	int sign = (turn == Turn::SENTE) ? 1 : -1;

	int from_index = Shogi::to_index(move.from_col, move.from_row);
	int to_index = Shogi::to_index(move.to_col, move.to_row);

	Cell target = get_cell({ move.to_col, move.to_row });
	if (!target.is_empty()) {
		undo.captured_type = target.piece_type;
		undo.captured_promoted = target.is_promoted;
	} else {
		undo.captured_type = PieceType::EMPTY;
		undo.captured_promoted = false;
	}

	if (move.is_drop) {
		PieceType piece_type = move.piece_type;
		int count = hand_[std::to_underlying(turn)][std::to_underlying(piece_type)];

		int hand_value = Shogi::PIECE_VALUES[std::to_underlying(piece_type)][0];
		int board_score = Shogi::get_piece_score(piece_type, false);
		board_score = Shogi::apply_board_discount(board_score);
		score_ += sign * (board_score - hand_value);

		zobrist_hash_ ^= zobrist_hand(turn, piece_type, count);
		zobrist_hash_ ^= zobrist_hand(turn, piece_type, count - 1);
		if (hand_[std::to_underlying(turn)][std::to_underlying(move.piece_type)] > 0) {
			hand_[std::to_underlying(turn)][std::to_underlying(move.piece_type)]--;
		}

		zobrist_hash_ ^=
				g_zobrist_board[std::to_underlying(turn)][std::to_underlying(piece_type)][0][move.to_col][move.to_row];
		board_[to_index] = Cell(piece_type, turn, false);
		add_piece_to_bitboard(to_index, turn, piece_type, false);

		if (piece_type == PieceType::PAWN) {
			pawn_columns_[std::to_underlying(turn)] |= (1 << move.to_col);
		}
	} else {
		Cell source = get_cell({ move.from_col, move.from_row });

		int from_score = Shogi::get_piece_score(source.piece_type, source.is_promoted);
		from_score = Shogi::apply_board_discount(from_score);
		score_ -= sign * from_score;

		int source_is_promoted = source.is_promoted ? 1 : 0;
		zobrist_hash_ ^= g_zobrist_board[std::to_underlying(turn)][std::to_underlying(source.piece_type)][source_is_promoted]
										[move.from_col][move.from_row];
		remove_piece_from_bitboard(from_index, turn, source.piece_type, source.is_promoted);

		if (!target.is_empty()) {
			int captured_score = Shogi::get_piece_score(target.piece_type, target.is_promoted);
			captured_score = Shogi::apply_board_discount(captured_score);
			score_ += sign * captured_score;
			int hand_value = Shogi::PIECE_VALUES[std::to_underlying(target.piece_type)][0];
			score_ += sign * hand_value;

			int target_is_promoted = target.is_promoted ? 1 : 0;
			zobrist_hash_ ^= g_zobrist_board[std::to_underlying(opponent_turn)][std::to_underlying(target.piece_type)]
											[target_is_promoted][move.to_col][move.to_row];
			remove_piece_from_bitboard(to_index, opponent_turn, target.piece_type, target.is_promoted);

			PieceType captured_type = target.piece_type;
			int count = hand_[std::to_underlying(turn)][std::to_underlying(captured_type)];
			zobrist_hash_ ^= zobrist_hand(turn, captured_type, count);
			zobrist_hash_ ^= zobrist_hand(turn, captured_type, count + 1);
			hand_[std::to_underlying(turn)][std::to_underlying(captured_type)]++;
		}

		bool is_promoted = move.is_promotion || source.is_promoted;

		int to_score = Shogi::get_piece_score(source.piece_type, is_promoted);
		to_score = Shogi::apply_board_discount(to_score);
		score_ += sign * to_score;

		int new_is_promoted = is_promoted ? 1 : 0;
		zobrist_hash_ ^= g_zobrist_board[std::to_underlying(turn)][std::to_underlying(source.piece_type)][new_is_promoted]
										[move.to_col][move.to_row];

		board_[to_index] = Cell(source.piece_type, turn, is_promoted);
		board_[from_index] = Cell();
		add_piece_to_bitboard(to_index, turn, source.piece_type, is_promoted);

		if (source.piece_type == PieceType::KING) {
			king_position_[std::to_underlying(turn)] = Coord{ move.to_col, move.to_row };
		}

		if (source.piece_type == PieceType::PAWN && !source.is_promoted) {
			pawn_columns_[std::to_underlying(turn)] &= ~(1 << move.from_col);
			if (!is_promoted) {
				pawn_columns_[std::to_underlying(turn)] |= (1 << move.to_col);
			}
		}
		if (!target.is_empty() && target.piece_type == PieceType::PAWN && !target.is_promoted) {
			pawn_columns_[std::to_underlying(opponent_turn)] &= ~(1 << move.to_col);
		}
	}

	zobrist_hash_ ^= g_zobrist_gote_to_move;
	turn_to_move_ = opponent_turn;

	return undo;
}

void BoardState::undo_move(const Shogi::UndoInfo &undo) {
	const Move &move = undo.move;

	Turn turn = Shogi::opponent(turn_to_move_);
	Turn opponent_turn = turn_to_move_;

	int from_index = Shogi::to_index(move.from_col, move.from_row);
	int to_index = Shogi::to_index(move.to_col, move.to_row);

	if (move.is_drop) {
		remove_piece_from_bitboard(to_index, turn, move.piece_type, false);
		board_[to_index] = Cell();
		hand_[std::to_underlying(turn)][std::to_underlying(move.piece_type)]++;
	} else {
		Cell moved_piece = get_cell({ move.to_col, move.to_row });
		remove_piece_from_bitboard(to_index, turn, moved_piece.piece_type, moved_piece.is_promoted);

		bool was_promoted_before = moved_piece.is_promoted && !move.is_promotion;

		board_[from_index] = Cell(moved_piece.piece_type, turn, was_promoted_before);
		add_piece_to_bitboard(from_index, turn, moved_piece.piece_type, was_promoted_before);

		PieceType captured_type = undo.captured_type;
		if (captured_type != PieceType::EMPTY) {
			board_[to_index] = Cell(captured_type, opponent_turn, undo.captured_promoted);
			add_piece_to_bitboard(to_index, opponent_turn, captured_type, undo.captured_promoted);
			if (hand_[std::to_underlying(turn)][std::to_underlying(captured_type)] > 0) {
				hand_[std::to_underlying(turn)][std::to_underlying(captured_type)]--;
			}
		} else {
			board_[to_index] = Cell();
		}

		if (moved_piece.piece_type == PieceType::KING) {
			king_position_[std::to_underlying(turn)] = Coord{ move.from_col, move.from_row };
		}
	}

	pawn_columns_[0] = undo.prev_pawn_columns[0];
	pawn_columns_[1] = undo.prev_pawn_columns[1];

	zobrist_hash_ = undo.prev_hash;
	score_ = undo.prev_score;

	turn_to_move_ = turn;
}

uint64_t BoardState::make_null_move() {
	uint64_t prev_hash = zobrist_hash_;
	zobrist_hash_ ^= g_zobrist_gote_to_move;
	turn_to_move_ = Shogi::opponent(turn_to_move_);
	return prev_hash;
}

void BoardState::undo_null_move(uint64_t prev_hash) {
	zobrist_hash_ = prev_hash;
	turn_to_move_ = Shogi::opponent(turn_to_move_);
}
