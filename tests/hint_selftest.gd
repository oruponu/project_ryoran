extends SceneTree


const START_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
const NON_BOOK_SENTE_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b P 1"
const NON_BOOK_GOTE_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w p 1"
const MATE_IN_ONE_SFEN = "4k4/9/4G4/9/9/9/9/9/4K4 b G 1"
const MATED_SFEN = "4k4/9/9/9/4l4/9/9/4g4/4K4 b - 1"
const MIDGAME_GOTE_SFEN = "l6nl/5+P1gk/2np1S3/p1p4Pp/3P2Sp1/1PPb2P1P/P5GS1/R8/LN4bKL w RGgsn5p 1"
const MIDGAME_SENTE_SFEN = "lkB4nl/8r/1sg5p/p1p2Bpp1/1Ps2p3/Pp4P1P/3s1PN2/KG1+p5/LN6L b rgGSN5P 1"


var _failures := 0


func _initialize() -> void:
	_check_book()
	_check_ordering("非定跡・先手番", NON_BOOK_SENTE_SFEN)
	_check_ordering("非定跡・後手番", NON_BOOK_GOTE_SFEN)
	_check_ordering("中盤・後手番", MIDGAME_GOTE_SFEN)
	_check_ordering("中盤・先手番", MIDGAME_SENTE_SFEN)
	_check_mate()
	_check_mated()
	_check_invalid_count()
	_check_arrow_lifecycle()
	await _check_filter()
	await _check_promotion_decision()
	await _check_move_submission_stops_analysis()
	await _check_end_game_stops_analysis()
	await _check_search_failed()
	await _check_arrows_follow_analysis()
	await _check_search_progress_bar()

	print("== hint_selftest: %s ==" % ("FAIL (%d)" % _failures if _failures > 0 else "ALL PASS"))
	quit(1 if _failures > 0 else 0)


func _search(sfen: String, count: int) -> Array:
	var engine := ShogiEngine.new()
	engine.update_state_from_sfen(sfen)
	return engine.search_top_moves(count)


func _expect(label: String, condition: bool) -> void:
	if condition:
		print("PASS: ", label)
	else:
		_failures += 1
		print("FAIL: ", label)


func _check_book() -> void:
	var moves := _search(START_SFEN, 3)
	print("開始局面の候補: ", moves)
	_expect("開始局面: 1〜3件", moves.size() >= 1 and moves.size() <= 3)


func _check_ordering(label: String, sfen: String) -> void:
	var moves := _search(sfen, 3)
	print("%s の候補: %s" % [label, moves])
	_expect("%s: 1〜3件" % label, moves.size() >= 1 and moves.size() <= 3)
	for move in moves:
		_expect("%s: score キーあり" % label, move.has("score"))
		_expect("%s: win_rate が0〜1" % label, move.win_rate >= 0.0 and move.win_rate <= 1.0)
	for i in range(1, moves.size()):
		_expect("%s: %d位>=%d位" % [label, i, i + 1], moves[i - 1].win_rate >= moves[i].win_rate)


func _check_mate() -> void:
	var moves := _search(MATE_IN_ONE_SFEN, 3)
	print("1手詰めの局面の候補: ", moves)
	_expect("1手詰めの局面: 候補は1件のみ", moves.size() == 1)
	_expect("1手詰めの局面: win_rate 1.0", moves.size() == 1 and moves[0].win_rate >= 0.99)


func _check_mated() -> void:
	var moves := _search(MATED_SFEN, 3)
	_expect("詰んだ局面: 空配列", moves.is_empty())


func _check_invalid_count() -> void:
	var moves := _search(NON_BOOK_SENTE_SFEN, 0)
	_expect("count=0: 空配列", moves.is_empty())


func _move(win_rate: float) -> Dictionary:
	return {
		"from_col": 2, "from_row": 6, "to_col": 2, "to_row": 5,
		"piece_type": 7, "is_promotion": false, "is_drop": false,
		"score": 0, "win_rate": win_rate,
	}


func _check_filter() -> void:
	var scene: PackedScene = load("res://scenes/main/main.tscn")
	var main: Node2D = scene.instantiate()
	root.add_child(main)
	await process_frame

	var worker: EngineWorker = main.engine_worker
	worker.stop()
	_expect("シグナル: analysis_updated が main に接続されている",
			worker.analysis_updated.is_connected(main._on_analysis_updated))

	var margin: float = GameConfig.HINT_WIN_RATE_MARGIN

	worker.analysis_updated.emit([_move(0.60), _move(0.60 - margin * 0.9), _move(0.60 - margin * 1.1)])
	_expect("フィルター: 勝率差が閾値内の手だけを残す", main._hint_moves.size() == 2)

	worker.analysis_updated.emit([_move(0.60), _move(0.60 - margin * 0.5), _move(0.60 - margin * 0.9)])
	_expect("フィルター: すべて閾値内なら3件", main._hint_moves.size() == 3)

	worker.analysis_updated.emit([_move(0.60), _move(0.10), _move(0.05)])
	_expect("フィルター: 勝率の低い手を除いて1件", main._hint_moves.size() == 1)
	_expect("フィルター: 1件でもヒントボタンは有効", not main.hint_button.disabled)

	worker.analysis_updated.emit([])
	_expect("フィルター: 結果が空なら候補なし", main._hint_moves.is_empty())
	_expect("フィルター: 結果が空ならヒントボタンは無効", main.hint_button.disabled)

	main.queue_free()


func _check_arrow_lifecycle() -> void:
	var board := Board.new()
	root.add_child(board)

	var entries: Array[Dictionary] = [
		{"from": Vector2(0, 0), "to": Vector2(70, 70)},
		{"from": Vector2(70, 0), "to": Vector2(0, 70)},
		{"from": Vector2(140, 0), "to": Vector2(140, 70)},
	]

	board.show_hint_arrows(entries)
	_expect("矢印: 3本生成", board._hint_arrows.size() == 3)

	board.show_hint_arrows(entries.slice(0, 1))
	_expect("矢印: もう一度表示すると前の矢印は置き換わる (1本)", board._hint_arrows.size() == 1)

	board.clear_hint_arrows()
	_expect("矢印: まとめて消える", board._hint_arrows.is_empty())

	board.show_hint_arrows([])
	_expect("矢印: 空エントリで0本", board._hint_arrows.is_empty())

	board.queue_free()


func _check_promotion_decision() -> void:
	var scene: PackedScene = load("res://scenes/main/main.tscn")
	var main: Node2D = scene.instantiate()
	root.add_child(main)
	await process_frame

	var answer := func() -> void: main.common_dialog._on_yes_pressed()
	answer.call_deferred()

	var should_promote: bool = await main._decide_promotion()
	_expect("成りの確認: 「成る」を選ぶと true", should_promote)

	main.queue_free()


func _check_move_submission_stops_analysis() -> void:
	var scene: PackedScene = load("res://scenes/main/main.tscn")
	var main: Node2D = scene.instantiate()
	root.add_child(main)
	await process_frame

	var worker: EngineWorker = main.engine_worker
	worker.request_analysis()
	_expect("着手: 前提として解析が走っている", worker._thread != null)

	var state: PieceState = main.game_state.get_piece(2, 6)
	var generation: int = worker._generation
	main._on_move_submitted(state, 2, 5, PromotionMode.Type.FORCE_STAY)
	_expect("着手: 受け付けた時点で解析を破棄する", worker._generation > generation)

	var deadline := Time.get_ticks_msec() + 3000
	while not main.is_ai_thinking and Time.get_ticks_msec() < deadline:
		await process_frame
	main.queue_free()
	for i in 10:
		await process_frame


func _check_end_game_stops_analysis() -> void:
	var scene: PackedScene = load("res://scenes/main/main.tscn")
	var main: Node2D = scene.instantiate()
	root.add_child(main)
	await process_frame

	var worker: EngineWorker = main.engine_worker
	worker.request_analysis()
	_expect("対局の終了: 前提として解析が走っている", worker._thread != null)

	var close_dialog := func() -> void: main.common_dialog._on_yes_pressed()
	close_dialog.call_deferred()
	main._end_game(GameEnding.Type.RESIGNATION, false)
	_expect("対局の終了: 結果を出す前に解析を破棄する", worker._is_discarded and not worker._has_next_request)

	var deadline := Time.get_ticks_msec() + 3000
	while main.is_game_active and Time.get_ticks_msec() < deadline:
		await process_frame
	main.queue_free()
	await process_frame


func _check_search_failed() -> void:
	var scene: PackedScene = load("res://scenes/main/main.tscn")
	var main: Node2D = scene.instantiate()
	root.add_child(main)
	await process_frame

	var worker: EngineWorker = main.engine_worker
	worker.stop()
	main.is_ai_thinking = true
	worker.search_failed.emit()
	_expect("search_failed: AIの思考中の状態を解除する", not main.is_ai_thinking)

	main.queue_free()
	await process_frame


func _check_arrows_follow_analysis() -> void:
	var scene: PackedScene = load("res://scenes/main/main.tscn")
	var main: Node2D = scene.instantiate()
	root.add_child(main)
	await process_frame

	var worker: EngineWorker = main.engine_worker
	worker.stop()

	worker.analysis_updated.emit([_move(0.60), _move(0.60), _move(0.60)])
	_expect("矢印の追従: ボタンを押すまでは描かない", main.board._hint_arrows.is_empty())

	main._on_hint_button_pressed()
	_expect("矢印の追従: ボタンを押すと3本描く", main.board._hint_arrows.size() == 3)

	worker.analysis_updated.emit([_move(0.60), _move(0.10)])
	_expect("矢印の追従: 表示中は新しい候補で描き直す (1本)", main.board._hint_arrows.size() == 1)

	worker.analysis_updated.emit([])
	_expect("矢印の追従: 結果が空なら矢印を消す", main.board._hint_arrows.is_empty())
	_expect("矢印の追従: 結果が空ならヒントボタンは無効", main.hint_button.disabled)

	worker.analysis_updated.emit([_move(0.60)])
	main._on_hint_button_pressed()
	main._on_layout_changed(false)
	worker.analysis_updated.emit([_move(0.60), _move(0.60)])
	_expect("矢印の追従: レイアウトの変更で消した後は描き直さない", main.board._hint_arrows.is_empty())

	main._on_hint_button_pressed()
	main._clear_hint()
	worker.analysis_updated.emit([_move(0.60)])
	_expect("矢印の追従: ヒントを消した後は描き直さない", main.board._hint_arrows.is_empty())

	main.queue_free()
	await process_frame


func _check_search_progress_bar() -> void:
	var scene: PackedScene = load("res://scenes/main/main.tscn")
	var main: Node2D = scene.instantiate()
	root.add_child(main)
	await process_frame

	var worker: EngineWorker = main.engine_worker
	worker.stop()
	_expect("AIの途中経過: search_progressed が main に接続されている",
			worker.search_progressed.is_connected(main._on_search_progressed))

	var hints_before: Array[Dictionary] = main._hint_moves.duplicate()
	worker.search_progressed.emit([_move(0.75)])
	_expect("AIの途中経過: バーが手番側の勝率で動く", main.win_rate_bar.sente_value_label.text == "75")
	_expect("AIの途中経過: ヒントの候補は変えない", main._hint_moves == hints_before)

	worker.search_progressed.emit([])
	_expect("AIの途中経過: 結果が空ならバーは動かさない", main.win_rate_bar.sente_value_label.text == "75")

	main.queue_free()
