#include "evaluator.hpp"
#include "attack_table.hpp"
#include "board_state.hpp"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

using Shogi::PieceType;
using Shogi::Turn;

int32_t Evaluator::sente_king_table_[Shogi::BOARD_SIZE][Shogi::BOARD_SIZE][3][3][OWNER_COUNT];
int32_t Evaluator::gote_king_table_[Shogi::BOARD_SIZE][Shogi::BOARD_SIZE][3][3][OWNER_COUNT];
int32_t Evaluator::piece_table_[Shogi::BOARD_SIZE][3][3][PIECE_STATE_COUNT];

void Evaluator::initialize() {
	if (is_initialized_) {
		return;
	}

	for (int king_index = 0; king_index < Shogi::BOARD_SIZE; ++king_index) {
		for (int index = 0; index < Shogi::BOARD_SIZE; ++index) {
			int king_distance = distance(king_index, index);
			defense_weight_table_[king_index][index] = KING_DEFENSE_WEIGHTS[king_distance];
			threat_weight_table_[king_index][index] = KING_THREAT_WEIGHTS[king_distance];
		}
	}

	for (int count = 0; count < 11; ++count) {
		multi_effect_weight_table_[count] =
				count == 0 ? 0 : static_cast<int>(6365 - std::pow(0.8525, count - 1) * 5341);
	}

	std::vector<std::vector<std::vector<long long>>> defense_effect_value(
			Shogi::BOARD_SIZE, std::vector<std::vector<long long>>(Shogi::BOARD_SIZE, std::vector<long long>(3)));
	std::vector<std::vector<std::vector<long long>>> threat_effect_value(
			Shogi::BOARD_SIZE, std::vector<std::vector<long long>>(Shogi::BOARD_SIZE, std::vector<long long>(3)));
	for (int king_index = 0; king_index < Shogi::BOARD_SIZE; ++king_index) {
		for (int index = 0; index < Shogi::BOARD_SIZE; ++index) {
			int direction = get_relative_direction(king_index, index);
			for (int effect_count = 0; effect_count < 3; ++effect_count) {
				long long multi_effect = multi_effect_weight_table_[effect_count];
				defense_effect_value[king_index][index][effect_count] =
						(multi_effect * defense_weight_table_[king_index][index] * DEFENSE_DIRECTION_WEIGHT[direction]) /
						(1024LL * 1024 * 1024);
				threat_effect_value[king_index][index][effect_count] =
						(multi_effect * threat_weight_table_[king_index][index] * THREAT_DIRECTION_WEIGHT[direction]) /
						(1024LL * 1024 * 1024);
			}
		}
	}

	const int support_weights[3] = { 0, 33, 43 };
	const int attack_weights[3] = { 0, 113, 122 };

	for (int king_index = 0; king_index < Shogi::BOARD_SIZE; ++king_index) {
		int inv_king_index = Shogi::BOARD_SIZE - 1 - king_index;
		for (int index = 0; index < Shogi::BOARD_SIZE; ++index) {
			bool is_near_king = (distance(index, king_index) == 1);
			int inv_index = Shogi::BOARD_SIZE - 1 - index;
			for (int sente_effect_count = 0; sente_effect_count < 3; ++sente_effect_count) {
				for (int gote_effect_count = 0; gote_effect_count < 3; ++gote_effect_count) {
					long long sente_king_score = defense_effect_value[king_index][index][sente_effect_count] -
							threat_effect_value[king_index][index][gote_effect_count];
					long long gote_king_score = -defense_effect_value[inv_king_index][inv_index][gote_effect_count] +
							threat_effect_value[inv_king_index][inv_index][sente_effect_count];
					for (int owner_index = 0; owner_index < OWNER_COUNT; ++owner_index) {
						int sente_king_bonus = 0;
						int gote_king_bonus = 0;
						if (is_near_king) {
							if (sente_effect_count <= 1) {
								sente_king_bonus = (owner_index == OWNER_SENTE) ? 20 : -11;
							} else if (owner_index == OWNER_SENTE) {
								sente_king_bonus = 11;
							}
							if (gote_effect_count <= 1) {
								gote_king_bonus = (owner_index == OWNER_GOTE) ? -20 : 11;
							} else if (owner_index == OWNER_GOTE) {
								gote_king_bonus = -11;
							}
						}
						sente_king_table_[king_index][index][sente_effect_count][gote_effect_count][owner_index] =
								static_cast<int32_t>((sente_king_score + sente_king_bonus) * TABLE_SCALE);
						gote_king_table_[king_index][index][sente_effect_count][gote_effect_count][owner_index] =
								static_cast<int32_t>((gote_king_score + gote_king_bonus) * TABLE_SCALE);
					}
				}
			}
		}
	}

	for (int index = 0; index < Shogi::BOARD_SIZE; ++index) {
		auto [col, row] = Shogi::Coord::from_index(index);
		int sente_position_bonus_index = (Shogi::BOARD_COLS - 1 - col) + row * Shogi::BOARD_COLS;

		int inv_col = Shogi::BOARD_COLS - 1 - col;
		int inv_row = Shogi::BOARD_ROWS - 1 - row;
		int gote_position_bonus_index = (Shogi::BOARD_COLS - 1 - inv_col) + inv_row * Shogi::BOARD_COLS;

		for (int sente_effect_count = 0; sente_effect_count < 3; ++sente_effect_count) {
			for (int gote_effect_count = 0; gote_effect_count < 3; ++gote_effect_count) {
				piece_table_[index][sente_effect_count][gote_effect_count][0] = 0;
				for (int piece_state_index = 1; piece_state_index < PIECE_STATE_COUNT; ++piece_state_index) {
					int piece_state = piece_state_index - 1;
					bool is_gote = (piece_state >= 16);
					piece_state %= 16;
					bool is_promoted = (piece_state >= 8);
					Shogi::PieceType piece_type = static_cast<Shogi::PieceType>(piece_state % 8);

					int score = 0;
					if (piece_type == Shogi::PieceType::KING) {
						if (!is_gote) {
							score = KING_POSITION_BONUS[sente_position_bonus_index] * TABLE_SCALE;
						} else {
							score = -KING_POSITION_BONUS[gote_position_bonus_index] * TABLE_SCALE;
						}
					} else {
						int piece_value = Shogi::PIECE_VALUES[std::to_underlying(piece_type)][is_promoted ? 1 : 0];
						if (!is_gote) {
							score = piece_value * support_weights[sente_effect_count] -
									piece_value * attack_weights[gote_effect_count];
						} else {
							score = -piece_value * support_weights[gote_effect_count] +
									piece_value * attack_weights[sente_effect_count];
						}
					}
					piece_table_[index][sente_effect_count][gote_effect_count][piece_state_index] = score;
				}
			}
		}
	}

	is_initialized_ = true;
}

int Evaluator::get_piece_state_index(const Cell &cell) {
	if (cell.is_empty()) {
		return 0;
	}

	int piece_type_index = std::to_underlying(cell.piece_type);
	int promoted_offset = cell.is_promoted ? 8 : 0;
	int turn_offset = (cell.turn == Turn::GOTE) ? 16 : 0;
	return 1 + piece_type_index + promoted_offset + turn_offset;
}

int Evaluator::get_owner_index(int piece_state_index) {
	return (piece_state_index + 15) / 16;
}

int Evaluator::get_relative_direction(int from_index, int to_index) {
	auto [from_col, from_row] = Shogi::Coord::from_index(from_index);
	auto [to_col, to_row] = Shogi::Coord::from_index(to_index);

	int diff_col = to_col - from_col;
	int diff_row = to_row - from_row;

	if (diff_col > 0) {
		diff_col = -diff_col;
	}

	if (diff_col == 0 && diff_row == 0) {
		// 同じマス
		return 9;
	}
	if (diff_col == 0 && diff_row < 0) {
		// 北
		return 0;
	}
	if (diff_col > diff_row && diff_row < 0) {
		// 北北東
		return 1;
	}
	if (diff_col == diff_row && diff_row < 0) {
		// 北東
		return 2;
	}
	if (diff_col < diff_row && diff_row < 0) {
		// 東北東
		return 3;
	}
	if (diff_col < 0 && diff_row == 0) {
		// 東
		return 4;
	}
	if (diff_col < -diff_row && diff_row > 0) {
		// 東南東
		return 5;
	}
	if (diff_col == -diff_row && diff_row > 0) {
		// 南東
		return 6;
	}
	if (diff_col > -diff_row && diff_row > 0) {
		// 南南東
		return 7;
	}
	if (diff_col == 0 && diff_row > 0) {
		// 南
		return 8;
	}

	return 9;
}

int Evaluator::calculate_score(const BoardState &board) {
	int score = 0;

	// 盤上の駒
	for (int col = 0; col < Shogi::BOARD_COLS; ++col) {
		for (int row = 0; row < Shogi::BOARD_ROWS; ++row) {
			const Cell &cell = board.get_cell({ col, row });
			if (cell.is_empty()) {
				continue;
			}

			int piece_score = Shogi::get_piece_score(cell.piece_type, cell.is_promoted);
			piece_score = Shogi::apply_board_discount(piece_score);

			if (cell.turn == Turn::SENTE) {
				score += piece_score;
			} else {
				score -= piece_score;
			}
		}
	}

	// 持ち駒
	for (Turn turn : { Turn::SENTE, Turn::GOTE }) {
		int sign = (turn == Turn::SENTE) ? 1 : -1;
		for (int piece_type_index = 0; piece_type_index < Shogi::PIECE_TYPE_COUNT; ++piece_type_index) {
			int count = board.get_hand_count(turn, static_cast<PieceType>(piece_type_index));
			if (count > 0) {
				int value = Shogi::PIECE_VALUES[piece_type_index][0];
				score += count * value * sign;
			}
		}
	}

	return score;
}

int Evaluator::calculate_spatial_score(const BoardState &board) {
	auto sente_king_position = board.get_king_position(Turn::SENTE);
	auto gote_king_position = board.get_king_position(Turn::GOTE);

	if (!sente_king_position.has_value() || !gote_king_position.has_value()) {
		return 0;
	}

	int sente_king_index = sente_king_position->to_index();
	int gote_king_index = gote_king_position->to_index();

	Bitboard attacks_any[2];
	Bitboard attacks_multi[2];

	const Bitboard &occupancy = board.get_occupancy();

	for (int turn_index = 0; turn_index < 2; ++turn_index) {
		Shogi::Turn turn = static_cast<Shogi::Turn>(turn_index);
		const Bitboard promoted_pieces = board.get_promoted_pieces(turn);
		for (int piece_type_index = 0; piece_type_index < Shogi::PIECE_TYPE_COUNT; ++piece_type_index) {
			Shogi::PieceType piece_type = static_cast<Shogi::PieceType>(piece_type_index);
			Bitboard pieces = board.get_pieces(turn, piece_type);
			while (!pieces.is_empty()) {
				int from_index = pieces.pop_lsb();

				bool is_promoted = promoted_pieces.is_set(from_index);
				Bitboard attacks = AttackTable::get_attacks(piece_type, is_promoted, from_index, turn, occupancy);

				attacks_multi[turn_index] |= (attacks_any[turn_index] & attacks);
				attacks_any[turn_index] |= attacks;
			}
		}
	}

	const auto &sente_king_scores = sente_king_table_[sente_king_index];
	const auto &gote_king_scores = gote_king_table_[gote_king_index];
	int score = 0;
	auto accumulate_squares = [&](int first_index, int square_count, uint64_t sente_any, uint64_t sente_multi,
									  uint64_t gote_any, uint64_t gote_multi) {
		for (int bit = 0; bit < square_count; ++bit) {
			const int index = first_index + bit;
			const int sente_attack_count = static_cast<int>((sente_any >> bit) & 1) + static_cast<int>((sente_multi >> bit) & 1);
			const int gote_attack_count = static_cast<int>((gote_any >> bit) & 1) + static_cast<int>((gote_multi >> bit) & 1);

			int piece_state_index = get_piece_state_index(board.get_cell(index));
			int owner_index = get_owner_index(piece_state_index);
			int total = sente_king_scores[index][sente_attack_count][gote_attack_count][owner_index] +
					gote_king_scores[index][sente_attack_count][gote_attack_count][owner_index] +
					piece_table_[index][sente_attack_count][gote_attack_count][piece_state_index];
			score += total / (TABLE_SCALE / EVAL_SCALE_FACTOR);
		}
	};
	accumulate_squares(0, 64, attacks_any[0].lower(), attacks_multi[0].lower(), attacks_any[1].lower(),
			attacks_multi[1].lower());
	accumulate_squares(64, Shogi::BOARD_SIZE - 64, attacks_any[0].upper(), attacks_multi[0].upper(),
			attacks_any[1].upper(), attacks_multi[1].upper());

	return score / EVAL_SCALE_FACTOR;
}
