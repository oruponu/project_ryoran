#pragma once

#include "board_state.hpp"
#include "shogi_utils.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Usi {

inline constexpr std::string_view STARTPOS_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

struct Position {
	BoardState board;
	std::vector<uint64_t> history_hashes;
	std::vector<bool> history_in_checks;
};

[[nodiscard]] std::vector<std::string_view> split_tokens(std::string_view text);
[[nodiscard]] std::string format_move(const Shogi::Move &move);
[[nodiscard]] std::optional<Shogi::Move> parse_move(BoardState &board, std::string_view text);
[[nodiscard]] std::optional<Position> parse_position(std::string_view args);

} // namespace Usi
