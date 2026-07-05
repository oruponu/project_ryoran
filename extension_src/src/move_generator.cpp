#include "move_generator.hpp"
#include <algorithm>
#include <array>
#include <cassert>

using Shogi::Coord;
using Shogi::PieceType;
using Shogi::Turn;

Bitboard MoveGenerator::get_checkers(const BoardState &board, Turn turn) {
	Bitboard checkers;
	auto king_position = board.get_king_position(turn);
	if (!king_position.has_value()) {
		return checkers;
	}

	int king_square = king_position->col * Shogi::BOARD_ROWS + king_position->row;

	const Turn enemy_turn = (turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
	int enemy_index = static_cast<int>(enemy_turn);

	const Bitboard &enemy_pawns = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::PAWN)];
	const Bitboard &enemy_lances = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::LANCE)];
	const Bitboard &enemy_knights = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::KNIGHT)];
	const Bitboard &enemy_silvers = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::SILVER)];
	const Bitboard &enemy_golds = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::GOLD)];
	const Bitboard &enemy_bishops = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::BISHOP)];
	const Bitboard &enemy_rooks = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::ROOK)];
	const Bitboard &enemy_kings = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::KING)];
	const Bitboard &enemy_promoted = board.bitboard_promoted_[enemy_index];
	const Bitboard occupancy = board.bitboard_all_;

	// 香車の利き
	Bitboard lance_attacks = AttackTable::get_lance_attacks(king_square, turn, occupancy);
	checkers |= (lance_attacks & (enemy_lances & ~enemy_promoted));

	// 角の利き
	Bitboard bishop_attacks = AttackTable::get_bishop_attacks(king_square, occupancy);
	checkers |= (bishop_attacks & enemy_bishops);

	// 飛車の利き
	Bitboard rook_attacks = AttackTable::get_rook_attacks(king_square, occupancy);
	checkers |= (rook_attacks & enemy_rooks);

	// 歩の利き
	checkers |= (AttackTable::get_pawn_attacks(king_square, turn) & (enemy_pawns & ~enemy_promoted));

	// 桂馬の利き
	checkers |= (AttackTable::get_knight_attacks(king_square, turn) & (enemy_knights & ~enemy_promoted));

	// 銀の利き
	checkers |= (AttackTable::get_silver_attacks(king_square, turn) & (enemy_silvers & ~enemy_promoted));

	// 金および金と同じ動きをする成り駒の利きをチェック
	Bitboard gold_likes = enemy_golds | (enemy_promoted & (enemy_pawns | enemy_lances | enemy_knights | enemy_silvers));
	checkers |= (AttackTable::get_gold_attacks(king_square, turn) & gold_likes);

	// 玉および竜（斜め1マス）と馬（縦横1マス）の利きをチェック
	Bitboard king_likes = enemy_kings | (enemy_promoted & (enemy_bishops | enemy_rooks));
	checkers |= (AttackTable::get_king_attacks(king_square) & king_likes);

	return checkers;
}

PinMasks MoveGenerator::calculate_pin_masks(const BoardState &board, Turn turn) {
	PinMasks pin_masks;
	pin_masks.pinned = Bitboard();

	auto king_position = board.get_king_position(turn);
	if (!king_position.has_value()) {
		return pin_masks;
	}

	int king_square = king_position->col * Shogi::BOARD_ROWS + king_position->row;

	Turn enemy_turn = (turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
	int my_index = static_cast<int>(turn);
	int enemy_index = static_cast<int>(enemy_turn);

	const Bitboard &enemy_lances = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::LANCE)];
	const Bitboard &enemy_bishops = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::BISHOP)];
	const Bitboard &enemy_rooks = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::ROOK)];
	const Bitboard &enemy_promoted = board.bitboard_promoted_[enemy_index];

	Bitboard enemy_line_sliders = enemy_rooks;
	Bitboard enemy_diagonal_sliders = enemy_bishops;

	for (int dir = 0; dir < 8; ++dir) {
		Bitboard sliders;
		if (dir % 2 != 0) {
			sliders = enemy_diagonal_sliders;
		} else {
			sliders = enemy_line_sliders;
			if (dir == 0 && enemy_turn == Turn::GOTE) {
				sliders |= (enemy_lances & ~enemy_promoted);
			} else if (dir == 4 && enemy_turn == Turn::SENTE) {
				sliders |= (enemy_lances & ~enemy_promoted);
			}
		}

		Bitboard ray = AttackTable::get_ray(king_square, dir);
		Bitboard attackers = ray & sliders;
		if (attackers.is_empty()) {
			continue;
		}

		int attacker_square;

		// 最も玉に近い攻撃駒を特定
		if (dir >= 1 && dir <= 4) {
			attacker_square = attackers.lsb();
		} else {
			attacker_square = attackers.msb();
		}

		Bitboard path = ray ^ AttackTable::get_ray(attacker_square, dir);
		path.set(attacker_square);

		Bitboard between = path & board.bitboard_all_;
		between.clear(attacker_square);

		if (between.count() == 1) {
			int pinned_square = between.lsb();
			Bitboard pinned;
			pinned.set(pinned_square);
			if (!(pinned & board.bitboard_side_[my_index]).is_empty()) {
				pin_masks.pinned.set(pinned_square);
				pin_masks.valid_ray_masks[pinned_square] = path;
			}
		}
	}

	return pin_masks;
}

bool MoveGenerator::is_valid_move(const BoardState &board, Coord from, Coord to) {
	const Cell &piece = board.get_cell(from);
	if (piece.is_empty()) {
		return false;
	}

	// 盤面の範囲外には移動不可
	if (!to.is_valid()) {
		return false;
	}

	// 現在地と同じ場所には移動不可
	if (from == to) {
		return false;
	}

	// 味方の駒がある場所には移動不可
	const Cell &target = board.get_cell(to);
	if (!target.is_empty() && target.turn == piece.turn) {
		return false;
	}

	int from_index = from.col * Shogi::BOARD_ROWS + from.row;
	int to_index = to.col * Shogi::BOARD_ROWS + to.row;

	Bitboard attacks;
	if (piece.is_promoted) {
		switch (piece.type) {
			case PieceType::BISHOP:
				attacks = AttackTable::get_promoted_bishop_attacks(from_index, board.bitboard_all_);
				break;
			case PieceType::ROOK:
				attacks = AttackTable::get_promoted_rook_attacks(from_index, board.bitboard_all_);
				break;
			default:
				attacks = AttackTable::get_gold_attacks(from_index, piece.turn);
				break;
		}
	} else {
		switch (piece.type) {
			case PieceType::PAWN:
				attacks = AttackTable::get_pawn_attacks(from_index, piece.turn);
				break;
			case PieceType::LANCE:
				attacks = AttackTable::get_lance_attacks(from_index, piece.turn, board.bitboard_all_);
				break;
			case PieceType::KNIGHT:
				attacks = AttackTable::get_knight_attacks(from_index, piece.turn);
				break;
			case PieceType::SILVER:
				attacks = AttackTable::get_silver_attacks(from_index, piece.turn);
				break;
			case PieceType::GOLD:
				attacks = AttackTable::get_gold_attacks(from_index, piece.turn);
				break;
			case PieceType::BISHOP:
				attacks = AttackTable::get_bishop_attacks(from_index, board.bitboard_all_);
				break;
			case PieceType::ROOK:
				attacks = AttackTable::get_rook_attacks(from_index, board.bitboard_all_);
				break;
			case PieceType::KING:
				attacks = AttackTable::get_king_attacks(from_index);
				break;
			default:
				return false;
		}
	}

	return attacks.is_set(to_index);
}

bool MoveGenerator::is_valid_drop(const BoardState &board, PieceType piece_type, bool is_enemy, Coord to) {
	// 盤面の範囲外には配置不可
	if (!to.is_valid()) {
		return false;
	}

	// すでに駒がある場所には配置不可
	if (!board.get_cell(to).is_empty()) {
		return false;
	}

	Turn turn = is_enemy ? Turn::GOTE : Turn::SENTE;
	if (board.get_hand_count(turn, piece_type) <= 0) {
		return false;
	}

	// 行き所のない場所には配置不可
	if (is_dead_end(piece_type, is_enemy, to.row)) {
		return false;
	}

	// 二歩になる場所には配置不可
	if (is_nifu(board, piece_type, turn, to.col)) {
		return false;
	}

	return true;
}

bool MoveGenerator::is_legal_move(BoardState &board, Coord from, Coord to) {
	if (!is_valid_move(board, from, to)) {
		return false;
	}

	const Cell from_cell = board.get_cell(from);
	const Cell to_cell = board.get_cell(to);
	const int from_idx = from.col * Shogi::BOARD_ROWS + from.row;
	const int to_idx = to.col * Shogi::BOARD_ROWS + to.row;
	const auto old_king_pos = board.king_pos_[static_cast<int>(from_cell.turn)];

	board.board_[to_idx] = from_cell;
	board.board_[from_idx] = Cell();

	board.remove_piece_from_bitboard(from_idx, from_cell.turn, from_cell.type, from_cell.is_promoted);
	if (!to_cell.is_empty()) {
		board.remove_piece_from_bitboard(to_idx, to_cell.turn, to_cell.type, to_cell.is_promoted);
	}
	board.add_piece_to_bitboard(to_idx, from_cell.turn, from_cell.type, from_cell.is_promoted);

	if (from_cell.type == PieceType::KING) {
		board.king_pos_[static_cast<int>(from_cell.turn)] = to;
	}

	// 王手放置になる手を除外
	const bool in_check = is_king_in_check(board, from_cell.turn);

	board.board_[from_idx] = from_cell;
	board.board_[to_idx] = to_cell;
	board.king_pos_[static_cast<int>(from_cell.turn)] = old_king_pos;

	board.remove_piece_from_bitboard(to_idx, from_cell.turn, from_cell.type, from_cell.is_promoted);
	if (!to_cell.is_empty()) {
		board.add_piece_to_bitboard(to_idx, to_cell.turn, to_cell.type, to_cell.is_promoted);
	}
	board.add_piece_to_bitboard(from_idx, from_cell.turn, from_cell.type, from_cell.is_promoted);

	return !in_check;
}

bool MoveGenerator::is_legal_drop(BoardState &board, PieceType piece_type, bool is_enemy, Coord to) {
	if (!is_valid_drop(board, piece_type, is_enemy, to)) {
		return false;
	}

	const Turn turn = is_enemy ? Turn::GOTE : Turn::SENTE;
	const int to_idx = to.col * Shogi::BOARD_ROWS + to.row;

	board.board_[to_idx] = Cell(piece_type, turn, false);

	board.add_piece_to_bitboard(to_idx, turn, piece_type, false);

	// 王手放置になる手を除外
	const bool in_check = is_king_in_check(board, turn);

	board.board_[to_idx] = Cell();

	board.remove_piece_from_bitboard(to_idx, turn, piece_type, false);

	if (in_check) {
		return false;
	}

	if (piece_type == PieceType::PAWN && is_uchifuzume(board, turn, to)) {
		return false;
	}

	return true;
}

bool MoveGenerator::is_dead_end(PieceType piece_type, bool is_enemy, int to_row) {
	int relative_row = is_enemy ? (Shogi::BOARD_ROWS - 1 - to_row) : to_row;
	switch (piece_type) {
		case PieceType::PAWN:
		case PieceType::LANCE:
			return relative_row == 0;
		case PieceType::KNIGHT:
			return relative_row <= 1;
		default:
			return false;
	}
}

bool MoveGenerator::is_nifu(const BoardState &board, PieceType piece_type, Turn turn, int col) {
	if (piece_type != PieceType::PAWN) {
		return false;
	}

	return (board.pawn_columns_[static_cast<int>(turn)] & (1 << col)) != 0;
}

bool MoveGenerator::is_uchifuzume(BoardState &board, Turn turn, Coord to) {
	const Turn enemy_turn = (turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
	auto king_position = board.get_king_position(enemy_turn);
	if (!king_position.has_value()) {
		return false;
	}

	const int to_index = to.col * Shogi::BOARD_ROWS + to.row;
	const int king_square = king_position->col * Shogi::BOARD_ROWS + king_position->row;
	if (!AttackTable::get_pawn_attacks(to_index, turn).is_set(king_square)) {
		return false;
	}

	const bool need_null_move = (board.get_turn_to_move() != turn);
	uint64_t null_move_hash = 0;
	if (need_null_move) {
		null_move_hash = board.make_null_move();
	}

	Shogi::Move drop_move(0, 0, to.col, to.row, PieceType::PAWN, false, true, false);
	Shogi::UndoInfo undo = board.apply_move(drop_move);
	Shogi::MoveList responses;
	get_legal_moves(board, responses);
	const bool is_mate = responses.is_empty();
	board.undo_move(undo);

	if (need_null_move) {
		board.undo_null_move(null_move_hash);
	}

	return is_mate;
}

bool MoveGenerator::is_king_in_check(const BoardState &board, Turn turn) {
	auto king_position = board.get_king_position(turn);
	if (!king_position) {
		return false;
	}

	int king_square = king_position->col * Shogi::BOARD_ROWS + king_position->row;

	const Turn enemy_turn = (turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
	int enemy_index = static_cast<int>(enemy_turn);

	const Bitboard &enemy_pawns = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::PAWN)];
	const Bitboard &enemy_lances = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::LANCE)];
	const Bitboard &enemy_knights = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::KNIGHT)];
	const Bitboard &enemy_silvers = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::SILVER)];
	const Bitboard &enemy_golds = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::GOLD)];
	const Bitboard &enemy_bishops = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::BISHOP)];
	const Bitboard &enemy_rooks = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::ROOK)];
	const Bitboard &enemy_kings = board.bitboard_piece_[enemy_index][static_cast<int>(PieceType::KING)];
	const Bitboard &enemy_promoted = board.bitboard_promoted_[enemy_index];
	const Bitboard &occupancy = board.bitboard_all_;

	// 香車の利きをチェック
	Bitboard lance_attacks = AttackTable::get_lance_attacks(king_square, turn, occupancy);
	if (!(lance_attacks & (enemy_lances & ~enemy_promoted)).is_empty()) {
		return true;
	}

	// 角の利きをチェック
	Bitboard bishop_attacks = AttackTable::get_bishop_attacks(king_square, occupancy);
	if (!(bishop_attacks & enemy_bishops).is_empty()) {
		return true;
	}

	// 飛車の利きをチェック
	Bitboard rook_attacks = AttackTable::get_rook_attacks(king_square, occupancy);
	if (!(rook_attacks & enemy_rooks).is_empty()) {
		return true;
	}

	// 歩の利きをチェック
	if (!(AttackTable::get_pawn_attacks(king_square, turn) & (enemy_pawns & ~enemy_promoted)).is_empty()) {
		return true;
	}

	// 桂馬の利きをチェック
	if (!(AttackTable::get_knight_attacks(king_square, turn) & (enemy_knights & ~enemy_promoted)).is_empty()) {
		return true;
	}

	// 銀の利きをチェック
	if (!(AttackTable::get_silver_attacks(king_square, turn) & (enemy_silvers & ~enemy_promoted)).is_empty()) {
		return true;
	}

	// 金および金と同じ動きをする成り駒の利きをチェック
	Bitboard gold_likes = enemy_golds | (enemy_promoted & (enemy_pawns | enemy_lances | enemy_knights | enemy_silvers));
	if (!(AttackTable::get_gold_attacks(king_square, turn) & gold_likes).is_empty()) {
		return true;
	}

	// 玉および竜（斜め1マス）と馬（縦横1マス）の利きをチェック
	Bitboard king_likes = enemy_kings | (enemy_promoted & (enemy_bishops | enemy_rooks));
	if (!(AttackTable::get_king_attacks(king_square) & king_likes).is_empty()) {
		return true;
	}

	return false;
}

void MoveGenerator::get_legal_moves(BoardState &board, Shogi::MoveList &move_list, bool only_captures) {
	move_list.clear();

	const Turn current_turn = board.turn_to_move_;
	const Turn opponent_turn = (current_turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
	const Bitboard my_pieces_bitboard = board.bitboard_side_[static_cast<int>(current_turn)];
	const Bitboard opponent_pieces_bitboard = board.bitboard_side_[static_cast<int>(opponent_turn)];
	const Bitboard occupancy = board.bitboard_all_;
	const int promotion_row_min = (current_turn == Turn::SENTE) ? 0 : 6;
	const int promotion_row_max = (current_turn == Turn::SENTE) ? 2 : 8;
	auto is_promotion_rank = [&](int row) { return (row >= promotion_row_min && row <= promotion_row_max); };
	PinMasks pin_masks = calculate_pin_masks(board, board.turn_to_move_);

	Bitboard checkers = get_checkers(board, current_turn);
	Bitboard evasion_mask;
	if (checkers.is_empty()) {
		// 王手なし
		evasion_mask = ~Bitboard();
	} else if (checkers.count() > 1) {
		// 両王手
		evasion_mask = Bitboard();
	} else {
		// 単独王手
		int checker_index = checkers.lsb();
		evasion_mask.set(checker_index);

		auto king_position = board.get_king_position(current_turn);
		if (king_position.has_value()) {
			int king_square = king_position->col * Shogi::BOARD_ROWS + king_position->row;
			for (int dir = 0; dir < 8; ++dir) {
				Bitboard ray = AttackTable::get_ray(king_square, dir);
				if (ray.is_set(checker_index)) {
					Bitboard between = ray ^ AttackTable::get_ray(checker_index, dir);
					evasion_mask |= between;
					break;
				}
			}
		}
	}

	// 盤上の駒
	for (int piece_type = 0; piece_type < Shogi::PIECE_TYPE_COUNT; ++piece_type) {
		PieceType type = static_cast<PieceType>(piece_type);
		Bitboard pieces = board.bitboard_piece_[static_cast<int>(current_turn)][piece_type];
		Bitboard promoted_pieces = board.bitboard_promoted_[static_cast<int>(current_turn)];

		// 両王手されている場合、玉以外の駒の移動は不可
		if (type != PieceType::KING && checkers.count() > 1) {
			continue;
		}

		while (!pieces.is_empty()) {
			int from_index = pieces.lsb();
			pieces.clear(from_index);

			Bitboard from_bitboard;
			from_bitboard.set(from_index);
			bool is_promoted = !(promoted_pieces & from_bitboard).is_empty();

			Coord from{ from_index / Shogi::BOARD_ROWS, from_index % Shogi::BOARD_ROWS };
			Bitboard attacks;

			// 駒の利き
			if (is_promoted) {
				if (type == PieceType::BISHOP) {
					attacks = AttackTable::get_promoted_bishop_attacks(from_index, occupancy);
				} else if (type == PieceType::ROOK) {
					attacks = AttackTable::get_promoted_rook_attacks(from_index, occupancy);
				} else {
					attacks = AttackTable::get_gold_attacks(from_index, current_turn);
				}
			} else {
				switch (type) {
					case PieceType::PAWN:
						attacks = AttackTable::get_pawn_attacks(from_index, current_turn);
						break;
					case PieceType::LANCE:
						attacks = AttackTable::get_lance_attacks(from_index, current_turn, occupancy);
						break;
					case PieceType::KNIGHT:
						attacks = AttackTable::get_knight_attacks(from_index, current_turn);
						break;
					case PieceType::SILVER:
						attacks = AttackTable::get_silver_attacks(from_index, current_turn);
						break;
					case PieceType::GOLD:
						attacks = AttackTable::get_gold_attacks(from_index, current_turn);
						break;
					case PieceType::BISHOP:
						attacks = AttackTable::get_bishop_attacks(from_index, occupancy);
						break;
					case PieceType::ROOK:
						attacks = AttackTable::get_rook_attacks(from_index, occupancy);
						break;
					case PieceType::KING:
						attacks = AttackTable::get_king_attacks(from_index);
						break;
					default:
						break;
				}
			}

			attacks = attacks & ~my_pieces_bitboard;
			if (type != PieceType::KING) {
				if (!(pin_masks.pinned & from_bitboard).is_empty()) {
					attacks &= pin_masks.valid_ray_masks[from_index];
				}

				attacks &= evasion_mask;
			}

			while (!attacks.is_empty()) {
				int to_index = attacks.lsb();
				attacks.clear(to_index);

				Coord to{ to_index / Shogi::BOARD_ROWS, to_index % Shogi::BOARD_ROWS };

				Bitboard to_bitboard;
				to_bitboard.set(to_index);
				bool is_capture = !(opponent_pieces_bitboard & to_bitboard).is_empty();

				if (only_captures && !is_capture) {
					continue;
				}

				if (type == PieceType::KING) {
					if (!is_legal_move(board, from, to)) {
						continue;
					}
				}

				bool can_promote = false;
				bool must_promote = false;

				// 成り判定
				if (!is_promoted && type != PieceType::KING && type != PieceType::GOLD) {
					if (is_promotion_rank(from.row) || is_promotion_rank(to.row)) {
						can_promote = true;
					}
				}

				if (!is_promoted && is_dead_end(type, current_turn == Turn::GOTE, to.row)) {
					must_promote = true;
				}

				if (!must_promote) {
					move_list.push(Shogi::Move(from.col, from.row, to.col, to.row, type, false, false, is_capture));
				}

				if (can_promote) {
					move_list.push(Shogi::Move(from.col, from.row, to.col, to.row, type, true, false, is_capture));
				}
			}
		}
	}

	// 持ち駒
	if (!only_captures && checkers.count() <= 1) {
		Bitboard empty_cells = ~occupancy;
		Bitboard valid_drop_targets = empty_cells & evasion_mask;

		if (!valid_drop_targets.is_empty()) {
			constexpr std::array<PieceType, 7> HAND_TYPES = { PieceType::PAWN, PieceType::LANCE, PieceType::KNIGHT,
				PieceType::SILVER, PieceType::GOLD, PieceType::BISHOP,
				PieceType::ROOK };

			for (PieceType type : HAND_TYPES) {
				if (board.get_hand_count(current_turn, type) == 0) {
					continue;
				}

				Bitboard target_bitboard = valid_drop_targets;

				while (!target_bitboard.is_empty()) {
					int to_index = target_bitboard.lsb();
					target_bitboard.clear(to_index);

					Coord to{ to_index / Shogi::BOARD_ROWS, to_index % Shogi::BOARD_ROWS };

					if (is_dead_end(type, current_turn == Turn::GOTE, to.row)) {
						continue;
					}
					if (type == PieceType::PAWN && is_nifu(board, type, current_turn, to.col)) {
						continue;
					}
					if (type == PieceType::PAWN && is_uchifuzume(board, current_turn, to)) {
						continue;
					}

					move_list.push(Shogi::Move(0, 0, to.col, to.row, type, false, true, false));
				}
			}
		}
	}
}

Bitboard MoveGenerator::attackers_to(const BoardState &board, int square, Turn side, const Bitboard &occupancy) {
	int side_index = static_cast<int>(side);
	Turn reverse_turn = (side == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;

	const Bitboard &pawns = board.bitboard_piece_[side_index][static_cast<int>(PieceType::PAWN)];
	const Bitboard &lances = board.bitboard_piece_[side_index][static_cast<int>(PieceType::LANCE)];
	const Bitboard &knights = board.bitboard_piece_[side_index][static_cast<int>(PieceType::KNIGHT)];
	const Bitboard &silvers = board.bitboard_piece_[side_index][static_cast<int>(PieceType::SILVER)];
	const Bitboard &golds = board.bitboard_piece_[side_index][static_cast<int>(PieceType::GOLD)];
	const Bitboard &bishops = board.bitboard_piece_[side_index][static_cast<int>(PieceType::BISHOP)];
	const Bitboard &rooks = board.bitboard_piece_[side_index][static_cast<int>(PieceType::ROOK)];
	const Bitboard &kings = board.bitboard_piece_[side_index][static_cast<int>(PieceType::KING)];
	const Bitboard &promoted = board.bitboard_promoted_[side_index];

	Bitboard attackers;
	attackers |= AttackTable::get_pawn_attacks(square, reverse_turn) & (pawns & ~promoted);
	attackers |= AttackTable::get_knight_attacks(square, reverse_turn) & (knights & ~promoted);
	attackers |= AttackTable::get_silver_attacks(square, reverse_turn) & (silvers & ~promoted);

	Bitboard gold_likes = golds | (promoted & (pawns | lances | knights | silvers));
	attackers |= AttackTable::get_gold_attacks(square, reverse_turn) & gold_likes;

	attackers |= AttackTable::get_lance_attacks(square, reverse_turn, occupancy) & (lances & ~promoted);
	attackers |= AttackTable::get_bishop_attacks(square, occupancy) & bishops;
	attackers |= AttackTable::get_rook_attacks(square, occupancy) & rooks;

	Bitboard king_likes = kings | (promoted & (bishops | rooks));
	attackers |= AttackTable::get_king_attacks(square) & king_likes;

	// 取り合いで取り除かれた駒は除外
	return attackers & occupancy;
}

int MoveGenerator::see(const BoardState &board, const Shogi::Move &move) {
	assert(move.is_capture && !move.is_drop);

	const int to_square = move.to_col * Shogi::BOARD_ROWS + move.to_row;
	const int from_square = move.from_col * Shogi::BOARD_ROWS + move.from_row;

	const Cell &victim = board.get_cell({ move.to_col, move.to_row });
	const Cell &first_attacker = board.get_cell({ move.from_col, move.from_row });

	int gain[40];
	int depth = 0;
	gain[0] = Shogi::PIECE_VALUES[static_cast<int>(victim.type)][victim.is_promoted ? 1 : 0];

	// 次に取り返される駒の価値
	int occupant_value = Shogi::PIECE_VALUES[static_cast<int>(first_attacker.type)][first_attacker.is_promoted ? 1 : 0];

	Bitboard occupancy = board.bitboard_all_;
	occupancy.clear(from_square);

	Turn side = (first_attacker.turn == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;

	while (depth < 39) {
		Bitboard attackers = attackers_to(board, to_square, side, occupancy);
		if (attackers.is_empty()) {
			break;
		}

		int cheapest_square = -1;
		int cheapest_value = 99999999;
		Bitboard iter = attackers;
		while (!iter.is_empty()) {
			int sq = iter.lsb();
			iter.clear(sq);
			const Cell &cell = board.get_cell({ sq / Shogi::BOARD_ROWS, sq % Shogi::BOARD_ROWS });
			int value = Shogi::PIECE_VALUES[static_cast<int>(cell.type)][cell.is_promoted ? 1 : 0];
			if (value < cheapest_value) {
				cheapest_value = value;
				cheapest_square = sq;
			}
		}

		// 玉は相手の利きが残るマスへ取り返し不可
		const Cell &cheapest_cell =
				board.get_cell({ cheapest_square / Shogi::BOARD_ROWS, cheapest_square % Shogi::BOARD_ROWS });
		if (cheapest_cell.type == PieceType::KING) {
			Turn opponent = (side == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
			if (!attackers_to(board, to_square, opponent, occupancy).is_empty()) {
				break;
			}
		}

		++depth;
		gain[depth] = occupant_value - gain[depth - 1];

		// 一般的なSEE実装の早期打ち切りは取った駒の背後の飛び駒を見落とすため不採用
		occupant_value = cheapest_value;
		occupancy.clear(cheapest_square);
		side = (side == Turn::SENTE) ? Turn::GOTE : Turn::SENTE;
	}

	// Negamax畳み込み
	for (int i = depth; i >= 1; --i) {
		gain[i - 1] = -std::max(-gain[i - 1], gain[i]);
	}
	return gain[0];
}
