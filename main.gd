extends Node2D


@onready var board = $Board
@onready var player_piece_stand = $PlayerPieceStand
@onready var enemy_piece_stand = $EnemyPieceStand
@onready var new_game_button = $HBoxContainer/NewGameButton
@onready var turn_label = $CanvasLayer/TurnLabel
@onready var check_label = $CanvasLayer/CheckLabel
@onready var common_dialog = $CommonDialog


var board_grid = []
var current_turn = 0
var holding_piece = null
var current_legal_coords: Array[Vector2i] = []
var move_history: Array[MoveRecord] = []


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	new_game_button.pressed.connect(_on_new_game_button_pressed)
	
	_reset_game()


func _on_new_game_button_pressed() -> void:
	var result = await request_new_game_decision()
	if not result:
		return
	
	_reset_game()


func _reset_game() -> void:
	board_grid.clear()
	current_turn = 0
	holding_piece = null
	current_legal_coords.clear()
	move_history.clear()
	
	board.clear_pieces()
	player_piece_stand.clear_pieces()
	enemy_piece_stand.clear_pieces()
	
	for x in range(GameConfig.BOARD_COLS):
		var column = []
		for y in range(GameConfig.BOARD_ROWS):
			column.append(null)
		board_grid.append(column)
	
	board.setup_starting_board(self)


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
	
	current_legal_coords = []
	
	# 王手放置になる手を除外
	for coord in legal_coords:
		if _is_king_safe_after_move(piece, coord.x, coord.y):
			current_legal_coords.append(coord)
	
	board.show_guides(current_legal_coords)


func _attempt_place(piece: Piece) -> void:
	var local_pos = board.to_local(piece.global_position)
	var col = floor(local_pos.x / GameConfig.GRID_SIZE)
	var row = floor(local_pos.y / GameConfig.GRID_SIZE)
	
	# 合法手でないならャンセル
	var target_pos = Vector2i(col, row)
	if not target_pos in current_legal_coords:
		_cancel_move(piece)
		return
	
	var move_record = MoveRecord.new(piece, piece.current_col, piece.current_row, col, row, null, false)
	
	if piece.current_col == -1 and piece.current_row == -1:
		_drop_piece(piece, col, row)
	else:
		await _move_piece(piece, col, row, move_record)
	
	move_history.append(move_record)
	
	holding_piece = null
	_finish_turn(piece)


func _finish_turn(piece: Piece) -> void:
	piece.is_held = false
	piece.z_index = 0
	
	board.clear_guides()
	
	holding_piece = null
	
	current_turn += 1
	_update_turn_display()
	
	var is_enemy_turn = current_turn % 2 != 0
	if _is_king_in_check(is_enemy_turn):
		if _is_checkmate(is_enemy_turn):
			var chose_to_resign = await request_checkmate_decision(is_enemy_turn)
			if chose_to_resign:
				await show_game_result(current_turn, is_enemy_turn)
			else:
				# TODO: 待ったの処理を実装
				pass
		else:
			check_label.play_animation()


func _move_piece(piece: Piece, col: int, row: int, move_record: MoveRecord) -> void:
	var target_piece = get_piece(col, row)
	if target_piece != null:
		capture_piece(target_piece)
		move_record.captured_piece = target_piece
	
	var prev_row = piece.current_row
	_update_piece_data(piece, col, row)
	_update_piece_position(piece, col, row)
	
	await _handle_promotion(piece, prev_row, row, move_record)


func _drop_piece(piece: Piece, col: int, row: int) -> void:
	piece.reparent(board)
	_update_piece_data(piece, col, row)
	_update_piece_position(piece, col, row)


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


func _handle_promotion(piece: Piece, prev_row: int, current_row: int, move_record: MoveRecord) -> void:
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
			move_record.promoted = true


func _update_turn_display() -> void:
	var current_side = "後手" if current_turn % 2 != 0 else "先手"
	turn_label.text = "%d 手目（%s）" % [current_turn, current_side]


func _is_checkmate(is_enemy_turn: bool) -> bool:
	for col in range(GameConfig.BOARD_COLS):
		for row in range(GameConfig.BOARD_ROWS):
			var piece = get_piece(col, row)
			
			if piece != null and piece.is_enemy == is_enemy_turn:
				var moves = piece.get_legal_moves()
				for move in moves:
					if _is_king_safe_after_move(piece, move.x, move.y):
						return false
	
	var target_stand = enemy_piece_stand if is_enemy_turn else player_piece_stand
	for piece in target_stand.get_children():
		if piece is Piece:
			var drops = piece.get_legal_drops()
			for drop in drops:
				if _is_king_safe_after_move(piece, drop.x, drop.y):
					return false
	
	return true


func _is_king_safe_after_move(piece: Piece, target_col: int, target_row: int) -> bool:
	var original_col = piece.current_col
	var original_row = piece.current_row
	var captured_piece = board_grid[target_col][target_row]
	
	if original_col != -1 and original_row != -1:
		board_grid[original_col][original_row] = null
	
	board_grid[target_col][target_row] = piece
	piece.current_col = target_col
	piece.current_row = target_row
	
	var is_safe = not _is_king_in_check(piece.is_enemy)
	
	piece.current_col = original_col
	piece.current_row = original_row
	if original_col != -1 and original_row != -1:
		board_grid[original_col][original_row] = piece
	
	board_grid[target_col][target_row] = captured_piece
	
	return is_safe


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


func request_new_game_decision() -> bool:
	return await common_dialog.ask_user("対局をはじめますか？", "はい", "いいえ")


func request_checkmate_decision(is_enemy_mated: bool) -> bool:
	var side_text = "後手" if is_enemy_mated else "先手"
	var message = "%sの玉が詰まされました。\n投了しますか？" % side_text
	return await common_dialog.ask_user(message, "投了する", "待った")


func request_promotion_decision() -> bool:
	return await common_dialog.ask_user("成りますか？", "成る", "成らない")


func show_game_result(move_count: int, is_enemy_mated: bool) -> void:
	var side_text = "先手" if is_enemy_mated else "後手"
	var message = "まで、%d手で%sの勝ち。" % [move_count, side_text]
	await common_dialog.ask_user(message, "OK", "")
