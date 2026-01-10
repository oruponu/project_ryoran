extends Area2D


var piece_type = "pawn"
var is_enemy = false
var is_held = false
var current_col = -1
var current_row = -1


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(_delta: float) -> void:
	if is_held:
		global_position = get_global_mouse_position()


func _on_input_event(_viewport: Node, event: InputEvent, _shape_idx: int) -> void:
	if event is InputEventMouseButton and event.is_pressed():
		# 敵駒の操作を禁止
		if !is_held and is_enemy:
			return
		
		# 他の駒が選択中の場合は何もしない
		if !is_held and GameManager.holding_piece != null:
			return

		is_held = !is_held
		if is_held:
			z_index = 10
			GameManager.holding_piece = self
		else:
			z_index = 0
			
			var col = floor(position.x / GameConfig.GRID_SIZE)
			var row = floor(position.y / GameConfig.GRID_SIZE)
			
			if col >= 0 and col < GameConfig.BOARD_COLS and row >= 0 and row < GameConfig.BOARD_ROWS:
				if GameManager.is_cell_empty(col, row) or (col == current_col and row == current_row):
					GameManager.update_board_state(current_col, current_row, col, row, self)
					current_col = col
					current_row = row
					GameManager.holding_piece = null
					
					# 駒をマスの中央に配置
					var new_x = (col * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
					var new_y = (row * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
					position = Vector2(new_x, new_y)
				else:
					# 別の駒があるため置けない
					is_held = true
					z_index = 10
			else:
				# 盤外のため置けない
				is_held = true
				z_index = 10


func init_pos(col: int, row: int, type: String, _is_enemy: bool) -> void:
	current_col = col
	current_row = row
	piece_type = type
	is_enemy = _is_enemy
	
	if is_enemy:
		rotation_degrees = 180
	else:
		rotation_degrees = 0
	
	var new_x = (col * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
	var new_y = (row * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
	position = Vector2(new_x, new_y)
	
	GameManager.update_board_state(-1, -1, col, row, self)
