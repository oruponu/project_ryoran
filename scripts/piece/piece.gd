class_name Piece

extends Area2D


signal clicked(piece: Piece)


const PIECE_DATA = {
	PieceState.Type.KING: {
		"default": "玉",
		"enemy": "王"
	},
	PieceState.Type.ROOK: {
		"default": "飛",
		"promoted": "龍"
	},
	PieceState.Type.BISHOP: {
		"default": "角",
		"promoted": "馬"
	},
	PieceState.Type.GOLD: {
		"default": "金"
	},
	PieceState.Type.SILVER: {
		"default": "銀",
		"promoted": "全"
	},
	PieceState.Type.KNIGHT: {
		"default": "桂",
		"promoted": "圭"
	},
	PieceState.Type.LANCE: {
		"default": "香",
		"promoted": "杏"
	},
	PieceState.Type.PAWN: {
		"default": "歩",
		"promoted": "と"
	}
}


@onready var label: Label = $Label


var state: PieceState = null
var piece_type: PieceState.Type = PieceState.Type.PAWN
var is_enemy: bool = false
var is_promoted: bool = false
var is_held: bool = false
var current_col: int = -1
var current_row: int = -1


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(_delta: float) -> void:
	if is_held:
		global_position = get_global_mouse_position()


func _on_input_event(viewport: Node, event: InputEvent, _shape_idx: int) -> void:
	if event is InputEventMouseButton:
		if event.is_pressed():
			clicked.emit(self)
			viewport.set_input_as_handled()


func is_in_hand() -> bool:
	return current_col == -1 and current_row == -1


func set_promoted(_is_promoted: bool) -> void:
	is_promoted = _is_promoted
	_update_display()


func init_pos(col: int, row: int, type: PieceState.Type, _is_enemy: bool) -> void:
	current_col = col
	current_row = row
	piece_type = type
	is_enemy = _is_enemy

	_update_display()

	position = GameConfig.cell_to_position(col, row)


func refresh_display() -> void:
	_update_display()


func _update_display() -> void:
	if not PIECE_DATA.has(piece_type):
		label.text = "？"
		return

	var data: Dictionary = PIECE_DATA[piece_type]
	var disp_text: String = data.get("default", "？")
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
