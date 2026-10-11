#include "attack_table.hpp"
#include "move_generator.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <utility>

using Shogi::Coord;
using Shogi::PieceType;
using Shogi::Turn;

Bitboard MoveGenerator::get_checkers(const BoardState &board, Turn turn) {
	auto king_position = board.get_king_position(turn);
	if (!king_position.has_value()) {
		return Bitboard();
	}

	return attackers_to(board, king_position->to_index(), Shogi::opponent(turn), board.get_occupancy());
}

PinMasks MoveGenerator::calculate_pin_masks(const BoardState &board, Turn turn) {
	PinMasks pin_masks;

	auto king_position = board.get_king_position(turn);
	if (!king_position.has_value()) {
		return pin_masks;
	}

	int king_index = king_position->to_index();

	Turn opponent_turn = Shogi::opponent(turn);

	const Bitboard &opponent_lances = board.get_pieces(opponent_turn, PieceType::LANCE);
	const Bitboard &opponent_bishops = board.get_pieces(opponent_turn, PieceType::BISHOP);
	const Bitboard &opponent_rooks = board.get_pieces(opponent_turn, PieceType::ROOK);
	const Bitboard &opponent_promoted = board.get_promoted_pieces(opponent_turn);

	Bitboard opponent_line_sliders = opponent_rooks;
	Bitboard opponent_diagonal_sliders = opponent_bishops;

	for (int direction = 0; direction < 8; ++direction) {
		Bitboard sliders;
		if (direction % 2 != 0) {
			sliders = opponent_diagonal_sliders;
		} else {
			sliders = opponent_line_sliders;
			if (direction == 0 && opponent_turn == Turn::GOTE) {
				sliders |= (opponent_lances & ~opponent_promoted);
			} else if (direction == 4 && opponent_turn == Turn::SENTE) {
				sliders |= (opponent_lances & ~opponent_promoted);
			}
		}

		Bitboard ray = AttackTable::get_ray(king_index, direction);
		Bitboard attackers = ray & sliders;
		if (attackers.is_empty()) {
			continue;
		}

		int attacker_index;

		// 最も玉に近い攻撃駒を特定
		if (direction >= 1 && direction <= 4) {
			attacker_index = attackers.lsb();
		} else {
			attacker_index = attackers.msb();
		}

		Bitboard path = ray ^ AttackTable::get_ray(attacker_index, direction);
		path.set(attacker_index);

		Bitboard between = path & board.get_occupancy();
		between.clear(attacker_index);

		if (between.count() == 1) {
			int pinned_index = between.lsb();
			if (board.get_pieces(turn).is_set(pinned_index)) {
				pin_masks.pinned.set(pinned_index);
				pin_masks.valid_ray_masks[pinned_index] = path;
			}
		}
	}

	return pin_masks;
}

bool MoveGenerator::is_dead_end(PieceType piece_type, bool is_gote, int to_row) {
	int relative_row = is_gote ? (Shogi::BOARD_ROWS - 1 - to_row) : to_row;
	using enum PieceType;
	switch (piece_type) {
		case PAWN:
		case LANCE:
			return relative_row == 0;
		case KNIGHT:
			return relative_row <= 1;
		default:
			return false;
	}
}

bool MoveGenerator::can_promote(PieceType piece_type, bool is_promoted, bool is_gote, int from_row, int to_row) {
	if (is_promoted || piece_type == PieceType::KING || piece_type == PieceType::GOLD) {
		return false;
	}

	auto in_zone = [is_gote](int row) {
		const int relative_row = is_gote ? (Shogi::BOARD_ROWS - 1 - row) : row;
		return 0 <= relative_row && relative_row < Shogi::PROMOTION_ZONE_ROWS;
	};
	return in_zone(from_row) || in_zone(to_row);
}

bool MoveGenerator::is_nifu(const BoardState &board, PieceType piece_type, Turn turn, int col) {
	if (piece_type != PieceType::PAWN) {
		return false;
	}

	return board.has_pawn_in_column(turn, col);
}

bool MoveGenerator::is_uchifuzume(BoardState &board, Coord to) {
	const Turn turn = board.get_turn_to_move();
	const Turn opponent_turn = Shogi::opponent(turn);
	auto king_position = board.get_king_position(opponent_turn);
	if (!king_position.has_value()) {
		return false;
	}

	const int to_index = to.to_index();
	const int king_index = king_position->to_index();
	if (!AttackTable::get_pawn_attacks(to_index, turn).is_set(king_index)) {
		return false;
	}

	Shogi::Move drop_move(0, 0, to.col, to.row, PieceType::PAWN, false, true, false);
	Shogi::UndoInfo undo = board.apply_move(drop_move);
	Shogi::MoveList responses;
	get_legal_moves(board, responses);
	const bool is_mate = responses.is_empty();
	board.undo_move(undo);

	return is_mate;
}

bool MoveGenerator::is_king_in_check(const BoardState &board, Turn turn) {
	return !get_checkers(board, turn).is_empty();
}

void MoveGenerator::get_legal_moves(BoardState &board, Shogi::MoveList &move_list, bool only_captures) {
	move_list.clear();

	const Turn turn = board.get_turn_to_move();
	const Turn opponent_turn = Shogi::opponent(turn);
	const Bitboard own_pieces_bitboard = board.get_pieces(turn);
	const Bitboard opponent_pieces_bitboard = board.get_pieces(opponent_turn);
	const Bitboard occupancy = board.get_occupancy();
	PinMasks pin_masks = calculate_pin_masks(board, turn);

	Bitboard checkers = get_checkers(board, turn);
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

		auto king_position = board.get_king_position(turn);
		if (king_position.has_value()) {
			int king_index = king_position->to_index();
			for (int direction = 0; direction < 8; ++direction) {
				Bitboard ray = AttackTable::get_ray(king_index, direction);
				if (ray.is_set(checker_index)) {
					Bitboard between = ray ^ AttackTable::get_ray(checker_index, direction);
					evasion_mask |= between;
					break;
				}
			}
		}
	}

	// 盤上の駒
	for (int piece_type_index = 0; piece_type_index < Shogi::PIECE_TYPE_COUNT; ++piece_type_index) {
		PieceType piece_type = static_cast<PieceType>(piece_type_index);
		Bitboard pieces = board.get_pieces(turn, piece_type);
		Bitboard promoted_pieces = board.get_promoted_pieces(turn);

		// 両王手されている場合、玉以外の駒の移動は不可
		if (piece_type != PieceType::KING && checkers.count() > 1) {
			continue;
		}

		while (!pieces.is_empty()) {
			int from_index = pieces.pop_lsb();

			bool is_promoted = promoted_pieces.is_set(from_index);

			Coord from = Coord::from_index(from_index);
			Bitboard attacks = AttackTable::get_attacks(piece_type, is_promoted, from_index, turn, occupancy) & ~own_pieces_bitboard;
			if (piece_type != PieceType::KING) {
				if (pin_masks.pinned.is_set(from_index)) {
					attacks &= pin_masks.valid_ray_masks[from_index];
				}

				attacks &= evasion_mask;
			}

			while (!attacks.is_empty()) {
				int to_index = attacks.pop_lsb();

				Coord to = Coord::from_index(to_index);

				bool is_capture = opponent_pieces_bitboard.is_set(to_index);

				if (only_captures && !is_capture) {
					continue;
				}

				if (piece_type == PieceType::KING) {
					Bitboard occupancy_without_king = occupancy;
					occupancy_without_king.clear(from_index);
					if (!attackers_to(board, to_index, opponent_turn, occupancy_without_king).is_empty()) {
						continue;
					}
				}

				// 成り判定
				bool promotable = can_promote(piece_type, is_promoted, turn == Turn::GOTE, from.row, to.row);
				bool must_promote = !is_promoted && is_dead_end(piece_type, turn == Turn::GOTE, to.row);

				if (!must_promote) {
					move_list.push(Shogi::Move(from.col, from.row, to.col, to.row, piece_type, false, false, is_capture));
				}

				if (promotable) {
					move_list.push(Shogi::Move(from.col, from.row, to.col, to.row, piece_type, true, false, is_capture));
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

			for (PieceType piece_type : HAND_TYPES) {
				if (board.get_hand_count(turn, piece_type) == 0) {
					continue;
				}

				Bitboard target_bitboard = valid_drop_targets;

				while (!target_bitboard.is_empty()) {
					int to_index = target_bitboard.pop_lsb();

					Coord to = Coord::from_index(to_index);

					if (is_dead_end(piece_type, turn == Turn::GOTE, to.row)) {
						continue;
					}
					if (piece_type == PieceType::PAWN && is_nifu(board, piece_type, turn, to.col)) {
						continue;
					}
					if (piece_type == PieceType::PAWN && is_uchifuzume(board, to)) {
						continue;
					}

					move_list.push(Shogi::Move(0, 0, to.col, to.row, piece_type, false, true, false));
				}
			}
		}
	}
}

Bitboard MoveGenerator::attackers_to(const BoardState &board, int index, Turn turn, const Bitboard &occupancy) {
	Turn opponent_turn = Shogi::opponent(turn);

	const Bitboard &pawns = board.get_pieces(turn, PieceType::PAWN);
	const Bitboard &lances = board.get_pieces(turn, PieceType::LANCE);
	const Bitboard &knights = board.get_pieces(turn, PieceType::KNIGHT);
	const Bitboard &silvers = board.get_pieces(turn, PieceType::SILVER);
	const Bitboard &golds = board.get_pieces(turn, PieceType::GOLD);
	const Bitboard &bishops = board.get_pieces(turn, PieceType::BISHOP);
	const Bitboard &rooks = board.get_pieces(turn, PieceType::ROOK);
	const Bitboard &kings = board.get_pieces(turn, PieceType::KING);
	const Bitboard &promoted = board.get_promoted_pieces(turn);

	Bitboard attackers;
	attackers |= AttackTable::get_pawn_attacks(index, opponent_turn) & (pawns & ~promoted);
	attackers |= AttackTable::get_knight_attacks(index, opponent_turn) & (knights & ~promoted);
	attackers |= AttackTable::get_silver_attacks(index, opponent_turn) & (silvers & ~promoted);

	Bitboard gold_likes = golds | (promoted & (pawns | lances | knights | silvers));
	attackers |= AttackTable::get_gold_attacks(index, opponent_turn) & gold_likes;

	attackers |= AttackTable::get_lance_attacks(index, opponent_turn, occupancy) & (lances & ~promoted);
	attackers |= AttackTable::get_bishop_attacks(index, occupancy) & bishops;
	attackers |= AttackTable::get_rook_attacks(index, occupancy) & rooks;

	Bitboard king_likes = kings | (promoted & (bishops | rooks));
	attackers |= AttackTable::get_king_attacks(index) & king_likes;

	// 取り合いで取り除かれた駒は除外
	return attackers & occupancy;
}

int MoveGenerator::see(const BoardState &board, const Shogi::Move &move) {
	assert(move.is_capture && !move.is_drop);

	const int to_index = Shogi::to_index(move.to_col, move.to_row);
	const int from_index = Shogi::to_index(move.from_col, move.from_row);

	const Cell &victim = board.get_cell({ move.to_col, move.to_row });
	const Cell &first_attacker = board.get_cell({ move.from_col, move.from_row });

	std::array<int, 40> gain;
	int depth = 0;
	gain[0] = Shogi::PIECE_VALUES[std::to_underlying(victim.piece_type)][victim.is_promoted ? 1 : 0];

	// 次に取り返される駒の価値
	int occupant_value = Shogi::PIECE_VALUES[std::to_underlying(first_attacker.piece_type)][first_attacker.is_promoted ? 1 : 0];

	Bitboard occupancy = board.get_occupancy();
	occupancy.clear(from_index);

	Turn turn = Shogi::opponent(first_attacker.turn);

	while (depth < std::ssize(gain) - 1) {
		Bitboard attackers = attackers_to(board, to_index, turn, occupancy);
		if (attackers.is_empty()) {
			break;
		}

		int cheapest_index = -1;
		int cheapest_value = std::numeric_limits<int>::max();
		Bitboard iter = attackers;
		while (!iter.is_empty()) {
			int index = iter.pop_lsb();
			const Cell &cell = board.get_cell(Coord::from_index(index));
			int value = Shogi::PIECE_VALUES[std::to_underlying(cell.piece_type)][cell.is_promoted ? 1 : 0];
			if (value < cheapest_value) {
				cheapest_value = value;
				cheapest_index = index;
			}
		}

		// 玉は相手の利きが残るマスへ取り返し不可
		const Cell &cheapest_cell =
				board.get_cell(Coord::from_index(cheapest_index));
		if (cheapest_cell.piece_type == PieceType::KING) {
			Turn opponent_turn = Shogi::opponent(turn);
			if (!attackers_to(board, to_index, opponent_turn, occupancy).is_empty()) {
				break;
			}
		}

		++depth;
		gain[depth] = occupant_value - gain[depth - 1];

		// 一般的なSEE実装の早期打ち切りは取った駒の背後の飛び駒を見落とすため不採用
		occupant_value = cheapest_value;
		occupancy.clear(cheapest_index);
		turn = Shogi::opponent(turn);
	}

	// Negamax畳み込み
	for (int i = depth; i >= 1; --i) {
		gain[i - 1] = -std::max(-gain[i - 1], gain[i]);
	}
	return gain[0];
}
