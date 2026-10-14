#include "usi_protocol.hpp"
#include "move_generator.hpp"
#include <format>
#include <string>
#include <utility>

namespace Usi {

namespace {

constexpr std::string_view PIECE_CHARS = "KRBGSNLP";

std::string format_square(int col, int row) {
	return std::format("{}{}", 9 - col, static_cast<char>('a' + row));
}

} // namespace

std::string format_move(const Shogi::Move &move) {
	if (move.is_drop) {
		return std::format("{}*{}", PIECE_CHARS[std::to_underlying(move.piece_type)], format_square(move.to_col, move.to_row));
	}
	return std::format("{}{}{}", format_square(move.from_col, move.from_row), format_square(move.to_col, move.to_row),
			move.is_promotion ? "+" : "");
}

std::optional<Shogi::Move> parse_move(BoardState &board, std::string_view text) {
	Shogi::MoveList move_list;
	MoveGenerator::get_legal_moves(board, move_list);
	for (int i = 0; i < move_list.count; ++i) {
		if (format_move(move_list[i]) == text) {
			return move_list[i];
		}
	}
	return std::nullopt;
}

} // namespace Usi
