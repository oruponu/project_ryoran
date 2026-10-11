class_name MoveExecutor

extends RefCounted


var _game_state: GameState
var _board_view: BoardView
var _audio_stream_player: GameAudioPlayer
var _promotion_decider: Callable


func _init(
	game_state: GameState,
	board_view: BoardView,
	audio_stream_player: GameAudioPlayer,
	promotion_decider: Callable
) -> void:
	_game_state = game_state
	_board_view = board_view
	_audio_stream_player = audio_stream_player
	_promotion_decider = promotion_decider


func execute(state: PieceState, col: int, row: int, mode: PromotionMode.Type) -> MoveRecord:
	var move_record := MoveRecord.new(state, state.current_col, state.current_row, col, row)

	if state.is_in_hand():
		_execute_drop(state, col, row)
	else:
		await _execute_move(state, col, row, move_record, mode)

	return move_record


func _execute_move(state: PieceState, col: int, row: int, move_record: MoveRecord, mode: PromotionMode.Type) -> void:
	var captured_state := _game_state.get_piece(col, row)
	if captured_state != null:
		move_record.captured_promoted = captured_state.is_promoted
		_game_state.capture(captured_state)
		_board_view.capture_piece(captured_state)
		move_record.captured_state = captured_state

	_game_state.move_piece(state, col, row)
	_board_view.place_piece(state)

	_audio_stream_player.play_place()

	await _handle_promotion(state, move_record, mode)


func _execute_drop(state: PieceState, col: int, row: int) -> void:
	_game_state.drop_piece(state, col, row)
	_board_view.drop_piece(state)

	_audio_stream_player.play_place()


func undo(record: MoveRecord) -> void:
	var state := record.state

	if record.is_drop():
		# 持ち駒から打った
		_game_state.return_to_hand(state)
		_board_view.return_to_stand(state)
	else:
		# 盤上の移動
		_game_state.move_piece(state, record.from_col, record.from_row)
		_board_view.place_piece(state)

		if record.is_promotion:
			_game_state.set_promoted(state, false)
			_board_view.refresh_display(state)

	if record.captured_state != null:
		_game_state.uncapture(record.captured_state, record.to_col, record.to_row, record.captured_promoted)
		_board_view.revive_piece(record.captured_state)


func _handle_promotion(state: PieceState, move_record: MoveRecord, mode: PromotionMode.Type) -> void:
	var should_promote := false
	match mode:
		PromotionMode.Type.ASK_USER:
			should_promote = await _promotion_decider.call()
		PromotionMode.Type.FORCE_PROMOTE:
			should_promote = true
		PromotionMode.Type.FORCE_STAY:
			should_promote = false

	if should_promote:
		_game_state.set_promoted(state, true)
		_board_view.refresh_display(state)
		move_record.is_promotion = true
