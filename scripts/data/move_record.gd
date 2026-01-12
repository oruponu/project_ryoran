class_name MoveRecord

extends RefCounted


var piece: Piece
var from_col: int
var from_row: int
var to_col: int
var to_row: int
var captured_piece: Piece = null
var is_promotion: bool = false
var captured_promoted: bool = false


func _init(
	_piece: Piece,
	_from_col: int,
	_from_row: int,
	_to_col: int,
	_to_row: int
) -> void:
	piece = _piece
	from_col = _from_col
	from_row = _from_row
	to_col = _to_col
	to_row = _to_row
