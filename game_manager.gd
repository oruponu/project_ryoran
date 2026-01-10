extends Node


var board_grid = []
var holding_piece = null
var player_hand = []
var enemy_hand = []
var promotion_dialog: Node = null


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	promotion_dialog = get_tree().root.get_node("Main/PromotionDialog")
	initialize_board()


func initialize_board() -> void:
	board_grid = []
	for x in range(GameConfig.BOARD_COLS):
		var column = []
		for y in range(GameConfig.BOARD_ROWS):
			column.append(null)
		board_grid.append(column)


func get_piece(col: int, row: int):
	return board_grid[col][row]


func is_cell_empty(col: int, row: int) -> bool:
	return board_grid[col][row] == null


func update_board_state(old_col: int, old_row: int, new_col: int, new_row: int, piece_obj) -> void:
	if old_col != -1 and old_row != -1:
		board_grid[old_col][old_row] = null
	
	board_grid[new_col][new_row] = piece_obj


func capture_piece(piece) -> void:
	if piece.current_col != -1 and piece.current_row != -1:
		board_grid[piece.current_col][piece.current_row] = null
	
	if !piece.is_enemy:
		player_hand.append(piece)
	else:
		enemy_hand.append(piece)


func request_promotion_decision() -> bool:
	if promotion_dialog:
		return await promotion_dialog.ask_user()
	return false
