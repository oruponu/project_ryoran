extends SceneTree


const START_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
const NON_BOOK_SENTE_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b P 1"
const NON_BOOK_GOTE_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w p 1"


class FixedSfenSerializer extends SfenSerializer:
	var sfen: String

	func _init(value: String) -> void:
		super(GameState.new())
		sfen = value

	func to_sfen() -> String:
		return sfen


class Recorder:
	var analysis: Array = []
	var progress: Array = []
	var completed: Array = []
	var failed := 0

	func _init(worker: EngineWorker) -> void:
		worker.analysis_updated.connect(func(moves: Array) -> void: analysis.append(moves))
		worker.search_progressed.connect(func(moves: Array) -> void: progress.append(moves))
		worker.search_completed.connect(func(move: Dictionary) -> void: completed.append(move))
		worker.search_failed.connect(func() -> void: failed += 1)


var _failures := 0


func _initialize() -> void:
	await _check_engine_depth_signal()
	await _check_engine_stop()
	await _check_progress()
	await _check_book_final()
	await _check_switch()
	await _check_search_during_analysis()
	await _check_stop()
	await _check_stop_during_search()
	await _check_stop_during_response_wait()
	await _check_exit()

	print("== engine_worker_selftest: %s ==" % ("FAIL (%d)" % _failures if _failures > 0 else "ALL PASS"))
	quit(1 if _failures > 0 else 0)


func _expect(label: String, condition: bool) -> void:
	if condition:
		print("PASS: ", label)
	else:
		_failures += 1
		print("FAIL: ", label)


func _wait_frames(count: int) -> void:
	for i in count:
		await process_frame


func _check_engine_depth_signal() -> void:
	var engine := ShogiEngine.new()
	var depths := []
	var last_moves := [[]]
	engine.depth_completed.connect(func(moves: Array, depth: int) -> void:
		depths.append(depth)
		last_moves[0] = moves)
	engine.update_state_from_sfen(NON_BOOK_SENTE_SFEN)
	engine.set_time_limit_msec(300)

	var result: Array = engine.search_top_moves(3)
	await _wait_frames(2)

	_expect("depth_completed: 1回以上届く", not depths.is_empty())
	_expect("depth_completed: 深さ1から順に届く", depths == range(1, depths.size() + 1))
	_expect("depth_completed: 最後に届いた結果が戻り値と一致する", last_moves[0] == result)

	depths.clear()
	engine.update_state_from_sfen(START_SFEN)
	result = engine.search_top_moves(3)
	await _wait_frames(2)
	_expect("depth_completed: 定跡の局面では届かない", depths.is_empty() and not result.is_empty())


func _check_engine_stop() -> void:
	var engine := ShogiEngine.new()
	var depths := []
	engine.depth_completed.connect(func(_moves: Array, depth: int) -> void: depths.append(depth))
	engine.update_state_from_sfen(NON_BOOK_SENTE_SFEN)
	engine.set_time_limit_msec(300)

	engine.stop()
	var result: Array = engine.search_top_moves(3)
	await _wait_frames(2)
	_expect("stop: 探索の前に止めても深さ1だけは読み切る", depths == [1])
	_expect("stop: 深さ1の結果を返す", not result.is_empty())

	depths.clear()
	engine.clear_stop()
	result = engine.search_top_moves(3)
	await _wait_frames(2)
	_expect("clear_stop: 深さ2以降も読む", depths.size() >= 2)


func _wait_until(condition: Callable, timeout_msec: int) -> bool:
	var deadline := Time.get_ticks_msec() + timeout_msec
	while not condition.call():
		if Time.get_ticks_msec() > deadline:
			return false
		await process_frame
	return true


func _make_worker(sfen: String) -> EngineWorker:
	var worker := EngineWorker.new()
	worker.setup(FixedSfenSerializer.new(sfen), RepetitionTracker.new())
	root.add_child(worker)
	return worker


func _same_move(a: Dictionary, b: Dictionary) -> bool:
	for key in ["from_col", "from_row", "to_col", "to_row", "piece_type", "is_promotion", "is_drop"]:
		if a[key] != b[key]:
			return false
	return true


func _all_legal(results: Array, sfen: String) -> bool:
	var legal: Array = ShogiEngine.new().get_legal_moves(sfen)
	for moves in results:
		for move in moves:
			if not legal.any(func(m: Dictionary) -> bool: return _same_move(m, move)):
				return false
	return true


func _check_progress() -> void:
	var worker := _make_worker(NON_BOOK_SENTE_SFEN)
	worker.analysis_time_msec = 1000
	var recorder := Recorder.new(worker)

	worker.request_analysis()
	var finished: bool = await _wait_until(func() -> bool: return worker._thread == null, 5000)

	_expect("解析: 持ち時間で止まる", finished)
	_expect("解析: 途中経過と最終結果で2回以上届く", recorder.analysis.size() >= 2)
	_expect("解析: 結果はどれもその局面の合法手", _all_legal(recorder.analysis, NON_BOOK_SENTE_SFEN))
	_expect("解析: 最終結果は最後の途中経過と同じ",
			recorder.analysis.size() >= 2 and recorder.analysis[-1] == recorder.analysis[-2])
	worker.queue_free()


func _check_book_final() -> void:
	var worker := _make_worker(START_SFEN)
	var recorder := Recorder.new(worker)

	worker.request_analysis()
	await _wait_until(func() -> bool: return worker._thread == null, 5000)

	_expect("定跡: 最終結果だけが1回届く", recorder.analysis.size() == 1)
	_expect("定跡: 定跡手が入っている", recorder.analysis.size() == 1 and not recorder.analysis[0].is_empty())
	worker.queue_free()


func _check_switch() -> void:
	var worker := _make_worker(NON_BOOK_SENTE_SFEN)
	var recorder := Recorder.new(worker)

	worker.request_analysis()
	await _wait_until(func() -> bool: return not recorder.analysis.is_empty(), 5000)

	worker._sfen_serializer.sfen = NON_BOOK_GOTE_SFEN
	worker.analysis_time_msec = 1000
	var before := recorder.analysis.size()
	worker.request_analysis()
	var finished: bool = await _wait_until(func() -> bool: return worker._thread == null, 5000)
	var after := recorder.analysis.slice(before)

	_expect("局面の切り替え: 前の解析の持ち時間を待たずに新しい解析が終わる", finished)
	_expect("局面の切り替え: 新しい局面の結果が届く", not after.is_empty())
	_expect("局面の切り替え: 前の局面の結果は届かない", _all_legal(after, NON_BOOK_GOTE_SFEN))
	worker.queue_free()


func _check_search_during_analysis() -> void:
	var worker := _make_worker(NON_BOOK_SENTE_SFEN)
	var recorder := Recorder.new(worker)

	worker.request_analysis()
	await _wait_until(func() -> bool: return not recorder.analysis.is_empty(), 5000)

	worker.request_search()
	var done: bool = await _wait_until(func() -> bool: return not recorder.completed.is_empty(), 5000)

	_expect("AIの探索: 解析の持ち時間を待たずに指し手が届く", done)
	_expect("AIの探索: 指し手はその局面の合法手", done and _all_legal([[recorder.completed[0]]], NON_BOOK_SENTE_SFEN))
	_expect("AIの探索: 途中経過も届く", not recorder.progress.is_empty())
	_expect("AIの探索: 起動に失敗しない", recorder.failed == 0)
	worker.queue_free()


func _check_stop() -> void:
	var worker := _make_worker(NON_BOOK_SENTE_SFEN)
	var recorder := Recorder.new(worker)

	worker.request_analysis()
	await _wait_until(func() -> bool: return not recorder.analysis.is_empty(), 5000)

	worker.stop()
	var before := recorder.analysis.size()
	var finished: bool = await _wait_until(func() -> bool: return worker._thread == null, 1000)
	await _wait_frames(5)

	_expect("stop: 1秒以内に止まる", finished)
	_expect("stop: 止めた後は何も届かない", recorder.analysis.size() == before)
	worker.queue_free()


func _check_stop_during_search() -> void:
	var worker := _make_worker(NON_BOOK_SENTE_SFEN)
	var recorder := Recorder.new(worker)

	worker.request_search()
	worker.stop()
	await _wait_until(func() -> bool: return worker._thread == null, 3000)
	await create_timer(GameConfig.MIN_AI_RESPONSE_TIME_SEC + 0.2).timeout

	_expect("stop: AIの探索の途中経過は届かない", recorder.progress.is_empty())
	_expect("stop: AIの探索の指し手は届かない", recorder.completed.is_empty())
	worker.queue_free()


func _check_stop_during_response_wait() -> void:
	# 定跡の局面ならAIの探索はすぐ終わり、最低応答時間を待つ
	var worker := _make_worker(START_SFEN)
	var recorder := Recorder.new(worker)

	worker.request_search()
	await _wait_until(func() -> bool: return worker._thread == null, 3000)
	_expect("待ち中のstop: 前提として最低応答時間を待っている", recorder.completed.is_empty())

	worker.stop()
	await create_timer(GameConfig.MIN_AI_RESPONSE_TIME_SEC + 0.2).timeout
	_expect("待ち中のstop: AIの指し手は届かない", recorder.completed.is_empty())
	worker.queue_free()


func _check_exit() -> void:
	var worker := _make_worker(NON_BOOK_SENTE_SFEN)
	var recorder := Recorder.new(worker)

	worker.request_analysis()
	await _wait_until(func() -> bool: return not recorder.analysis.is_empty(), 5000)

	var started := Time.get_ticks_msec()
	root.remove_child(worker)
	var elapsed := Time.get_ticks_msec() - started
	worker.queue_free()
	await _wait_frames(2)

	_expect("終了: 解析中でも待たされない", elapsed < 1000)
