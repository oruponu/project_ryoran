extends Node2D


@onready var board: Board = $Board
@onready var player_piece_stand: PieceStand = $PlayerPieceStand
@onready var enemy_piece_stand: PieceStand = $EnemyPieceStand
@onready var move_history_panel: MoveHistoryPanel = $MoveHistoryPanel
@onready var new_game_button: Button = $HBoxContainer/NewGameButton
@onready var undo_button: Button = $HBoxContainer/UndoButton
@onready var resign_button: Button = $HBoxContainer/ResignButton
@onready var turn_label: Label = $CanvasLayer/TurnLabel
@onready var check_label: CheckLabel = $CanvasLayer/CheckLabel
@onready var win_rate_bar: WinRateBar = $WinRateBar
@onready var common_dialog: CommonDialog = $CommonDialog
@onready var audio_stream_player: GameAudioPlayer = $AudioStreamPlayer
@onready var engine_worker: EngineWorker = $EngineWorker


var game_state := GameState.new()
var sfen_serializer: SfenSerializer
var input_controller: InputController
var move_executor: MoveExecutor
var is_game_active: bool = false
var is_ai_thinking: bool = false
var _shogi_engine: ShogiEngine = ShogiEngine.new()


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	new_game_button.pressed.connect(_on_new_game_button_pressed)
	undo_button.pressed.connect(_on_undo_button_pressed)
	resign_button.pressed.connect(_on_resign_button_pressed)

	sfen_serializer = SfenSerializer.new(game_state, player_piece_stand, enemy_piece_stand)

	engine_worker.setup(sfen_serializer, game_state.repetition_tracker)
	engine_worker.search_completed.connect(_on_search_completed)
	engine_worker.analysis_completed.connect(_on_analysis_completed)

	board.piece_spawned.connect(_on_piece_spawned)

	input_controller = InputController.new(game_state, board, _shogi_engine, sfen_serializer)
	input_controller.move_submitted.connect(_on_move_submitted)

	move_executor = MoveExecutor.new(
		game_state,
		board,
		player_piece_stand,
		enemy_piece_stand,
		audio_stream_player,
		engine_worker,
		_shogi_engine,
		request_promotion_decision
	)

	_reset_game()


func _on_piece_spawned(piece: Piece) -> void:
	game_state.register_piece(piece)
	piece.clicked.connect(_on_piece_clicked)


func _on_new_game_button_pressed() -> void:
	var result: bool = await request_new_game_decision()
	if not result:
		return

	_reset_game()


func _on_undo_button_pressed() -> void:
	# 後手が投了しているときのみ一手前に戻す
	if not is_game_active and not game_state.is_gote_turn():
		_undo_last_move()
	else:
		_undo_last_move()
		_undo_last_move()


func _on_resign_button_pressed() -> void:
	var result: bool = await request_resign_decision()
	if not result:
		return

	var is_player_win := game_state.is_gote_turn()
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
	undo_button.disabled = game_state.move_history.is_empty()
	resign_button.disabled = game_state.move_history.is_empty()


func _reset_game() -> void:
	game_state.reset()
	input_controller.reset()
	is_game_active = true
	is_ai_thinking = false

	board.clear_pieces()
	player_piece_stand.clear_pieces()
	enemy_piece_stand.clear_pieces()

	_update_button_states()
	_update_turn_display()
	engine_worker.reset()
	win_rate_bar.reset_bar(true)
	board.setup_starting_board()
	var initial_in_check := _shogi_engine.is_king_in_check(sfen_serializer.to_sfen(), game_state.is_gote_turn())
	game_state.repetition_tracker.reset(_current_position_hash(), initial_in_check)
	move_history_panel.clear()
	move_history_panel.add_game_start(game_state.current_turn)
	check_label.cancel_animation()


func _on_piece_clicked(piece: Piece) -> void:
	if not is_game_active or is_ai_thinking:
		return

	input_controller.handle_click(piece)


func _on_move_submitted(piece: Piece, col: int, row: int) -> void:
	var move_record := MoveRecord.new(piece, piece.current_col, piece.current_row, col, row)

	if piece.is_in_hand():
		move_executor.execute_drop(piece, col, row)
	else:
		var mode: PromotionMode.Type
		if _shogi_engine.is_dead_end(piece.piece_type, piece.is_enemy, row):
			mode = PromotionMode.Type.FORCE_PROMOTE
		else:
			mode = PromotionMode.Type.ASK_USER

		await move_executor.execute_move(piece, col, row, move_record, mode)

	game_state.move_history.append(move_record)

	_finish_turn(piece)


func _finish_turn(piece: Piece) -> void:
	input_controller.release_holding(piece)

	game_state.current_turn += 1
	_update_button_states()
	_update_last_move_highlight()
	_update_turn_display()

	var record: MoveRecord = game_state.move_history.back()
	var prev_record: MoveRecord = game_state.move_history[-2] if game_state.move_history.size() >= 2 else null
	move_history_panel.add_move(game_state.current_turn, record, prev_record)

	var stm_is_enemy := game_state.is_gote_turn()
	var sfen := sfen_serializer.to_sfen()
	var in_check := _shogi_engine.is_king_in_check(sfen, stm_is_enemy)

	var rep_count := game_state.repetition_tracker.record(_shogi_engine.get_position_hash(sfen), in_check)
	if rep_count >= GameConfig.SENNICHITE_COUNT:
		match game_state.repetition_tracker.classify_sennichite():
			RepetitionTracker.STM_PERPETUAL:
				await _finish_game_perpetual_check(stm_is_enemy)
			RepetitionTracker.OPP_PERPETUAL:
				await _finish_game_perpetual_check(not stm_is_enemy)
			_:
				await _finish_game_draw()
		return

	if in_check:
		if not _shogi_engine.has_any_legal_move(sfen):
			if stm_is_enemy == EngineWorker.AI_IS_ENEMY:
				await _finish_game(stm_is_enemy)
				return

			var chose_to_resign: bool = await request_checkmate_decision(stm_is_enemy)
			if chose_to_resign:
				await _finish_game(stm_is_enemy)
				return
			else:
				_undo_last_move()
				_undo_last_move()
				return
		else:
			check_label.play_animation()
			audio_stream_player.play_check()

	var next_is_enemy := game_state.is_gote_turn()
	if is_game_active and next_is_enemy == EngineWorker.AI_IS_ENEMY:
		_play_ai_turn()
	else:
		engine_worker.request_analysis()


func _play_ai_turn() -> void:
	is_ai_thinking = true
	_update_button_states()

	if not engine_worker.request_search():
		is_ai_thinking = false
		_update_button_states()


func _on_analysis_completed(win_rate: float) -> void:
	win_rate_bar.update_bar(win_rate)


func _on_search_completed(move: Dictionary) -> void:
	# 投了かどうか
	if move.is_empty():
		await _finish_game(!EngineWorker.AI_IS_ENEMY)
		is_ai_thinking = false
		return

	var piece: Piece = null
	if move.is_drop:
		var is_enemy: bool = EngineWorker.AI_IS_ENEMY
		var stand: PieceStand = enemy_piece_stand if is_enemy else player_piece_stand
		for child in stand.get_children():
			if child is Piece:
				var candidate := child as Piece
				if candidate.piece_type == move.piece_type:
					piece = candidate
					break
	else:
		piece = game_state.get_piece(move.from_col, move.from_row)

	var col: int = move.to_col
	var row: int = move.to_row
	var move_record := MoveRecord.new(piece, piece.current_col, piece.current_row, col, row)

	if piece.is_in_hand():
		move_executor.execute_drop(piece, col, row)
	else:
		var mode: PromotionMode.Type = PromotionMode.Type.FORCE_PROMOTE if move.is_promotion else PromotionMode.Type.FORCE_STAY
		await move_executor.execute_move(piece, col, row, move_record, mode)

	game_state.move_history.append(move_record)

	is_ai_thinking = false
	_finish_turn(piece)


func _finish_game(is_player_win: bool) -> void:
	game_state.current_turn += 1
	_update_turn_display()
	move_history_panel.add_resignation(game_state.current_turn)
	check_label.cancel_animation()
	await show_game_result(game_state.current_turn - 1, is_player_win)
	is_game_active = false

	_update_button_states()


func _finish_game_draw() -> void:
	game_state.current_turn += 1
	_update_turn_display()
	move_history_panel.add_sennichite(game_state.current_turn)
	check_label.cancel_animation()
	await show_draw_result(game_state.current_turn - 1)
	is_game_active = false

	_update_button_states()


func _finish_game_perpetual_check(is_player_win: bool) -> void:
	game_state.current_turn += 1
	_update_turn_display()
	move_history_panel.add_perpetual_check(game_state.current_turn, is_player_win)
	check_label.cancel_animation()
	await show_perpetual_check_result(game_state.current_turn - 1, is_player_win)
	is_game_active = false

	_update_button_states()


func _undo_last_move() -> void:
	if game_state.move_history.is_empty():
		return

	if not is_game_active:
		game_state.current_turn -= 1
		move_history_panel.remove_last_move()

	var last_move: MoveRecord = game_state.move_history.pop_back()
	game_state.repetition_tracker.undo()

	move_executor.undo(last_move)

	game_state.current_turn -= 1
	is_game_active = true
	_update_last_move_highlight()
	_update_turn_display()
	_update_button_states()
	move_history_panel.remove_last_move()
	check_label.cancel_animation()

	if game_state.current_turn <= 0:
		engine_worker.reset()
		win_rate_bar.reset_bar(false)
	else:
		engine_worker.request_analysis()


func _update_last_move_highlight() -> void:
	if game_state.move_history.is_empty():
		board.clear_last_move_highlight()
	else:
		var last_record: MoveRecord = game_state.move_history.back()
		board.update_last_move_highlight(last_record.to_col, last_record.to_row)


func _update_turn_display() -> void:
	var current_side := "後手番" if game_state.is_gote_turn() else "先手番"
	turn_label.text = current_side


func _current_position_hash() -> int:
	return _shogi_engine.get_position_hash(sfen_serializer.to_sfen())


func request_new_game_decision() -> bool:
	return await common_dialog.ask_user("対局をはじめますか？", "はい", "いいえ")


func request_resign_decision() -> bool:
	return await common_dialog.ask_user("投了しますか？", "投了する", "投了しない")


func request_checkmate_decision(is_enemy_mated: bool) -> bool:
	var side_text := "後手" if is_enemy_mated else "先手"
	var message := "%sの玉が詰まされました。\n投了しますか？" % side_text
	return await common_dialog.ask_user(message, "投了する", "待った")


func request_promotion_decision() -> bool:
	return await common_dialog.ask_user("成りますか？", "成る", "成らない")


func show_game_result(move_count: int, is_player_win: bool) -> void:
	var side_text := "先手" if is_player_win else "後手"
	var message := "まで、%d手で%sの勝ち。" % [move_count, side_text]
	await common_dialog.ask_user(message, "OK", "")


func show_draw_result(move_count: int) -> void:
	var message := "まで、%d手で千日手。引き分け。" % move_count
	await common_dialog.ask_user(message, "OK", "")


func show_perpetual_check_result(move_count: int, is_player_win: bool) -> void:
	var winner_text := "先手" if is_player_win else "後手"
	var message := "まで、%d手で連続王手の千日手。%sの反則勝ち。" % [move_count, winner_text]
	await common_dialog.ask_user(message, "OK", "")
