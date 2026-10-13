extends SceneTree


const START_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
const NON_BOOK_SENTE_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b P 1"
const NON_BOOK_GOTE_SFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w p 1"


var _failures := 0


func _initialize() -> void:
	await _check_engine_depth_signal()
	await _check_engine_stop()

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
