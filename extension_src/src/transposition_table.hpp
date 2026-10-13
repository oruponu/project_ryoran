#pragma once

#include <cstdint>

enum class TTFlag : uint8_t {
	EXACT,
	LOWER_BOUND,
	UPPER_BOUND
};
