class_name Piece

extends Area2D


enum Type {
	KING,
	ROOK,
	BISHOP,
	GOLD,
	SILVER,
	KNIGHT,
	LANCE,
	PAWN
}


const PIECE_DATA = {
	Type.KING: {
		"default": "玉",
		"enemy": "王"
	},
	Type.ROOK: {
		"default": "飛",
		"promoted": "龍"
	},
	Type.BISHOP: {
		"default": "角",
		"promoted": "馬"
	},
	Type.GOLD: {
		"default": "金"
	},
	Type.SILVER: {
		"default": "銀",
		"promoted": "全"
	},
	Type.KNIGHT: {
		"default": "桂",
		"promoted": "圭"
	},
	Type.LANCE: {
		"default": "香",
		"promoted": "杏"
	},
	Type.PAWN: {
		"default": "歩",
		"promoted": "と"
	}
}

const MOVES = {
	Type.KING: [Vector2(0, -1), Vector2(1, -1), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1), Vector2(-1, 1), Vector2(-1, 0), Vector2(-1, -1)],
	Type.GOLD: [Vector2(0, -1), Vector2(1, -1), Vector2(1, 0), Vector2(0, 1), Vector2(-1, 0), Vector2(-1, -1)],
	Type.SILVER: [Vector2(0, -1), Vector2(1, -1), Vector2(1, 1), Vector2(-1, 1), Vector2(-1, -1)],
	Type.KNIGHT: [Vector2(-1, -2), Vector2(1, -2)],
	Type.PAWN: [Vector2(0, -1)]
}


@onready var label = $Label


var piece_type = Type.PAWN
var is_enemy = false
var is_promoted = false
var is_held = false
var current_col = -1
var current_row = -1
var main: Node


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(_delta: float) -> void:
	if is_held:
		global_position = get_global_mouse_position()


func _on_input_event(viewport: Node, event: InputEvent, _shape_idx: int) -> void:
	if event is InputEventMouseButton:
		if event.is_pressed():
			main.handle_piece_input(self)
			viewport.set_input_as_handled()


func is_legal_move(target_col: int, target_row: int) -> bool:
	# 盤面の範囲外には移動不可
	if not _is_inside_board(target_col, target_row):
		return false
	
	# 現在地と同じ場所には移動不可
	if target_col == current_col and target_row == current_row:
		return false
	
	# ルールで認められていない場所には移動不可
	if not can_move_geometry(target_col, target_row):
		return false
	
	# 味方の駒がある場所には移動不可
	var target_piece = main.get_piece(target_col, target_row)
	if target_piece != null:
		if target_piece.is_enemy == is_enemy:
			return false
	
	return true


func is_legal_drop(target_col: int, target_row: int) -> bool:
	# 盤面の範囲外には配置不可
	if not _is_inside_board(target_col, target_row):
		return false
	
	# すでに駒がある場所には配置不可
	if main.get_piece(target_col, target_row):
		return false
	
	# 行き所のない場所には配置不可
	if _is_dead_end(target_row):
		return false
	
	# 二歩になる場所には配置不可
	if _is_nifu(target_col):
		return false
	
	return true


func get_legal_moves() -> Array[Vector2i]:
	var legal_moves: Array[Vector2i] = []
	
	for col in range(GameConfig.BOARD_COLS):
		for row in range(GameConfig.BOARD_ROWS):
			if is_legal_move(col, row):
				legal_moves.append(Vector2i(col, row))
	
	return legal_moves


func get_legal_drops() -> Array[Vector2i]:
	var legal_drops: Array[Vector2i] = []
	
	for col in range(GameConfig.BOARD_COLS):
		for row in range(GameConfig.BOARD_ROWS):
			if is_legal_drop(col, row):
				legal_drops.append(Vector2i(col, row))
	
	# TODO: 打ち歩詰めを禁止
	return legal_drops


func can_move_geometry(target_col: int, target_row: int) -> bool:
	var dx = target_col - current_col
	var dy = target_row - current_row
	
	if is_enemy:
		dx = -dx
		dy = -dy
	
	var relative_pos = Vector2(dx, dy)
	var effective_type = piece_type
	
	if is_promoted:
		match piece_type:
			Type.SILVER, Type.KNIGHT, Type.LANCE, Type.PAWN:
				effective_type = Type.GOLD
	
	match effective_type:
		Type.ROOK:
			if dx == 0 or dy == 0:
				if _is_path_blocked(target_col, target_row):
					return false
				return true
			if is_promoted and abs(dx) <= 1 and abs(dy) <= 1:
				return true
			return false
		Type.BISHOP:
			if abs(dx) == abs(dy):
				if _is_path_blocked(target_col, target_row):
					return false
				return true
			if is_promoted and (abs(dx) + abs(dy) <= 1):
				return true
			return false
		Type.LANCE:
			if dx == 0 and dy < 0:
				if _is_path_blocked(target_col, target_row):
					return false
				return true
			return false
		Type.KNIGHT:
			return relative_pos in MOVES[Type.KNIGHT]
		_:
			if MOVES.has(effective_type):
				return relative_pos in MOVES[effective_type]
	return false


func _is_inside_board(col: int, row: int) -> bool:
	return col >= 0 and col < GameConfig.BOARD_COLS and row >= 0 and row < GameConfig.BOARD_ROWS


func _is_path_blocked(target_col: int, target_row: int) -> bool:
	var dx = target_col - current_col
	var dy = target_row - current_row
	var steps = max(abs(dx), abs(dy))
	
	var step_x = sign(dx)
	var step_y = sign(dy)
	
	# 現在地と目的地の間にあるマスを確認
	for i in range(1, steps):
		var check_col = current_col + (step_x * i)
		var check_row = current_row + (step_y * i)
		
		if main.get_piece(check_col, check_row) != null:
			return true
	return false


func _is_dead_end(target_row: int) -> bool:
	var relative_row = target_row if not is_enemy else GameConfig.BOARD_ROWS - 1 - target_row
	match piece_type:
		Type.PAWN, Type.LANCE:
			return relative_row == 0
		Type.KNIGHT:
			return relative_row <= 1
	
	return false


func _is_nifu(target_col: int) -> bool:
	if piece_type != Type.PAWN:
		return false
	
	for row in range(GameConfig.BOARD_ROWS):
		var target = main.get_piece(target_col, row)
		if target != null:
			if target.is_enemy == self.is_enemy and target.piece_type == Type.PAWN and not target.is_promoted:
				return true
	
	return false


func promote() -> void:
	is_promoted = true
	_update_display()


func init_pos(col: int, row: int, type: Type, _is_enemy: bool, _main: Node) -> void:
	current_col = col
	current_row = row
	piece_type = type
	is_enemy = _is_enemy
	main = _main
	
	_update_display()
	
	var new_x = (col * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
	var new_y = (row * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
	position = Vector2(new_x, new_y)
	
	main.update_board_state(-1, -1, col, row, self)


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
	
	if is_promoted:
		label.modulate = Color(0.8, 0, 0)
	else:
		label.modulate = Color.BLACK
	
	if is_enemy:
		rotation_degrees = 180
	else:
		rotation_degrees = 0
