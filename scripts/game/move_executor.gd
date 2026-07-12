class_name MoveExecutor

extends RefCounted


var _game_state: GameState
var _board_view: BoardView
var _audio_stream_player: GameAudioPlayer
var _engine_worker: EngineWorker
var _shogi_engine: ShogiEngine
var _promotion_decider: Callable


func _init(
	game_state: GameState,
	board_view: BoardView,
	audio_stream_player: GameAudioPlayer,
	engine_worker: EngineWorker,
	shogi_engine: ShogiEngine,
	promotion_decider: Callable
) -> void:
	_game_state = game_state
	_board_view = board_view
	_audio_stream_player = audio_stream_player
	_engine_worker = engine_worker
	_shogi_engine = shogi_engine
	_promotion_decider = promotion_decider


func execute_move(state: PieceState, col: int, row: int, move_record: MoveRecord, mode: PromotionMode.Type) -> void:
	var prev_row: int = state.current_row

	var target_state := _game_state.get_piece(col, row)
	if target_state != null:
		move_record.captured_promoted = target_state.is_promoted
		_game_state.capture(target_state)
		_board_view.capture_piece(target_state)
		move_record.captured_piece = target_state

	_game_state.move_piece(state, col, row)
	_board_view.place_piece(state)

	_audio_stream_player.play_place()

	await _handle_promotion(state, prev_row, row, move_record, mode)


func execute_drop(state: PieceState, col: int, row: int) -> void:
	_game_state.drop_piece(state, col, row)
	_board_view.drop_piece(state)

	_audio_stream_player.play_place()


func undo(record: MoveRecord) -> void:
	var state := record.piece

	if record.from_col == -1 and record.from_row == -1:
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

	if record.captured_piece != null:
		_game_state.uncapture(record.captured_piece, record.to_col, record.to_row, record.captured_promoted)
		_board_view.revive_piece(record.captured_piece)


func _handle_promotion(state: PieceState, prev_row: int, current_row: int, move_record: MoveRecord, mode: PromotionMode.Type) -> void:
	if not _shogi_engine.can_promote(state.piece_type, state.is_promoted, state.is_enemy, prev_row, current_row):
		return

	_board_view.node_for(state).is_held = false

	var should_promote := false
	var analysis_suspended := false
	match mode:
		PromotionMode.Type.ASK_USER:
			_engine_worker.suspend_analysis()
			analysis_suspended = true
			should_promote = await _promotion_decider.call()
		PromotionMode.Type.FORCE_PROMOTE:
			should_promote = true
		PromotionMode.Type.FORCE_STAY:
			should_promote = false

	if should_promote:
		_game_state.set_promoted(state, true)
		_board_view.refresh_display(state)
		move_record.is_promotion = true

	if analysis_suspended:
		_engine_worker.resume_analysis()
