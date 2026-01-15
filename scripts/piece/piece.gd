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


@onready var label = $Label


var piece_type = Type.PAWN
var is_enemy = false
var is_promoted = false
var is_held = false
var current_col = -1
var current_row = -1
var main: Node
var _shogi_engine = ShogiEngine.new()


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
	return _shogi_engine.is_legal_move(main, self, target_col, target_row)


func is_legal_drop(target_col: int, target_row: int) -> bool:
	return _shogi_engine.is_legal_drop(main, self, target_col, target_row)


func get_legal_moves() -> Array[Vector2i]:
	return _shogi_engine.get_legal_moves(main, self)


func get_legal_drops() -> Array[Vector2i]:
	return _shogi_engine.get_legal_drops(main, self)


func set_promoted(_is_promoted: bool) -> void:
	is_promoted = _is_promoted
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
