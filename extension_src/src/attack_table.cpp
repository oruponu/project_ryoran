#include "attack_table.hpp"

using Shogi::Coord;
using Shogi::Turn;

void AttackTable::initialize() {
	if (is_initialized_) {
		return;
	}

	auto set_if_valid = [](Bitboard &bitboard, Coord coord) {
		if (coord.is_valid()) {
			bitboard.set(coord.to_index());
		}
	};

	for (int index = 0; index < Shogi::BOARD_SIZE; ++index) {
		auto [col, row] = Coord::from_index(index);

		for (int turn_index = 0; turn_index < 2; ++turn_index) {
			Shogi::Turn turn = static_cast<Shogi::Turn>(turn_index);
			int sign = (turn == Shogi::Turn::SENTE) ? -1 : 1;

			// 歩
			set_if_valid(attacks_pawn_[turn_index][index], { col, row + sign });

			// 桂馬
			set_if_valid(attacks_knight_[turn_index][index], { col - 1, row + sign * 2 });
			set_if_valid(attacks_knight_[turn_index][index], { col + 1, row + sign * 2 });
			// 銀
			set_if_valid(attacks_silver_[turn_index][index], { col - 1, row + sign });
			set_if_valid(attacks_silver_[turn_index][index], { col, row + sign });
			set_if_valid(attacks_silver_[turn_index][index], { col + 1, row + sign });
			set_if_valid(attacks_silver_[turn_index][index], { col - 1, row - sign });
			set_if_valid(attacks_silver_[turn_index][index], { col + 1, row - sign });
			// 金
			set_if_valid(attacks_gold_[turn_index][index], { col - 1, row + sign });
			set_if_valid(attacks_gold_[turn_index][index], { col, row + sign });
			set_if_valid(attacks_gold_[turn_index][index], { col + 1, row + sign });
			set_if_valid(attacks_gold_[turn_index][index], { col - 1, row });
			set_if_valid(attacks_gold_[turn_index][index], { col + 1, row });
			set_if_valid(attacks_gold_[turn_index][index], { col, row - sign });
		}

		// 玉
		for (int diff_col = -1; diff_col <= 1; ++diff_col) {
			for (int diff_row = -1; diff_row <= 1; ++diff_row) {
				if (diff_col == 0 && diff_row == 0) {
					continue;
				}
				set_if_valid(attacks_king_[index], { col + diff_col, row + diff_row });
			}
		}
	}

	// 走り駒
	int diff_cols[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
	int diff_rows[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
	for (int index = 0; index < Shogi::BOARD_SIZE; ++index) {
		auto [col, row] = Coord::from_index(index);

		for (int direction = 0; direction < 8; ++direction) {
			Bitboard ray;
			Coord coord = { col + diff_cols[direction], row + diff_rows[direction] };
			while (coord.is_valid()) {
				ray.set(coord.to_index());
				coord.col += diff_cols[direction];
				coord.row += diff_rows[direction];
			}
			rays_[direction][index] = ray;
		}
	}

	is_initialized_ = true;
}

Bitboard AttackTable::get_lance_attacks(int index, Turn turn, const Bitboard &occupancy) {
	int direction = (turn == Turn::SENTE) ? 0 : 4; // 0: 上，4: 下
	Bitboard ray = rays_[direction][index];
	Bitboard blockers = ray & occupancy;

	if (!blockers.is_empty()) {
		int blocker_index;
		if (turn == Turn::SENTE) {
			blocker_index = blockers.msb();
		} else {
			blocker_index = blockers.lsb();
		}

		Bitboard ray_from_blocker = rays_[direction][blocker_index];
		return ray & ~ray_from_blocker;
	}

	return ray;
}

Bitboard AttackTable::get_bishop_attacks(int index, const Bitboard &occupancy) {
	Bitboard attacks;
	int directions[4] = { 1, 3, 5, 7 }; // 1: 右上，3: 右下，5: 左下，7: 左上

	for (int i = 0; i < 4; ++i) {
		int direction = directions[i];
		Bitboard ray = rays_[direction][index];
		Bitboard blockers = ray & occupancy;

		if (!blockers.is_empty()) {
			int blocker_index;

			// 左下と左上はindexが減る方向
			// 右上と右下はindexが増える方向
			if (direction == 5 || direction == 7) {
				blocker_index = blockers.msb();
			} else {
				blocker_index = blockers.lsb();
			}

			Bitboard ray_from_blocker = rays_[direction][blocker_index];
			attacks |= (ray & ~ray_from_blocker);
		} else {
			attacks |= ray;
		}
	}

	return attacks;
}

Bitboard AttackTable::get_rook_attacks(int index, const Bitboard &occupancy) {
	Bitboard attacks;
	int directions[4] = { 0, 2, 4, 6 }; // 0: 上，2: 右，4: 下，6: 左

	for (int i = 0; i < 4; ++i) {
		int direction = directions[i];
		Bitboard ray = rays_[direction][index];
		Bitboard blockers = ray & occupancy;

		if (!blockers.is_empty()) {
			int blocker_index;

			// 上と左はindexが減る方向
			// 下と右はindexが増える方向
			if (direction == 0 || direction == 6) {
				blocker_index = blockers.msb();
			} else {
				blocker_index = blockers.lsb();
			}

			Bitboard ray_from_blocker = rays_[direction][blocker_index];
			attacks |= (ray & ~ray_from_blocker);
		} else {
			attacks |= ray;
		}
	}

	return attacks;
}
