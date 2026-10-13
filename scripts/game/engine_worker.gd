class_name EngineWorker

extends Node


signal analysis_updated(moves: Array)
signal search_progressed(moves: Array)
signal search_completed(move: Dictionary)
signal search_failed


enum Kind { SEARCH, ANALYSIS }


var analysis_time_msec: int = GameConfig.ANALYSIS_TIME_MSEC

var _sfen_serializer: SfenSerializer
var _repetition_tracker: RepetitionTracker
var _engine: ShogiEngine = ShogiEngine.new()
var _thread: Thread
var _running_kind: Kind = Kind.ANALYSIS
var _is_discarded: bool = false
var _next_kind: Kind = Kind.ANALYSIS
var _has_next_request: bool = false
var _generation: int = 0
var _search_started_msec: int = 0


func _exit_tree() -> void:
	if _thread != null:
		_engine.stop()
		_thread.wait_to_finish()
		_thread = null


func setup(sfen_serializer: SfenSerializer, repetition_tracker: RepetitionTracker) -> void:
	_sfen_serializer = sfen_serializer
	_repetition_tracker = repetition_tracker
	_engine.depth_completed.connect(_on_depth_completed)


func request_search() -> void:
	_request(Kind.SEARCH)


func request_analysis() -> void:
	_request(Kind.ANALYSIS)


func stop() -> void:
	_has_next_request = false
	_generation += 1
	if _thread != null:
		_engine.stop()
		_is_discarded = true


func _request(kind: Kind) -> void:
	_next_kind = kind
	_has_next_request = true
	if _thread == null:
		_start_next()
	elif _running_kind == Kind.ANALYSIS:
		_engine.stop()
		_is_discarded = true


func _start_next() -> void:
	if not _has_next_request:
		return
	_has_next_request = false

	var is_search := _next_kind == Kind.SEARCH
	_engine.clear_stop()
	_engine.update_state_from_sfen(_sfen_serializer.to_sfen())
	_engine.set_game_history(_repetition_tracker.history_hashes(), _repetition_tracker.history_in_checks())
	_engine.set_time_limit_msec(GameConfig.AI_SEARCH_TIME_MSEC if is_search else analysis_time_msec)
	var count := 1 if is_search else GameConfig.HINT_CANDIDATE_COUNT

	_running_kind = _next_kind
	_is_discarded = false
	_thread = Thread.new()
	var error := _thread.start(_run.bind(count))
	if error != OK:
		_thread = null
		push_error("探索スレッドを起動できません（%s）" % error_string(error))
		if is_search:
			search_failed.emit()
		return

	if is_search:
		_search_started_msec = Time.get_ticks_msec()


func _run(count: int) -> void:
	var moves: Array = _engine.search_top_moves(count)
	_on_thread_finished.call_deferred(moves)


func _on_depth_completed(moves: Array, _depth: int) -> void:
	if _thread == null or _is_discarded:
		return

	if _running_kind == Kind.SEARCH:
		search_progressed.emit(moves)
	else:
		analysis_updated.emit(moves)


func _on_thread_finished(moves: Array) -> void:
	if _thread == null:
		return
	_thread.wait_to_finish()
	_thread = null

	var kind := _running_kind
	var is_discarded := _is_discarded
	var generation := _generation
	_start_next()

	if is_discarded:
		return

	if kind == Kind.ANALYSIS:
		analysis_updated.emit(moves)
		return

	var elapsed_sec := (Time.get_ticks_msec() - _search_started_msec) / 1000.0
	var remaining_sec := GameConfig.MIN_AI_RESPONSE_TIME_SEC - elapsed_sec
	if remaining_sec > 0.0:
		await get_tree().create_timer(remaining_sec).timeout
		if generation != _generation:
			return

	# 投了は空のDictionary
	search_completed.emit(moves[0] if not moves.is_empty() else {})
