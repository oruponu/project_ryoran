extends Node


var board_grid = []
var current_turn = 0
var holding_piece = null
var board: Node2D = null
var player_piece_stand: PieceStand = null
var enemy_piece_stand: PieceStand = null
var turn_label: Label = null
var check_label: Label = null
var promotion_dialog: Node = null


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	var main_node = get_tree().root.get_node("Main")
	if main_node:
		board = main_node.get_node("Board")
		player_piece_stand = main_node.get_node("PlayerPieceStand")
		enemy_piece_stand = main_node.get_node("EnemyPieceStand")
		turn_label = main_node.get_node("CanvasLayer/TurnLabel")
		check_label = main_node.get_node("CanvasLayer/CheckLabel")
		promotion_dialog = main_node.get_node("PromotionDialog")
	
	initialize_board()


func initialize_board() -> void:
	board_grid = []
	for x in range(GameConfig.BOARD_COLS):
		var column = []
		for y in range(GameConfig.BOARD_ROWS):
			column.append(null)
		board_grid.append(column)


func handle_piece_input(piece: Piece) -> void:
	if holding_piece == null:
		_pick_up(piece)
	else:
		_attempt_place(holding_piece)


func _pick_up(piece: Piece) -> void:
	var is_enemy_turn = current_turn % 2 != 0
	if piece.is_enemy != is_enemy_turn:
		return
	
	holding_piece = piece
	piece.is_held = true
	piece.z_index = 10
	
	var legal_coords: Array[Vector2i] = []
	if piece.current_col == -1 and piece.current_row == -1:
		if piece.get_parent() is PieceStand:
			piece.get_parent().update_layout()
		legal_coords = piece.get_legal_drops()
	else:
		legal_coords = piece.get_legal_moves()
	
	board.show_guides(legal_coords)


func _attempt_place(piece: Piece) -> void:
	if board == null:
		_cancel_move(piece)
		return
	
	var local_pos = board.to_local(piece.global_position)
	var col = floor(local_pos.x / GameConfig.GRID_SIZE)
	var row = floor(local_pos.y / GameConfig.GRID_SIZE)
	
	var success = false
	if piece.current_col == -1 and piece.current_row == -1:
		success = _try_drop(piece, col, row)
	else:
		success = await _try_move(piece, col, row)
	
	if success:
		holding_piece = null
		_finish_turn(piece)
	else:
		_cancel_move(piece)

	
func _finish_turn(piece: Piece) -> void:
	piece.is_held = false
	piece.z_index = 0
	
	board.clear_guides()
	
	holding_piece = null
	
	current_turn += 1
	_update_turn_display()
	
	var is_enemy_turn = current_turn % 2 != 0
	if _is_king_in_check(is_enemy_turn):
		if check_label == null:
			return
	
		check_label.play_animation()


func _try_move(piece: Piece, col: int, row: int) -> bool:
	if not piece.is_legal_move(col, row):
		return false
	
	var target_piece = get_piece(col, row)
	if target_piece != null:
		capture_piece(target_piece)
	
	var prev_row = piece.current_row
	_update_piece_data(piece, col, row)
	_update_piece_position(piece, col, row)
	
	await _handle_promotion(piece, prev_row, row)
	
	return true


func _try_drop(piece: Piece, col: int, row: int) -> bool:
	if not piece.is_legal_drop(col, row):
		return false
	
	piece.reparent(board)
	_update_piece_data(piece, col, row)
	_update_piece_position(piece, col, row)
	
	return true


func _cancel_move(piece: Piece) -> void:
	piece.is_held = false
	piece.z_index = 0
	
	board.clear_guides()
	
	holding_piece = null
	
	if piece.current_col == -1 and piece.current_row == -1:
		if piece.get_parent() is PieceStand:
			piece.get_parent().update_layout()
	else:
		_update_piece_position(piece, piece.current_col, piece.current_row)


func _update_piece_data(piece: Piece, col: int, row: int) -> void:
	update_board_state(piece.current_col, piece.current_row, col, row, piece)
	piece.current_col = col
	piece.current_row = row


func _update_piece_position(piece: Piece, col: int, row: int) -> void:
	var new_x = (col * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
	var new_y = (row * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
	piece.position = Vector2(new_x, new_y)


func _handle_promotion(piece: Piece, prev_row: int, current_row: int) -> void:
	if piece.is_promoted or piece.piece_type == Piece.Type.KING or piece.piece_type == Piece.Type.GOLD:
		return
	
	var is_in_zone = false
	if !piece.is_enemy:
		if current_row <= 2 or prev_row <= 2:
			is_in_zone = true
	else:
		if current_row >= 6 or prev_row >= 6:
			is_in_zone = true
	
	if is_in_zone:
		piece.is_held = false
		var should_promote = await request_promotion_decision()
		if should_promote:
			piece.promote()


func _update_turn_display() -> void:
	if turn_label == null:
		return
	
	var current_side = "後手" if current_turn % 2 != 0 else "先手"
	turn_label.text = "%d 手目（%s）" % [current_turn, current_side]


func _is_king_in_check(target_is_enemy: bool) -> bool:
	var king_pos = _find_king_grid_position(target_is_enemy)
	if king_pos == Vector2i(-1, -1):
		return false
	
	var king_col = king_pos.x
	var king_row = king_pos.y
	
	for col in range(GameConfig.BOARD_COLS):
		for row in range(GameConfig.BOARD_ROWS):
			var attacker = get_piece(col, row)
			if attacker == null or attacker.is_enemy == target_is_enemy:
				continue
			
			if attacker.can_move_geometry(king_col, king_row):
				return true
	
	return false


func _find_king_grid_position(is_enemy_king: bool) -> Vector2i:
	for col in range(GameConfig.BOARD_COLS):
		for row in range(GameConfig.BOARD_ROWS):
			var piece = get_piece(col, row)
			if piece != null:
				if piece.piece_type == Piece.Type.KING and piece.is_enemy == is_enemy_king:
					return Vector2i(col, row)
	
	return Vector2i(-1, -1)


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
	
	if piece.is_enemy:
		player_piece_stand.add_piece(piece)
	else:
		enemy_piece_stand.add_piece(piece)


func request_promotion_decision() -> bool:
	if promotion_dialog:
		return await promotion_dialog.ask_user()
	return false
