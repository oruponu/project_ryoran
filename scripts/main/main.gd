extends Node2D


@onready var board = $Board
@onready var player_piece_stand = $PlayerPieceStand
@onready var enemy_piece_stand = $EnemyPieceStand
@onready var move_history_panel = $MoveHistoryPanel
@onready var new_game_button = $HBoxContainer/NewGameButton
@onready var undo_button = $HBoxContainer/UndoButton
@onready var resign_button = $HBoxContainer/ResignButton
@onready var turn_label = $CanvasLayer/TurnLabel
@onready var check_label = $CanvasLayer/CheckLabel
@onready var win_rate_bar = $WinRateBar
@onready var common_dialog = $CommonDialog
@onready var audio_stream_player = $AudioStreamPlayer


var board_grid = []
var current_turn = 0
var holding_piece = null
var current_legal_coords: Array[Vector2i] = []
var move_history: Array[MoveRecord] = []
var is_game_active: bool = false
var is_ai_thinking: bool = false
var _shogi_engine: ShogiEngine = ShogiEngine.new()
var _ai_thread: Thread
var _eval_engine: ShogiEngine = ShogiEngine.new()
var _eval_thread: Thread
var last_analyzed_turn: int = 0


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	new_game_button.pressed.connect(_on_new_game_button_pressed)
	undo_button.pressed.connect(_on_undo_button_pressed)
	resign_button.pressed.connect(_on_resign_button_pressed)
	
	_shogi_engine.is_enemy_side = true
	
	_reset_game()


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(_delta: float) -> void:
	if not is_game_active:
		return
	
	if is_ai_thinking:
		return
	
	if _eval_thread != null:
		if _eval_thread.is_alive():
			return
		else:
			if _eval_thread.is_started():
				_eval_thread.wait_to_finish()
			_eval_thread = null
	
	if current_turn == last_analyzed_turn:
		return
	
	last_analyzed_turn = current_turn
	_start_background_analysis()


func _on_new_game_button_pressed() -> void:
	var result = await request_new_game_decision()
	if not result:
		return
	
	_reset_game()


func _on_undo_button_pressed() -> void:
	_undo_last_move()
	_undo_last_move()


func _on_resign_button_pressed() -> void:
	var result = await request_resign_decision()
	if not result:
		return
	
	var is_player_win = current_turn % 2 != 0
	await _finish_game(is_player_win)


func _update_button_states() -> void:
	if not is_game_active:
		new_game_button.disabled = false
		undo_button.disabled = false
		resign_button.disabled = true
		return
	
	if is_ai_thinking:
		new_game_button.disabled = true
		undo_button.disabled = true
		resign_button.disabled = true
		return
	
	new_game_button.disabled = false
	undo_button.disabled = move_history.is_empty()
	resign_button.disabled = move_history.is_empty()


func _reset_game() -> void:
	board_grid.clear()
	current_turn = 0
	holding_piece = null
	current_legal_coords.clear()
	move_history.clear()
	is_game_active = true
	is_ai_thinking = false
	
	board.clear_pieces()
	player_piece_stand.clear_pieces()
	enemy_piece_stand.clear_pieces()
	
	for x in range(GameConfig.BOARD_COLS):
		var column = []
		for y in range(GameConfig.BOARD_ROWS):
			column.append(null)
		board_grid.append(column)
	
	_update_button_states()
	_update_turn_display()
	win_rate_bar.reset_bar()
	board.setup_starting_board(self)
	move_history_panel.clear()
	move_history_panel.add_game_start(current_turn)
	check_label.cancel_animation()


func handle_piece_input(piece: Piece) -> void:
	if not is_game_active or is_ai_thinking:
		return
	
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
	
	current_legal_coords = []
	if piece.current_col == -1 and piece.current_row == -1:
		if piece.get_parent() is PieceStand:
			piece.get_parent().update_layout()
		current_legal_coords = piece.get_legal_drops()
	else:
		current_legal_coords = piece.get_legal_moves()
	
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
	
	var move_record = MoveRecord.new(piece, piece.current_col, piece.current_row, col, row)
	
	if piece.current_col == -1 and piece.current_row == -1:
		_drop_piece(piece, col, row)
	else:
		await _move_piece(piece, col, row, move_record, PromotionMode.Type.ASK_USER)
	
	move_history.append(move_record)
	
	holding_piece = null
	_finish_turn(piece)


func _finish_turn(piece: Piece) -> void:
	piece.is_held = false
	piece.z_index = 0
	
	board.clear_guides()
	
	holding_piece = null
	
	current_turn += 1
	_update_button_states()
	_update_last_move_highlight()
	_update_turn_display()
	
	var record = move_history.back()
	var prev_record = move_history[-2] if move_history.size() >= 2 else null
	move_history_panel.add_move(current_turn, record, prev_record)
	
	var target_is_enemy = current_turn % 2 != 0
	if ShogiEngine.is_king_in_check(self, target_is_enemy):
		if _is_checkmate(target_is_enemy):
			if _shogi_engine != null and target_is_enemy == _shogi_engine.is_enemy_side:
				await _finish_game(target_is_enemy)
				return
			
			var chose_to_resign = await request_checkmate_decision(target_is_enemy)
			if chose_to_resign:
				await _finish_game(target_is_enemy)
				return
			else:
				_undo_last_move()
				_undo_last_move()
		else:
			check_label.play_animation()
			audio_stream_player.play_check()
	
	var next_is_enemy = current_turn % 2 != 0
	if is_game_active and next_is_enemy == _shogi_engine.is_enemy_side:
		_play_ai_turn()


func _play_ai_turn() -> void:
	is_ai_thinking = true
	_update_button_states()

	_shogi_engine.update_state(self)
	
	_ai_thread = Thread.new()
	_ai_thread.start(_calculate_next_move)


func _start_background_analysis() -> void:
	_eval_engine.update_state(self)
	_eval_thread = Thread.new()
	_eval_thread.start(_run_background_analysis)


func _run_background_analysis() -> void:
	var is_enemy_side = current_turn % 2 != 0
	_eval_engine.is_enemy_side = is_enemy_side
	var move = _eval_engine.search_best_move()
	call_deferred("_on_background_analysis_completed", move)


func _on_background_analysis_completed(move: Dictionary) -> void:
	if _eval_thread != null:
		if _eval_thread.is_alive():
			_eval_thread.wait_to_finish()
		_eval_thread = null
	win_rate_bar.update_bar(move.win_rate)


func _calculate_next_move() -> void:
	var move = _shogi_engine.search_best_move()
	call_deferred("_apply_next_move", move)


func _apply_next_move(move: Dictionary) -> void:
	_ai_thread.wait_to_finish()
	_ai_thread = null
	
	# 投了かどうか
	if move.is_empty():
		await _finish_game(!_shogi_engine.is_enemy_side)
		is_ai_thinking = false
		return
	
	var piece: Piece = null
	if move.is_drop:
		var is_enemy = _shogi_engine.is_enemy_side
		var stand = enemy_piece_stand if is_enemy else player_piece_stand
		for child in stand.get_children():
			if child.piece_type == move.piece_type:
				piece = child
				break
	else:
		piece = get_piece(move.from_col, move.from_row)
	
	var col = move.to_col
	var row = move.to_row
	var move_record = MoveRecord.new(piece, piece.current_col, piece.current_row, col, row)
	
	if piece.current_col == -1 and piece.current_row == -1:
		_drop_piece(piece, col, row)
	else:
		var mode = PromotionMode.Type.FORCE_PROMOTE if move.is_promotion else PromotionMode.Type.FORCE_STAY
		await _move_piece(piece, col, row, move_record, mode)
	
	move_history.append(move_record)
	win_rate_bar.update_bar(1 - move.win_rate)
	
	is_ai_thinking = false
	_finish_turn(piece)


func _finish_game(is_player_win: bool) -> void:
	current_turn += 1
	_update_turn_display()
	move_history_panel.add_resignation(current_turn)
	check_label.cancel_animation()
	await show_game_result(current_turn - 1, is_player_win)
	is_game_active = false
	
	_update_button_states()


func _move_piece(piece: Piece, col: int, row: int, move_record: MoveRecord, mode: PromotionMode.Type) -> void:
	var prev_row = piece.current_row
	
	var target_piece = get_piece(col, row)
	if target_piece != null:
		move_record.captured_promoted = target_piece.is_promoted
		capture_piece(target_piece)
		move_record.captured_piece = target_piece

	_update_piece_data(piece, col, row)
	_update_piece_position(piece, col, row)
	
	audio_stream_player.play_place()
	
	await _handle_promotion(piece, prev_row, row, move_record, mode)


func _drop_piece(piece: Piece, col: int, row: int) -> void:
	var source_stand = piece.get_parent()
	
	piece.reparent(board)
	piece.visible = true
	
	_update_piece_data(piece, col, row)
	_update_piece_position(piece, col, row)
	
	audio_stream_player.play_place()
	
	if source_stand is PieceStand:
		source_stand.update_layout()


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


func _handle_promotion(piece: Piece, prev_row: int, current_row: int, move_record: MoveRecord, mode: PromotionMode.Type) -> void:
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
		
		var should_promote = false
		match mode:
			PromotionMode.Type.ASK_USER:
				should_promote = await request_promotion_decision()
			PromotionMode.Type.FORCE_PROMOTE:
				should_promote = true
			PromotionMode.Type.FORCE_STAY:
				should_promote = false
		
		if should_promote:
			piece.set_promoted(true)
			move_record.is_promotion = true


func _undo_last_move() -> void:
	if move_history.is_empty():
		return
	
	if not is_game_active:
		current_turn -= 1
		move_history_panel.remove_last_move()
	
	var last_move = move_history.pop_back()
	var piece = last_move.piece
	
	if last_move.from_col == -1 and last_move.from_row == -1:
		# 持ち駒から打った
		board_grid[last_move.to_col][last_move.to_row] = null
		
		piece.current_col = -1
		piece.current_row = -1
		
		if piece.is_enemy:
			enemy_piece_stand.add_piece(piece)
		else:
			player_piece_stand.add_piece(piece)
	else:
		# 盤上の移動
		update_board_state(piece.current_col, piece.current_row, last_move.from_col, last_move.from_row, piece)
		piece.current_col = last_move.from_col
		piece.current_row = last_move.from_row
		
		_update_piece_position(piece, piece.current_col, piece.current_row)
		
		if last_move.is_promotion:
			piece.set_promoted(false)
	
	if last_move.captured_piece != null:
		var captured = last_move.captured_piece
		
		var source_stand = captured.get_parent()
		
		captured.reparent(board)
		captured.visible = true
		captured.is_enemy = !captured.is_enemy
		captured.rotation_degrees = 180 if captured.is_enemy else 0
		
		if last_move.captured_promoted:
			captured.set_promoted(true)
		
		captured.current_col = last_move.to_col
		captured.current_row = last_move.to_row
		board_grid[captured.current_col][captured.current_row] = captured
		
		_update_piece_position(captured, captured.current_col, captured.current_row)
		
		if source_stand is PieceStand:
			source_stand.update_layout()
	
	current_turn -= 1
	is_game_active = true
	_update_last_move_highlight()
	_update_turn_display()
	_update_button_states()
	move_history_panel.remove_last_move()
	check_label.cancel_animation()


func _update_last_move_highlight() -> void:
	if move_history.is_empty():
		board.clear_last_move_highlight()
	else:
		var last_record = move_history.back()
		board.update_last_move_highlight(last_record.to_col, last_record.to_row)


func _update_turn_display() -> void:
	var current_side = "後手番" if current_turn % 2 != 0 else "先手番"
	turn_label.text = current_side


func _is_checkmate(target_is_enemy: bool) -> bool:
	for col in range(GameConfig.BOARD_COLS):
		for row in range(GameConfig.BOARD_ROWS):
			var piece = get_piece(col, row)
			
			if piece != null and piece.is_enemy == target_is_enemy:
				var moves = piece.get_legal_moves()
				for move in moves:
					if ShogiEngine.is_king_safe_after_move(self, piece, move.x, move.y):
						return false
	
	var target_stand = enemy_piece_stand if target_is_enemy else player_piece_stand
	for piece in target_stand.get_children():
		if piece is Piece:
			var drops = piece.get_legal_drops()
			for drop in drops:
				if ShogiEngine.is_king_safe_after_move(self, piece, drop.x, drop.y):
					return false
	
	return true


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


func request_resign_decision() -> bool:
	return await common_dialog.ask_user("投了しますか？", "投了する", "投了しない")


func request_checkmate_decision(is_enemy_mated: bool) -> bool:
	var side_text = "後手" if is_enemy_mated else "先手"
	var message = "%sの玉が詰まされました。\n投了しますか？" % side_text
	return await common_dialog.ask_user(message, "投了する", "待った")


func request_promotion_decision() -> bool:
	return await common_dialog.ask_user("成りますか？", "成る", "成らない")


func show_game_result(move_count: int, is_player_win: bool) -> void:
	var side_text = "先手" if is_player_win else "後手"
	var message = "まで、%d手で%sの勝ち。" % [move_count, side_text]
	await common_dialog.ask_user(message, "OK", "")
