class_name Piece

extends Area2D


signal clicked(piece: Piece)


const PIECE_DATA = {
	PieceState.Type.KING: {
		"default": "玉",
		"gote": "王"
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


func _process(_delta: float) -> void:
	if is_held:
		global_position = get_global_mouse_position()


func _on_input_event(viewport: Node, event: InputEvent, _shape_idx: int) -> void:
	if event is InputEventMouseButton:
		if event.is_pressed():
			clicked.emit(self)
			viewport.set_input_as_handled()


func refresh_display() -> void:
	if state == null or not PIECE_DATA.has(state.piece_type):
		label.text = "？"
		return

	var data: Dictionary = PIECE_DATA[state.piece_type]
	var display_text: String = data.get("default", "？")
	if state.is_promoted and data.has("promoted"):
		display_text = data["promoted"]
	elif state.is_gote and data.has("gote"):
		display_text = data["gote"]
	label.text = display_text

	if state.is_promoted:
		label.modulate = Color(0.8, 0, 0)
	else:
		label.modulate = Color.BLACK

	rotation_degrees = 180 if state.is_gote else 0
