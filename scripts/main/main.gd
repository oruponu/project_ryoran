extends Node2D


@onready var board: Board = $Board
@onready var sente_piece_stand: PieceStand = $SentePieceStand
@onready var gote_piece_stand: PieceStand = $GotePieceStand
@onready var move_history_panel: MoveHistoryPanel = $MoveHistoryPanel
@onready var new_game_button: Button = $HBoxContainer/NewGameButton
@onready var undo_button: Button = $HBoxContainer/UndoButton
@onready var hint_button: Button = $HBoxContainer/HintButton
@onready var resign_button: Button = $HBoxContainer/ResignButton
@onready var button_container: HBoxContainer = $HBoxContainer
@onready var turn_label: Label = $CanvasLayer/TurnLabel
@onready var check_label: CheckLabel = $CanvasLayer/CheckLabel
@onready var last_move_label: Label = $CanvasLayer/LastMoveLabel
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
var game_dialogs: GameDialogs
var layout: MainLayout
var is_game_active: bool = false
var is_ai_thinking: bool = false
var _hint_moves: Array[Dictionary] = []
var _shogi_engine: ShogiEngine = ShogiEngine.new()


func _ready() -> void:
	new_game_button.pressed.connect(_on_new_game_button_pressed)
	undo_button.pressed.connect(_on_undo_button_pressed)
	hint_button.pressed.connect(_on_hint_button_pressed)
	resign_button.pressed.connect(_on_resign_button_pressed)
	move_history_panel.entries_changed.connect(_on_history_entries_changed)

	sfen_serializer = SfenSerializer.new(game_state)
	board_view = BoardView.new(game_state, board, sente_piece_stand, gote_piece_stand, board.piece_scene)
	game_referee = GameReferee.new(game_state, sfen_serializer, _shogi_engine)

	engine_worker.setup(sfen_serializer, game_referee.repetition_tracker)
	engine_worker.search_completed.connect(_on_search_completed)
	engine_worker.analysis_completed.connect(_on_analysis_completed)

	input_controller = InputController.new(game_state, board, board_view, _shogi_engine, sfen_serializer)
	input_controller.move_submitted.connect(_on_move_submitted)

	game_dialogs = GameDialogs.new(common_dialog)
	move_executor = MoveExecutor.new(
		game_state,
		board_view,
		audio_stream_player,
		_decide_promotion
	)

	layout = MainLayout.new(
		self,
		board,
		sente_piece_stand,
		gote_piece_stand,
		move_history_panel,
		button_container,
		win_rate_bar,
		last_move_label,
		common_dialog
	)
	layout.layout_changed.connect(_on_layout_changed)
	get_tree().root.size_changed.connect(layout.update)
	layout.update()

	_reset_game()


func _on_history_entries_changed() -> void:
	last_move_label.text = move_history_panel.last_entry_text()


func _on_new_game_button_pressed() -> void:
	input_controller.cancel_holding()

	var result: bool = await game_dialogs.request_new_game()
	if not result:
		return

	_reset_game()


func _on_undo_button_pressed() -> void:
	input_controller.cancel_holding()

	_undo_to_player_turn()


func _on_resign_button_pressed() -> void:
	input_controller.cancel_holding()

	var result: bool = await game_dialogs.request_resign()
	if not result:
		return

	var is_sente_win := game_state.is_gote_turn()
	await _end_game(GameEnding.Type.RESIGNATION, is_sente_win)


func _on_layout_changed(_is_portrait: bool) -> void:
	input_controller.cancel_holding()
	board.clear_hint_arrows()


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


func _unhandled_input(event: InputEvent) -> void:
	var mouse_button := event as InputEventMouseButton
	if mouse_button == null or not mouse_button.pressed or mouse_button.button_index != MOUSE_BUTTON_LEFT:
		return
	if not is_game_active or is_ai_thinking:
		return

	var is_touch := mouse_button.device == InputEvent.DEVICE_ID_EMULATION
	var coord := _tap_coord(mouse_button)
	input_controller.handle_tap(coord, _tap_state(mouse_button, coord), is_touch)
	get_viewport().set_input_as_handled()


func _tap_coord(event: InputEventMouseButton) -> Vector2i:
	var local_event := board.make_input_local(event) as InputEventMouseButton
	var coord := GameConfig.position_to_cell(local_event.position)
	if coord.x < 0 or coord.x >= GameConfig.BOARD_COLS or coord.y < 0 or coord.y >= GameConfig.BOARD_ROWS:
		return InputController.OUTSIDE
	return coord


func _tap_state(event: InputEventMouseButton, coord: Vector2i) -> PieceState:
	if coord != InputController.OUTSIDE:
		return game_state.get_piece(coord.x, coord.y)

	for stand: PieceStand in [sente_piece_stand, gote_piece_stand]:
		var local_event := stand.make_input_local(event) as InputEventMouseButton
		var state := stand.piece_at(local_event.position)
		if state != null:
			return state
	return null


func _on_move_submitted(state: PieceState, col: int, row: int, mode: PromotionMode.Type) -> void:
	_clear_hint()

	var record: MoveRecord = await move_executor.execute(state, col, row, mode)
	_finish_turn(record)


func _finish_turn(record: MoveRecord) -> void:
	var outcome := game_referee.commit(record)
	_update_button_states()
	_update_last_move_highlight()
	_update_turn_display()

	var prev_record: MoveRecord = game_state.move_history[-2] if game_state.move_history.size() >= 2 else null
	move_history_panel.add_move(game_state.move_count, record, prev_record)

	var is_gote_turn := game_state.is_gote_turn()
	match outcome:
		GameReferee.Outcome.SENNICHITE:
			await _end_game(GameEnding.Type.SENNICHITE, false)
			return
		GameReferee.Outcome.SENTE_WINS_BY_PERPETUAL_CHECK:
			await _end_game(GameEnding.Type.PERPETUAL_CHECK, true)
			return
		GameReferee.Outcome.GOTE_WINS_BY_PERPETUAL_CHECK:
			await _end_game(GameEnding.Type.PERPETUAL_CHECK, false)
			return
		GameReferee.Outcome.CHECKMATE:
			if _is_ai_turn():
				await _end_game(GameEnding.Type.RESIGNATION, is_gote_turn)
				return

			var chose_to_resign: bool = await game_dialogs.request_checkmate(is_gote_turn)
			if chose_to_resign:
				await _end_game(GameEnding.Type.RESIGNATION, is_gote_turn)
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


func _decide_promotion() -> bool:
	engine_worker.suspend_analysis()
	var should_promote: bool = await game_dialogs.request_promotion()
	engine_worker.resume_analysis()
	return should_promote


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
		await _end_game(GameEnding.Type.RESIGNATION, GameConfig.AI_IS_GOTE)
		is_ai_thinking = false
		return

	var state: PieceState = null
	if move.is_drop:
		state = game_state.find_hand_piece(GameConfig.AI_IS_GOTE, move.piece_type)
	else:
		state = game_state.get_piece(move.from_col, move.from_row)

	if state == null:
		push_error("AIの着手を実行できません（対象の駒が見つかりません）")
		is_ai_thinking = false
		_update_button_states()
		return

	var mode: PromotionMode.Type = PromotionMode.Type.FORCE_PROMOTE if move.is_promotion else PromotionMode.Type.FORCE_STAY
	var record: MoveRecord = await move_executor.execute(state, move.to_col, move.to_row, mode)

	is_ai_thinking = false
	_finish_turn(record)


func _end_game(ending: GameEnding.Type, is_sente_win: bool) -> void:
	_clear_hint()
	move_history_panel.add_ending(game_state.move_count + 1, ending, is_sente_win)
	check_label.cancel_animation()
	await game_dialogs.show_result(game_state.move_count, ending, is_sente_win)
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

	return board_view.position_of(state)


func _update_turn_display() -> void:
	var turn_text := "後手番" if game_state.is_gote_turn() else "先手番"
	turn_label.text = turn_text


func _is_ai_turn() -> bool:
	return game_state.is_gote_turn() == GameConfig.AI_IS_GOTE


func _has_player_move() -> bool:
	var player_move_count := 1 if GameConfig.AI_IS_GOTE else 2
	return game_state.move_history.size() >= player_move_count
