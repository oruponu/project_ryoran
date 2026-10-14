#pragma once

#include "board_state.hpp"
#include "shogi_utils.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace Usi {

[[nodiscard]] std::string format_move(const Shogi::Move &move);
[[nodiscard]] std::optional<Shogi::Move> parse_move(BoardState &board, std::string_view text);

} // namespace Usi
