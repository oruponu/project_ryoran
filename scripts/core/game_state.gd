class_name GameState

extends RefCounted


var board_grid: Array = []
var current_turn: int = 0
var move_history: Array[MoveRecord] = []
var repetition_tracker := RepetitionTracker.new()


func reset() -> void:
	board_grid.clear()
	current_turn = 0
	move_history.clear()

	for x in range(GameConfig.BOARD_COLS):
		var column: Array = []
		for y in range(GameConfig.BOARD_ROWS):
			column.append(null)
		board_grid.append(column)


func get_piece(col: int, row: int) -> Piece:
	return board_grid[col][row]


func is_cell_empty(col: int, row: int) -> bool:
	return board_grid[col][row] == null


func update_board_state(old_col: int, old_row: int, new_col: int, new_row: int, piece_obj: Piece) -> void:
	if old_col != -1 and old_row != -1:
		board_grid[old_col][old_row] = null

	board_grid[new_col][new_row] = piece_obj


func register_piece(piece: Piece) -> void:
	update_board_state(-1, -1, piece.current_col, piece.current_row, piece)


func remove_piece(col: int, row: int) -> void:
	board_grid[col][row] = null


func is_gote_turn() -> bool:
	return current_turn % 2 != 0
