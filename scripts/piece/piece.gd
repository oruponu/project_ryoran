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
var is_held: bool = false


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(_delta: float) -> void:
	if is_held:
		global_position = get_global_mouse_position()


func _on_input_event(viewport: Node, event: InputEvent, _shape_idx: int) -> void:
	if event is InputEventMouseButton:
		if event.is_pressed():
			clicked.emit(self)
			viewport.set_input_as_handled()


func refresh_display() -> void:
	_update_display()


func _update_display() -> void:
	if state == null or not PIECE_DATA.has(state.piece_type):
		label.text = "？"
		return

	var data: Dictionary = PIECE_DATA[state.piece_type]
	var disp_text: String = data.get("default", "？")
	if state.is_promoted and data.has("promoted"):
		disp_text = data["promoted"]
	elif state.is_enemy and data.has("enemy"):
		disp_text = data["enemy"]
	label.text = disp_text

	if state.is_promoted:
		label.modulate = Color(0.8, 0, 0)
	else:
		label.modulate = Color.BLACK

	if state.is_enemy:
		rotation_degrees = 180
	else:
		rotation_degrees = 0
