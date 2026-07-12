class_name MoveExecutor

extends RefCounted


var _game_state: GameState
var _board: Board
var _player_piece_stand: PieceStand
var _enemy_piece_stand: PieceStand
var _audio_stream_player: GameAudioPlayer
var _engine_worker: EngineWorker
var _shogi_engine: ShogiEngine
var _promotion_decider: Callable


func _init(
	game_state: GameState,
	board: Board,
	player_piece_stand: PieceStand,
	enemy_piece_stand: PieceStand,
	audio_stream_player: GameAudioPlayer,
	engine_worker: EngineWorker,
	shogi_engine: ShogiEngine,
	promotion_decider: Callable
) -> void:
	_game_state = game_state
	_board = board
	_player_piece_stand = player_piece_stand
	_enemy_piece_stand = enemy_piece_stand
	_audio_stream_player = audio_stream_player
	_engine_worker = engine_worker
	_shogi_engine = shogi_engine
	_promotion_decider = promotion_decider


func execute_move(piece: Piece, col: int, row: int, move_record: MoveRecord, mode: PromotionMode.Type) -> void:
	var prev_row: int = piece.current_row

	var target_piece := _game_state.get_piece(col, row)
	if target_piece != null:
		move_record.captured_promoted = target_piece.is_promoted
		_game_state.capture(target_piece.state)
		_capture_piece(target_piece)
		move_record.captured_piece = target_piece

	_game_state.move_piece(piece.state, col, row)
	_update_piece_data(piece, col, row)
	_update_piece_position(piece, col, row)

	_audio_stream_player.play_place()

	await _handle_promotion(piece, prev_row, row, move_record, mode)


func execute_drop(piece: Piece, col: int, row: int) -> void:
	var source_stand := piece.get_parent()
	_game_state.drop_piece(piece.state, col, row)

	piece.reparent(_board)
	piece.visible = true

	_update_piece_data(piece, col, row)
	_update_piece_position(piece, col, row)

	_audio_stream_player.play_place()

	if source_stand is PieceStand:
		source_stand.update_layout()


func undo(record: MoveRecord) -> void:
	var piece := record.piece

	if record.from_col == -1 and record.from_row == -1:
		# 持ち駒から打った
		_game_state.return_to_hand(piece.state)
		_game_state.remove_piece(record.to_col, record.to_row)

		piece.current_col = -1
		piece.current_row = -1

		if piece.is_enemy:
			_enemy_piece_stand.add_piece(piece, true)
		else:
			_player_piece_stand.add_piece(piece, true)
	else:
		# 盤上の移動
		_game_state.move_piece(piece.state, record.from_col, record.from_row)
		_game_state.update_board_state(piece.current_col, piece.current_row, record.from_col, record.from_row, piece)
		piece.current_col = record.from_col
		piece.current_row = record.from_row

		_update_piece_position(piece, piece.current_col, piece.current_row)

		if record.is_promotion:
			_game_state.set_promoted(piece.state, false)
			piece.set_promoted(false)

	if record.captured_piece != null:
		var captured := record.captured_piece

		_game_state.uncapture(captured.state, record.to_col, record.to_row, record.captured_promoted)
		var source_stand := captured.get_parent()

		captured.reparent(_board)
		captured.visible = true
		captured.is_enemy = !captured.is_enemy
		captured.rotation_degrees = 180 if captured.is_enemy else 0

		if record.captured_promoted:
			captured.set_promoted(true)

		captured.current_col = record.to_col
		captured.current_row = record.to_row
		_game_state.update_board_state(-1, -1, captured.current_col, captured.current_row, captured)

		_update_piece_position(captured, captured.current_col, captured.current_row)

		if source_stand is PieceStand:
			source_stand.update_layout(true)


func _capture_piece(piece: Piece) -> void:
	if not piece.is_in_hand():
		_game_state.remove_piece(piece.current_col, piece.current_row)

	if piece.is_enemy:
		_player_piece_stand.add_piece(piece)
	else:
		_enemy_piece_stand.add_piece(piece)


func _update_piece_data(piece: Piece, col: int, row: int) -> void:
	_game_state.update_board_state(piece.current_col, piece.current_row, col, row, piece)
	piece.current_col = col
	piece.current_row = row


func _update_piece_position(piece: Piece, col: int, row: int) -> void:
	piece.position = GameConfig.cell_to_position(col, row)


func _handle_promotion(piece: Piece, prev_row: int, current_row: int, move_record: MoveRecord, mode: PromotionMode.Type) -> void:
	if not _shogi_engine.can_promote(piece.piece_type, piece.is_promoted, piece.is_enemy, prev_row, current_row):
		return

	piece.is_held = false

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
		_game_state.set_promoted(piece.state, true)
		piece.set_promoted(true)
		move_record.is_promotion = true

	if analysis_suspended:
		_engine_worker.resume_analysis()
