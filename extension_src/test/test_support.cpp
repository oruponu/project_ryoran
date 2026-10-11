#include "move_generator.hpp"
#include "test_support.hpp"
#include <format>
#include <fstream>
#include <iterator>
#include <print>
#include <string>
#include <utility>

namespace {

int g_failures = 0;

const char *PIECE_NAMES[] = { "K", "R", "B", "G", "S", "N", "L", "P" };

void dump_board(const BoardState &board) {
	for (int row = 0; row < 9; ++row) {
		std::string line;
		for (int file = 9; file >= 1; --file) {
			const Cell &cell = board.get_cell({ file - 1, row });
			if (cell.is_empty()) {
				line += " . ";
			} else {
				char symbol = PIECE_NAMES[std::to_underlying(cell.piece_type)][0];
				if (cell.turn == Turn::GOTE) {
					symbol = static_cast<char>(symbol - 'A' + 'a');
				}
				line += std::format("{}{} ", symbol, cell.is_promoted ? '+' : ' ');
			}
		}
		std::println("{}  | rank {}", line, row + 1);
	}
	for (Turn turn : { Turn::SENTE, Turn::GOTE }) {
		std::print("{} hand: ", turn == Turn::SENTE ? "SENTE" : "GOTE ");
		for (int piece_type_index = 0; piece_type_index < Shogi::PIECE_TYPE_COUNT; ++piece_type_index) {
			int count = board.get_hand_count(turn, static_cast<PieceType>(piece_type_index));
			if (count > 0) {
				std::print("{} x{}  ", PIECE_NAMES[piece_type_index], count);
			}
		}
		std::print("\n");
	}
}

} // namespace

void check(bool condition, const char *name) {
	std::println("{}: {}", condition ? "PASS" : "FAIL", name);
	if (!condition) {
		++g_failures;
	}
}

int failure_count() {
	return g_failures;
}

std::vector<uint8_t> read_file(const char *path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return {};
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

// 60手目の局面に先手の詰みがある対局の棋譜（69手で先手勝ち）
extern const KMove KIFU2[] = {
	{ 2, 7, 2, 6, P, false }, // 1 ▲２六歩
	{ 8, 3, 8, 4, P, false }, // 2 △８四歩
	{ 7, 7, 7, 6, P, false }, // 3 ▲７六歩
	{ 8, 4, 8, 5, P, false }, // 4 △８五歩
	{ 8, 8, 7, 7, B, false }, // 5 ▲７七角
	{ 3, 3, 3, 4, P, false }, // 6 △３四歩
	{ 7, 9, 8, 8, S, false }, // 7 ▲８八銀
	{ 2, 2, 7, 7, B, true }, // 8 △７七角成
	{ 8, 8, 7, 7, S, false }, // 9 ▲７七銀
	{ 5, 1, 6, 2, K, false }, // 10 △６二玉
	{ 0, 0, 5, 5, B, false }, // 11 ▲５五角打
	{ 0, 0, 3, 3, B, false }, // 12 △３三角打
	{ 5, 5, 3, 3, B, true }, // 13 ▲３三角成
	{ 2, 1, 3, 3, N, false }, // 14 △３三桂
	{ 4, 9, 5, 8, G, false }, // 15 ▲５八金
	{ 6, 2, 7, 2, K, false }, // 16 △７二玉
	{ 3, 9, 3, 8, S, false }, // 17 ▲３八銀
	{ 3, 3, 4, 5, N, false }, // 18 △４五桂
	{ 6, 9, 6, 8, G, false }, // 19 ▲６八金
	{ 3, 1, 3, 2, S, false }, // 20 △３二銀
	{ 4, 7, 4, 6, P, false }, // 21 ▲４六歩
	{ 0, 0, 3, 9, B, false }, // 22 △３九角打
	{ 2, 8, 2, 7, R, false }, // 23 ▲２七飛
	{ 4, 5, 5, 7, N, true }, // 24 △５七桂成
	{ 6, 8, 5, 7, G, false }, // 25 ▲５七金
	{ 8, 5, 8, 6, P, false }, // 26 △８六歩
	{ 7, 7, 8, 6, S, false }, // 27 ▲８六銀
	{ 0, 0, 8, 8, P, false }, // 28 △８八歩打
	{ 8, 9, 7, 7, N, false }, // 29 ▲７七桂
	{ 8, 8, 8, 9, P, true }, // 30 △８九歩成
	{ 5, 9, 4, 9, K, false }, // 31 ▲４九玉
	{ 3, 9, 5, 7, B, true }, // 32 △５七角成
	{ 5, 8, 5, 7, G, false }, // 33 ▲５七金
	{ 8, 9, 9, 9, P, false }, // 34 △９九と
	{ 0, 0, 7, 5, N, false }, // 35 ▲７五桂打
	{ 0, 0, 5, 4, L, false }, // 36 △５四香打
	{ 3, 7, 3, 6, P, false }, // 37 ▲３六歩
	{ 5, 4, 5, 7, L, true }, // 38 △５七香成
	{ 2, 7, 5, 7, R, false }, // 39 ▲５七飛
	{ 0, 0, 6, 8, G, false }, // 40 △６八金打
	{ 5, 7, 5, 3, R, true }, // 41 ▲５三飛成
	{ 0, 0, 5, 8, G, false }, // 42 △５八金打
	{ 4, 9, 3, 9, K, false }, // 43 ▲３九玉
	{ 7, 1, 6, 2, S, false }, // 44 △６二銀
	{ 5, 3, 5, 4, R, false }, // 45 ▲５四龍
	{ 7, 3, 7, 4, P, false }, // 46 △７四歩
	{ 5, 4, 7, 4, R, false }, // 47 ▲７四龍
	{ 6, 2, 7, 3, S, false }, // 48 △７三銀
	{ 7, 5, 6, 3, N, true }, // 49 ▲６三桂成
	{ 7, 2, 7, 1, K, false }, // 50 △７一玉
	{ 6, 3, 7, 3, N, false }, // 51 ▲７三成桂
	{ 8, 1, 7, 3, N, false }, // 52 △７三桂
	{ 7, 4, 7, 3, R, false }, // 53 ▲７三龍
	{ 6, 1, 7, 2, G, false }, // 54 △７二金
	{ 0, 0, 5, 3, B, false }, // 55 ▲５三角打
	{ 7, 1, 8, 1, K, false }, // 56 △８一玉
	{ 0, 0, 4, 5, B, false }, // 57 ▲４五角打
	{ 0, 0, 5, 4, N, false }, // 58 △５四桂打
	{ 4, 5, 5, 4, B, false }, // 59 ▲５四角
	{ 5, 8, 4, 9, G, false }, // 60 △４九金
	{ 3, 9, 4, 9, K, false }, // 61 ▲４九玉
	{ 6, 8, 5, 9, G, false }, // 62 △５九金
	{ 4, 9, 5, 9, K, false }, // 63 ▲５九玉
	{ 8, 1, 9, 2, K, false }, // 64 △９二玉
	{ 5, 4, 7, 2, B, true }, // 65 ▲７二角成
	{ 8, 2, 7, 2, R, false }, // 66 △７二飛
	{ 7, 3, 7, 2, R, false }, // 67 ▲７二龍
	{ 0, 0, 8, 2, B, false }, // 68 △８二角打
	{ 0, 0, 8, 3, G, false }, // 69 ▲８三金打
};

void setup_initial_position(BoardState &board) {
	auto put = [&](int file, int rank, PieceType piece_type, Turn turn) {
		board.set_cell({ file - 1, rank - 1 }, piece_type, turn, false);
	};
	for (int file = 1; file <= 9; ++file) {
		put(file, 7, P, Turn::SENTE);
		put(file, 3, P, Turn::GOTE);
	}
	put(1, 9, L, Turn::SENTE);
	put(9, 9, L, Turn::SENTE);
	put(2, 9, N, Turn::SENTE);
	put(8, 9, N, Turn::SENTE);
	put(3, 9, S, Turn::SENTE);
	put(7, 9, S, Turn::SENTE);
	put(4, 9, G, Turn::SENTE);
	put(6, 9, G, Turn::SENTE);
	put(5, 9, K, Turn::SENTE);
	put(2, 8, R, Turn::SENTE);
	put(8, 8, B, Turn::SENTE);
	put(1, 1, L, Turn::GOTE);
	put(9, 1, L, Turn::GOTE);
	put(2, 1, N, Turn::GOTE);
	put(8, 1, N, Turn::GOTE);
	put(3, 1, S, Turn::GOTE);
	put(7, 1, S, Turn::GOTE);
	put(4, 1, G, Turn::GOTE);
	put(6, 1, G, Turn::GOTE);
	put(5, 1, K, Turn::GOTE);
	put(8, 2, R, Turn::GOTE);
	put(2, 2, B, Turn::GOTE);
}

bool replay(BoardState &board, int move_count, const KMove *kifu) {
	for (int i = 0; i < move_count; ++i) {
		const KMove &kifu_move = kifu[i];
		Shogi::MoveList move_list;
		MoveGenerator::get_legal_moves(board, move_list);
		const Move *found = nullptr;
		for (const Move &move : move_list) {
			if (kifu_move.from_file == 0) {
				if (move.is_drop && move.piece_type == kifu_move.piece_type && move.to_col == kifu_move.to_file - 1 && move.to_row == kifu_move.to_rank - 1) {
					found = &move;
					break;
				}
			} else {
				if (!move.is_drop && move.from_col == kifu_move.from_file - 1 && move.from_row == kifu_move.from_rank - 1 && move.to_col == kifu_move.to_file - 1 &&
						move.to_row == kifu_move.to_rank - 1 && move.is_promotion == kifu_move.is_promotion) {
					found = &move;
					break;
				}
			}
		}
		if (found == nullptr) {
			std::println("FAIL: move {} not found in legal moves (file={} rank={} -> file={} rank={})", i + 1, kifu_move.from_file,
					kifu_move.from_rank, kifu_move.to_file, kifu_move.to_rank);
			dump_board(board);
			return false;
		}
		static_cast<void>(board.apply_move(*found));
	}
	return true;
}
