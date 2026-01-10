extends Area2D


const PIECE_DATA = {
	"king": {
		"default": "玉",
		"enemy": "王"
	},
	"rook": {
		"default": "飛",
		"promoted": "龍"
	},
	"bishop": {
		"default": "角",
		"promoted": "馬"
	},
	"gold": {
		"default": "金"
	},
	"silver": {
		"default": "銀",
		"promoted": "全"
	},
	"knight": {
		"default": "桂",
		"promoted": "圭"
	},
	"lance": {
		"default": "香",
		"promoted": "杏"
	},
	"pawn": {
		"default": "歩",
		"promoted": "と"
	}
}


@onready var label = $Label

var piece_type = "pawn"
var is_enemy = false
var is_promoted = false
var is_held = false
var current_col = -1
var current_row = -1


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(_delta: float) -> void:
	if is_held:
		global_position = get_global_mouse_position()


func _on_input_event(_viewport: Node, event: InputEvent, _shape_idx: int) -> void:
	if event is InputEventMouseButton and event.is_pressed():
		if current_col == -1 or current_row == -1:
			return
		
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
				var target_piece = GameManager.get_piece(col, row)
				var can_move = false
				
				if target_piece == null:
					can_move = true
				elif col == current_col and row == current_row:
					can_move = true
				elif target_piece.is_enemy != self.is_enemy:
					can_move = true
					GameManager.capture_piece(target_piece)
					target_piece.move_to_hand()
				
				if can_move:
					GameManager.update_board_state(current_col, current_row, col, row, self)
					current_col = col
					current_row = row
					GameManager.holding_piece = null
					
					# 駒をマスの中央に配置
					var new_x = (col * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
					var new_y = (row * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
					position = Vector2(new_x, new_y)
				else:
					# 自駒があるため置けない
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
	
	_update_display()
	
	var new_x = (col * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
	var new_y = (row * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
	position = Vector2(new_x, new_y)
	
	GameManager.update_board_state(-1, -1, col, row, self)


func _update_display() -> void:
	if not PIECE_DATA.has(piece_type):
		label.text = "？"
		return
	
	var data = PIECE_DATA[piece_type]
	var disp_text = data.get("default", "？")
	if is_promoted and data.has("promoted"):
		disp_text = data["promoted"]
	elif is_enemy and data.has("enemy"):
		disp_text = data["enemy"]
	label.text = disp_text
	
	if is_enemy:
		rotation_degrees = 180
	else:
		rotation_degrees = 0


func move_to_hand() -> void:
	current_col = -1
	current_row = -1
	
	visible = false
	position = Vector2(-100, -100)
	
	is_enemy = !is_enemy
	
	if is_enemy:
		rotation_degrees = 180
	else:
		rotation_degrees = 0
