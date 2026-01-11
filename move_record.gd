class_name MoveRecord

extends RefCounted


var piece: Piece
var from_col: int
var from_row: int
var to_col: int
var to_row: int
var captured_piece: Piece = null
var promoted: bool = false


func _init(
	p_piece: Piece,
	p_from_col: int,
	p_from_row: int,
	p_to_col: int,
	p_to_row: int,
	p_captured_piece: Piece,
	p_promoted: bool
) -> void:
	piece = p_piece
	from_col = p_from_col
	from_row = p_from_row
	to_col = p_to_col
	to_row = p_to_row
	captured_piece = p_captured_piece
	promoted = p_promoted
