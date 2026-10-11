extends Node2D


@onready var board: Board = $Board
@onready var sente_piece_stand: PieceStand = $SentePieceStand
@onready var gote_piece_stand: PieceStand = $GotePieceStand
@onready var move_history_panel: MoveHistoryPanel = $MoveHistoryPanel
@onready var new_game_button: Button = $HBoxContainer/NewGameButton
@onready var undo_button: Button = $HBoxContainer/UndoButton
@onready var hint_button: Button = $HBoxContainer/HintButton
@onready var resign_button: Button = $HBoxContainer/ResignButton
@onready var turn_label: Label = $CanvasLayer/TurnLabel
@onready var check_label: CheckLabel = $CanvasLayer/CheckLabel
@onready var win_rate_bar: WinRateBar = $WinRateBar
@onready var common_dialog: CommonDialog = $CommonDialog
@onready var audio_stream_player: GameAudioPlayer = $AudioStreamPlayer
@onready var engine_worker: EngineWorker = $EngineWorker


var game_state := GameState.new()
var sfen_serializer: SfenSerializer
var board_view: BoardView
var input_controller: InputController
var move_executor: MoveExecutor
var game_referee: GameReferee
var is_game_active: bool = false
var is_ai_thinking: bool = false
var _hint_moves: Array[Dictionary] = []
var _shogi_engine: ShogiEngine = ShogiEngine.new()


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	new_game_button.pressed.connect(_on_new_game_button_pressed)
	undo_button.pressed.connect(_on_undo_button_pressed)
	hint_button.pressed.connect(_on_hint_button_pressed)
	resign_button.pressed.connect(_on_resign_button_pressed)

	sfen_serializer = SfenSerializer.new(game_state)
	board_view = BoardView.new(game_state, board, sente_piece_stand, gote_piece_stand, board.piece_scene)
	board_view.piece_spawned.connect(_on_piece_spawned)
	game_referee = GameReferee.new(game_state, sfen_serializer, _shogi_engine)

	engine_worker.setup(sfen_serializer, game_referee.repetition_tracker)
	engine_worker.search_completed.connect(_on_search_completed)
	engine_worker.analysis_completed.connect(_on_analysis_completed)

	input_controller = InputController.new(game_state, board, _shogi_engine, sfen_serializer)
	input_controller.move_submitted.connect(_on_move_submitted)

	move_executor = MoveExecutor.new(
		game_state,
		board_view,
		audio_stream_player,
		engine_worker,
		_shogi_engine,
		request_promotion_decision
	)

	_reset_game()


func _on_piece_spawned(piece: Piece) -> void:
	piece.clicked.connect(_on_piece_clicked)


func _on_new_game_button_pressed() -> void:
	input_controller.cancel_holding()

	var result: bool = await request_new_game_decision()
	if not result:
		return

	_reset_game()


func _on_undo_button_pressed() -> void:
	input_controller.cancel_holding()

	_undo_to_player_turn()


func _on_resign_button_pressed() -> void:
	input_controller.cancel_holding()

	var result: bool = await request_resign_decision()
	if not result:
		return

	var is_sente_win := game_state.is_gote_turn()
	await _finish_game(is_sente_win)


func _update_button_states() -> void:
	if not is_game_active:
		new_game_button.disabled = false
		undo_button.disabled = not _has_player_move()
		hint_button.disabled = true
		resign_button.disabled = true
		return

	if is_ai_thinking:
		new_game_button.disabled = true
		undo_button.disabled = true
		hint_button.disabled = true
		resign_button.disabled = true
		return

	new_game_button.disabled = false
	undo_button.disabled = not _has_player_move()
	hint_button.disabled = _hint_moves.is_empty()
	resign_button.disabled = game_state.move_history.is_empty()


func _reset_game() -> void:
	game_state.reset()
	input_controller.reset()
	is_game_active = true
	is_ai_thinking = false
	_clear_hint()

	_update_button_states()
	_update_turn_display()
	engine_worker.reset()
	win_rate_bar.reset_bar(true)
	game_state.setup_starting_position()
	board_view.rebuild()
	game_referee.start()
	move_history_panel.clear()
	move_history_panel.add_game_start(game_state.move_count)
	check_label.cancel_animation()
	if _is_ai_turn():
		_play_ai_turn()
	else:
		engine_worker.request_analysis()


func _on_piece_clicked(piece: Piece) -> void:
	if not is_game_active or is_ai_thinking:
		return

	input_controller.handle_click(piece)


func _on_move_submitted(piece: Piece, col: int, row: int) -> void:
	_clear_hint()

	var state := piece.state
	var mode: PromotionMode.Type
	if _shogi_engine.is_dead_end(state.piece_type, state.is_gote, row):
		mode = PromotionMode.Type.FORCE_PROMOTE
	else:
		mode = PromotionMode.Type.ASK_USER

	var record: MoveRecord = await move_executor.execute(state, col, row, mode)
	_finish_turn(record)


func _finish_turn(record: MoveRecord) -> void:
	_clear_hint()
	input_controller.release_holding(board_view.node_for(record.state))

	var outcome := game_referee.commit(record)
	_update_button_states()
	_update_last_move_highlight()
	_update_turn_display()

	var prev_record: MoveRecord = game_state.move_history[-2] if game_state.move_history.size() >= 2 else null
	move_history_panel.add_move(game_state.move_count, record, prev_record)

	var is_gote_turn := game_state.is_gote_turn()
	match outcome:
		GameReferee.Outcome.SENNICHITE:
			await _finish_game_draw()
			return
		GameReferee.Outcome.SENTE_WINS_BY_PERPETUAL_CHECK:
			await _finish_game_perpetual_check(true)
			return
		GameReferee.Outcome.GOTE_WINS_BY_PERPETUAL_CHECK:
			await _finish_game_perpetual_check(false)
			return
		GameReferee.Outcome.CHECKMATE:
			if _is_ai_turn():
				await _finish_game(is_gote_turn)
				return

			var chose_to_resign: bool = await request_checkmate_decision(is_gote_turn)
			if chose_to_resign:
				await _finish_game(is_gote_turn)
			else:
				_undo_to_player_turn()
			return
		GameReferee.Outcome.CHECK:
			check_label.play_animation()
			audio_stream_player.play_check()

	if is_game_active and _is_ai_turn():
		_play_ai_turn()
	else:
		engine_worker.request_analysis()


func _play_ai_turn() -> void:
	is_ai_thinking = true
	_update_button_states()

	if not engine_worker.request_search():
		is_ai_thinking = false
		_update_button_states()


func _on_analysis_completed(moves: Array) -> void:
	if moves.is_empty():
		win_rate_bar.update_bar(_to_sente_win_rate(0.0))
		_hint_moves = []
		_update_button_states()
		return

	win_rate_bar.update_bar(_to_sente_win_rate(moves[0].win_rate))

	var best_win_rate: float = moves[0].win_rate
	_hint_moves = []
	for move in moves:
		if best_win_rate - move.win_rate <= GameConfig.HINT_WIN_RATE_MARGIN:
			_hint_moves.append(move)

	_update_button_states()


func _to_sente_win_rate(win_rate: float) -> float:
	return 1.0 - win_rate if game_state.is_gote_turn() else win_rate


func _on_search_completed(move: Dictionary) -> void:
	# 投了かどうか
	if move.is_empty():
		await _finish_game(GameConfig.AI_IS_GOTE)
		is_ai_thinking = false
		return

	var state: PieceState = null
	if move.is_drop:
		state = game_state.find_hand_piece(GameConfig.AI_IS_GOTE, move.piece_type)
	else:
		state = game_state.get_piece(move.from_col, move.from_row)

	var mode: PromotionMode.Type = PromotionMode.Type.FORCE_PROMOTE if move.is_promotion else PromotionMode.Type.FORCE_STAY
	var record: MoveRecord = await move_executor.execute(state, move.to_col, move.to_row, mode)

	is_ai_thinking = false
	_finish_turn(record)


func _finish_game(is_sente_win: bool) -> void:
	_clear_hint()
	move_history_panel.add_resignation(game_state.move_count + 1)
	check_label.cancel_animation()
	await show_game_result(game_state.move_count, is_sente_win)
	is_game_active = false

	_update_button_states()


func _finish_game_draw() -> void:
	move_history_panel.add_sennichite(game_state.move_count + 1)
	check_label.cancel_animation()
	await show_draw_result(game_state.move_count)
	is_game_active = false

	_update_button_states()


func _finish_game_perpetual_check(is_sente_win: bool) -> void:
	move_history_panel.add_perpetual_check(game_state.move_count + 1, is_sente_win)
	check_label.cancel_animation()
	await show_perpetual_check_result(game_state.move_count, is_sente_win)
	is_game_active = false

	_update_button_states()


func _undo_to_player_turn() -> void:
	_undo_last_move()
	if _is_ai_turn():
		_undo_last_move()


func _undo_last_move() -> void:
	if game_state.move_history.is_empty():
		return

	_clear_hint()

	if not is_game_active:
		move_history_panel.remove_last_move()

	var last_move := game_referee.rollback()
	move_executor.undo(last_move)

	is_game_active = true
	_update_last_move_highlight()
	_update_turn_display()
	_update_button_states()
	move_history_panel.remove_last_move()
	check_label.cancel_animation()

	if game_state.move_count <= 0:
		engine_worker.reset()
		win_rate_bar.reset_bar(false)

	engine_worker.request_analysis()


func _update_last_move_highlight() -> void:
	if game_state.move_history.is_empty():
		board.clear_last_move_highlight()
	else:
		var last_record: MoveRecord = game_state.move_history.back()
		board.update_last_move_highlight(last_record.to_col, last_record.to_row)


func _clear_hint() -> void:
	_hint_moves = []
	board.clear_hint_arrows()
	_update_button_states()


func _on_hint_button_pressed() -> void:
	if _hint_moves.is_empty():
		return

	input_controller.cancel_holding()

	var entries: Array[Dictionary] = []
	for move in _hint_moves:
		var from_pos: Variant = _hint_from_position(move)
		if from_pos == null:
			continue

		entries.append({
			"from": from_pos,
			"to": GameConfig.cell_to_position(move.to_col, move.to_row),
		})

	board.show_hint_arrows(entries)


func _hint_from_position(move: Dictionary) -> Variant:
	if not move.is_drop:
		return GameConfig.cell_to_position(move.from_col, move.from_row)

	var state := game_state.find_hand_piece(game_state.is_gote_turn(), move.piece_type)
	if state == null:
		return null

	var piece := board_view.node_for(state)
	if piece == null:
		return null

	return board.to_local(piece.global_position)


func _update_turn_display() -> void:
	var turn_text := "後手番" if game_state.is_gote_turn() else "先手番"
	turn_label.text = turn_text


func _is_ai_turn() -> bool:
	return game_state.is_gote_turn() == GameConfig.AI_IS_GOTE


func _has_player_move() -> bool:
	var player_move_count := 1 if GameConfig.AI_IS_GOTE else 2
	return game_state.move_history.size() >= player_move_count


func request_new_game_decision() -> bool:
	return await common_dialog.ask_user("対局をはじめますか？", "はい", "いいえ")


func request_resign_decision() -> bool:
	return await common_dialog.ask_user("投了しますか？", "投了する", "投了しない")


func request_checkmate_decision(is_gote_mated: bool) -> bool:
	var mated_text := "後手" if is_gote_mated else "先手"
	var message := "%sの玉が詰まされました。\n投了しますか？" % mated_text
	return await common_dialog.ask_user(message, "投了する", "待った")


func request_promotion_decision() -> bool:
	return await common_dialog.ask_user("成りますか？", "成る", "成らない")


func show_game_result(move_count: int, is_sente_win: bool) -> void:
	var winner_text := "先手" if is_sente_win else "後手"
	var message := "まで、%d手で%sの勝ち。" % [move_count, winner_text]
	await common_dialog.ask_user(message, "OK", "")


func show_draw_result(move_count: int) -> void:
	var message := "まで、%d手で千日手。引き分け。" % move_count
	await common_dialog.ask_user(message, "OK", "")


func show_perpetual_check_result(move_count: int, is_sente_win: bool) -> void:
	var winner_text := "先手" if is_sente_win else "後手"
	var message := "まで、%d手で連続王手の千日手。%sの反則勝ち。" % [move_count, winner_text]
	await common_dialog.ask_user(message, "OK", "")
