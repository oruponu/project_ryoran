extends SceneTree


var _failures: int = 0
var _engine := ShogiEngine.new()
var _game_state: GameState
var _referee: GameReferee
var _last_record: MoveRecord


func _initialize() -> void:
	_test_continue_and_check()
	_test_checkmate()
	_test_stalemate()
	_test_sennichite()
	_test_perpetual_check_by_sente()
	_test_perpetual_check_by_gote()
	_test_rollback()

	if _failures == 0:
		print("Referee selftest: all passed")
	else:
		printerr("Referee selftest: %d failed" % _failures)
	quit(1 if _failures > 0 else 0)


func _check(test_name: String, condition: bool) -> void:
	if condition:
		print("ok: " + test_name)
	else:
		_failures += 1
		printerr("FAILED: " + test_name)


func _setup(pieces: Array, move_count: int) -> void:
	_game_state = GameState.new()
	_game_state.reset()
	_game_state.move_count = move_count
	for state: PieceState in pieces:
		_game_state.register_piece(state)
	_referee = GameReferee.new(_game_state, SfenSerializer.new(_game_state), _engine)
	_referee.start()


func _setup_rook_and_kings() -> void:
	_setup([
		PieceState.new(PieceState.Type.KING, true, 4, 0),
		PieceState.new(PieceState.Type.KING, false, 4, 8),
		PieceState.new(PieceState.Type.ROOK, false, 0, 4),
	], 0)


func _play(col: int, row: int, to_col: int, to_row: int) -> GameReferee.Outcome:
	var state := _game_state.get_piece(col, row)
	_last_record = MoveRecord.new(state, col, row, to_col, to_row)
	_game_state.move_piece(state, to_col, to_row)
	return _referee.commit(_last_record)


func _play_all(moves: Array) -> Array:
	var outcomes := []
	for move: Array in moves:
		outcomes.append(_play(move[0], move[1], move[2], move[3]))
	return outcomes


func _is_in_progress(outcome: GameReferee.Outcome) -> bool:
	return outcome == GameReferee.Outcome.CONTINUE or outcome == GameReferee.Outcome.CHECK


func _test_continue_and_check() -> void:
	_setup_rook_and_kings()
	_check("続行: 王手でない手は CONTINUE", _play(0, 4, 0, 5) == GameReferee.Outcome.CONTINUE)
	_check("続行: 手数が1つ進む", _game_state.move_count == 1)
	_check("続行: 棋譜に指した手が追加される",
			_game_state.move_history.size() == 1 and _game_state.move_history[0] == _last_record)

	_play(4, 0, 3, 0)
	_check("王手: 飛車で王手をかけると CHECK", _play(0, 5, 3, 5) == GameReferee.Outcome.CHECK)
	_check("王手: 手数と棋譜が3手分になる", _game_state.move_count == 3 and _game_state.move_history.size() == 3)


func _test_checkmate() -> void:
	_setup([
		PieceState.new(PieceState.Type.KING, true, 4, 0),
		PieceState.new(PieceState.Type.KING, false, 4, 8),
		PieceState.new(PieceState.Type.GOLD, false, 4, 2),
		PieceState.new(PieceState.Type.GOLD, false, 3, 2),
	], 0)
	_check("詰み: 頭金で CHECKMATE", _play(4, 2, 4, 1) == GameReferee.Outcome.CHECKMATE)
	_check("詰み: 詰まされたのは手番側 (後手)", _game_state.is_gote_turn())


func _test_stalemate() -> void:
	_setup([
		PieceState.new(PieceState.Type.KING, true, 8, 0),
		PieceState.new(PieceState.Type.KING, false, 4, 8),
		PieceState.new(PieceState.Type.GOLD, false, 6, 1),
		PieceState.new(PieceState.Type.GOLD, false, 7, 3),
	], 0)
	_check("ステイルメイト: 王手がなく合法手もない局面は CONTINUE", _play(7, 3, 7, 2) == GameReferee.Outcome.CONTINUE)


func _test_sennichite() -> void:
	_setup_rook_and_kings()
	var cycle := [[0, 4, 0, 5], [4, 0, 3, 0], [0, 5, 0, 4], [3, 0, 4, 0]]
	var outcomes := []
	for i in 3:
		outcomes.append_array(_play_all(cycle))

	_check("千日手: 11手目までは CONTINUE",
			outcomes.slice(0, 11).all(func(outcome): return outcome == GameReferee.Outcome.CONTINUE))
	_check("千日手: 4回目の同一局面 (12手目) で SENNICHITE", outcomes[11] == GameReferee.Outcome.SENNICHITE)

	var record := _referee.rollback()
	_game_state.move_piece(record.state, record.from_col, record.from_row)
	_check("千日手: 待ったで戻して同じ手を指し直すと再び SENNICHITE", _play(3, 0, 4, 0) == GameReferee.Outcome.SENNICHITE)
	_check("千日手: 指し直しても手数は12手のまま", _game_state.move_count == 12)


func _test_perpetual_check_by_sente() -> void:
	_setup([
		PieceState.new(PieceState.Type.KING, true, 4, 0),
		PieceState.new(PieceState.Type.KING, false, 4, 8),
		PieceState.new(PieceState.Type.ROOK, false, 8, 4),
	], 0)
	var outcomes := _play_all([[8, 4, 4, 4]])
	var cycle := [[4, 0, 3, 0], [4, 4, 3, 4], [3, 0, 4, 0], [3, 4, 4, 4]]
	for i in 3:
		outcomes.append_array(_play_all(cycle))

	_check("連続王手 (先手が王手をかけ続ける): 12手目までは対局が続く", outcomes.slice(0, 12).all(_is_in_progress))
	_check("連続王手 (先手が王手をかけ続ける): 13手目で後手の勝ち",
			outcomes[12] == GameReferee.Outcome.GOTE_WINS_BY_PERPETUAL_CHECK)


func _test_perpetual_check_by_gote() -> void:
	_setup([
		PieceState.new(PieceState.Type.KING, true, 4, 0),
		PieceState.new(PieceState.Type.KING, false, 4, 8),
		PieceState.new(PieceState.Type.ROOK, true, 0, 4),
	], 1)
	var outcomes := _play_all([[0, 4, 4, 4]])
	var cycle := [[4, 8, 5, 8], [4, 4, 5, 4], [5, 8, 4, 8], [5, 4, 4, 4]]
	for i in 3:
		outcomes.append_array(_play_all(cycle))

	_check("連続王手 (後手が王手をかけ続ける): 12手目までは対局が続く", outcomes.slice(0, 12).all(_is_in_progress))
	_check("連続王手 (後手が王手をかけ続ける): 13手目で先手の勝ち",
			outcomes[12] == GameReferee.Outcome.SENTE_WINS_BY_PERPETUAL_CHECK)


func _test_rollback() -> void:
	_setup_rook_and_kings()
	_play(0, 4, 0, 5)
	_play(4, 0, 3, 0)
	var hashes_before := _referee.repetition_tracker.history_hashes()
	var in_checks_before := _referee.repetition_tracker.history_in_checks()

	_play(0, 5, 3, 5)
	var checked_record := _last_record

	var record := _referee.rollback()
	_check("待った: 直前に commit した MoveRecord が返る", record == checked_record)
	_check("待った: 手数が戻る", _game_state.move_count == 2)
	_check("待った: 棋譜が戻る",
			_game_state.move_history.size() == 2 and not _game_state.move_history.has(checked_record))
	_check("待った: 千日手の履歴が戻る",
			_referee.repetition_tracker.history_hashes() == hashes_before
			and _referee.repetition_tracker.history_in_checks() == in_checks_before)

	_game_state.move_piece(record.state, record.from_col, record.from_row)
	_check("待った: 同じ手を指し直すと同じ結果 (CHECK)", _play(0, 5, 3, 5) == GameReferee.Outcome.CHECK)
