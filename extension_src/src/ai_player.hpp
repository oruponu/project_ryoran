#ifndef AI_PLAYER_HPP
#define AI_PLAYER_HPP

#include "board_state.hpp"
#include "shogi_engine.hpp"
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/ref_counted.hpp>

namespace godot {

class AIPlayer {

  private:
    bool is_enemy_side;

  public:
    explicit AIPlayer(bool p_is_enemy_side) : is_enemy_side(p_is_enemy_side) {}
    ~AIPlayer() {}

    Dictionary get_next_move(Node2D *main_node);
};

} // namespace godot

#endif
