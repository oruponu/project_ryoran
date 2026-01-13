#include "ai_player.hpp"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

AIPlayer::AIPlayer() {}
AIPlayer::~AIPlayer() {}

int AIPlayer::text_calculation(int a, int b) {
    return a + b;
}

void AIPlayer::_bind_methods() {
    ClassDB::bind_method(D_METHOD("text_calculation", "a", "b"), &AIPlayer::text_calculation);
}
