#include "attack_table.hpp"

using Shogi::Coord;
using Shogi::Turn;

void AttackTable::initialize() {
	if (initialized_) {
		return;
	}

	auto set_if_valid = [](Bitboard &bitboard, Coord coord) {
		if (coord.is_valid()) {
			bitboard.set(coord.col * Shogi::BOARD_ROWS + coord.row);
		}
	};

	for (int index = 0; index < Shogi::BOARD_SIZE; ++index) {
		int col = index / Shogi::BOARD_ROWS;
		int row = index % Shogi::BOARD_ROWS;

		for (int side = 0; side < 2; ++side) {
			Shogi::Turn turn = static_cast<Shogi::Turn>(side);
			int sign = (turn == Shogi::Turn::SENTE) ? -1 : 1;

			// 歩
			set_if_valid(attacks_pawn_[side][index], { col, row + sign });

			// 桂馬
			set_if_valid(attacks_knight_[side][index], { col - 1, row + sign * 2 });
			set_if_valid(attacks_knight_[side][index], { col + 1, row + sign * 2 });
			// 銀
			set_if_valid(attacks_silver_[side][index], { col - 1, row + sign });
			set_if_valid(attacks_silver_[side][index], { col, row + sign });
			set_if_valid(attacks_silver_[side][index], { col + 1, row + sign });
			set_if_valid(attacks_silver_[side][index], { col - 1, row - sign });
			set_if_valid(attacks_silver_[side][index], { col + 1, row - sign });
			// 金
			set_if_valid(attacks_gold_[side][index], { col - 1, row + sign });
			set_if_valid(attacks_gold_[side][index], { col, row + sign });
			set_if_valid(attacks_gold_[side][index], { col + 1, row + sign });
			set_if_valid(attacks_gold_[side][index], { col - 1, row });
			set_if_valid(attacks_gold_[side][index], { col + 1, row });
			set_if_valid(attacks_gold_[side][index], { col, row - sign });
		}

		// 玉
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dy = -1; dy <= 1; ++dy) {
				if (dx == 0 && dy == 0) {
					continue;
				}
				set_if_valid(attacks_king_[index], { col + dx, row + dy });
			}
		}
	}

	// 走り駒
	int dxs[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
	int dys[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
	for (int index = 0; index < Shogi::BOARD_SIZE; ++index) {
		int col = index / Shogi::BOARD_ROWS;
		int row = index % Shogi::BOARD_ROWS;

		for (int dir = 0; dir < 8; ++dir) {
			Bitboard ray;
			Coord coord = { col + dxs[dir], row + dys[dir] };
			while (coord.is_valid()) {
				ray.set(coord.col * Shogi::BOARD_ROWS + coord.row);
				coord.col += dxs[dir];
				coord.row += dys[dir];
			}
			rays_[dir][index] = ray;
		}
	}

	initialized_ = true;
}

Bitboard AttackTable::get_lance_attacks(int square, Turn turn, const Bitboard &occupancy) {
	int direction = (turn == Turn::SENTE) ? 0 : 4; // 0: 上，4: 下
	Bitboard ray = rays_[direction][square];
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

Bitboard AttackTable::get_bishop_attacks(int square, const Bitboard &occupancy) {
	Bitboard attacks;
	int directions[4] = { 1, 3, 5, 7 }; // 1: 右上，3: 右下，5: 左下，7: 左上

	for (int dir = 0; dir < 4; ++dir) {
		int direction = directions[dir];
		Bitboard ray = rays_[direction][square];
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

Bitboard AttackTable::get_rook_attacks(int square, const Bitboard &occupancy) {
	Bitboard attacks;
	int directions[4] = { 0, 2, 4, 6 }; // 0: 上，2: 右，4: 下，6: 左

	for (int dir = 0; dir < 4; ++dir) {
		int direction = directions[dir];
		Bitboard ray = rays_[direction][square];
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
