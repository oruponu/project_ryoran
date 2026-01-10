extends Node


var board_grid = []
var holding_piece = null


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	initialize_board()


func initialize_board() -> void:
	board_grid = []
	for x in range(GameConfig.BOARD_COLS):
		var column = []
		for y in range(GameConfig.BOARD_ROWS):
			column.append(null)
		board_grid.append(column)


func is_cell_empty(col: int, row: int) -> bool:
	return board_grid[col][row] == null


func update_board_state(old_col: int, old_row: int, new_col: int, new_row: int, piece_obj) -> void:
	if old_col != -1 and old_row != -1:
		board_grid[old_col][old_row] = null
	
	board_grid[new_col][new_row] = piece_obj
