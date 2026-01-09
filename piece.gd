extends Area2D


var is_held = false


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(_delta: float) -> void:
	if is_held:
		global_position = get_global_mouse_position()


func _on_input_event(_viewport: Node, event: InputEvent, _shape_idx: int) -> void:
	if event is InputEventMouseButton and event.is_pressed():
		is_held = !is_held
		if is_held:
			z_index = 10
		else:
			z_index = 0
			
			var col = floor(position.x / GameConfig.GRID_SIZE)
			var row = floor(position.y / GameConfig.GRID_SIZE)
			
			if col >= 0 and col < GameConfig.BOARD_COLS and row >= 0 and row < GameConfig.BOARD_ROWS:
				# 駒をマスの中央に配置
				var new_x = (col * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
				var new_y = (row * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
				position = Vector2(new_x, new_y)
			else:
				is_held = true
				z_index = 10
