#ifndef AI_PLAYER_HPP
#define AI_PLAYER_HPP

#include <godot_cpp/classes/ref_counted.hpp>

using namespace godot;

class AIPlayer : public RefCounted {
    GDCLASS(AIPlayer, RefCounted);

protected:
    static void _bind_methods();

public:
    AIPlayer();
    ~AIPlayer();

    int test_calculation(int a, int b);
};

#endif
