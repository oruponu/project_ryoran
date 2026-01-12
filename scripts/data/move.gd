class_name Move

extends RefCounted


var piece: Piece
var to_col: int
var to_row: int
var is_promotion: bool
var is_drop: bool


func _init(_piece: Piece, _to_col: int, _to_row: int, _is_promotion: bool) -> void:
	piece = _piece
	to_col = _to_col
	to_row = _to_row
	is_promotion = _is_promotion
	is_drop = piece.current_col == -1 and piece.current_row == -1
