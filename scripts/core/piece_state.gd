class_name PieceState

extends RefCounted


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


var piece_type: Type = Type.PAWN
var is_gote: bool = false
var is_promoted: bool = false
var current_col: int = -1
var current_row: int = -1


func _init(_piece_type: Type = Type.PAWN, _is_gote: bool = false, _col: int = -1, _row: int = -1) -> void:
	piece_type = _piece_type
	is_gote = _is_gote
	current_col = _col
	current_row = _row


func is_in_hand() -> bool:
	return current_col == -1 and current_row == -1
