class_name EngineWorker

extends Node


signal search_completed(move: Dictionary)
signal analysis_completed(moves: Array)


var _sfen_serializer: SfenSerializer
var _repetition_tracker: RepetitionTracker
var _search_engine: ShogiEngine = ShogiEngine.new()
var _search_thread: Thread
var _analysis_engine: ShogiEngine = ShogiEngine.new()
var _analysis_thread: Thread
var _analysis_pending: bool = false
var _analysis_suspended: bool = false
var _generation: int = 0
var _search_started_msec: int = 0


func _exit_tree() -> void:
	if _search_thread != null and _search_thread.is_started():
		_search_thread.wait_to_finish()
		_search_thread = null

	if _analysis_thread != null and _analysis_thread.is_started():
		_analysis_thread.wait_to_finish()
		_analysis_thread = null


func setup(sfen_serializer: SfenSerializer, repetition_tracker: RepetitionTracker) -> void:
	_sfen_serializer = sfen_serializer
	_repetition_tracker = repetition_tracker
	_analysis_engine.set_time_limit_msec(GameConfig.HINT_ANALYSIS_TIME_MSEC)


func request_search() -> bool:
	if _search_thread != null:
		push_error("AI探索スレッドを起動できません（既に実行中です）")
		return false

	_search_engine.update_state_from_sfen(_sfen_serializer.to_sfen())
	_search_engine.set_game_history(_repetition_tracker.history_hashes(), _repetition_tracker.history_in_checks())

	var generation := _generation
	_search_thread = Thread.new()
	var error := _search_thread.start(_run_search.bind(generation))
	if error != OK:
		_search_thread = null
		push_error("AI探索スレッドを起動できません（%s）" % error_string(error))
		return false

	_search_started_msec = Time.get_ticks_msec()
	return true


func request_analysis() -> void:
	if _analysis_suspended or _analysis_thread != null:
		_analysis_pending = true
		return

	var sfen := _sfen_serializer.to_sfen()
	_analysis_engine.update_state_from_sfen(sfen)
	_analysis_engine.set_game_history(_repetition_tracker.history_hashes(), _repetition_tracker.history_in_checks())

	var generation := _generation
	_analysis_thread = Thread.new()
	var error := _analysis_thread.start(_run_analysis.bind(generation, sfen))
	if error != OK:
		_analysis_thread = null
		push_error("評価解析スレッドを起動できません（%s）" % error_string(error))


func reset() -> void:
	_generation += 1
	_analysis_pending = false
	_analysis_suspended = false


func suspend_analysis() -> void:
	_analysis_suspended = true


func resume_analysis() -> void:
	_analysis_suspended = false
	_analysis_pending = false


func _run_search(generation: int) -> void:
	var move: Dictionary = _search_engine.search_best_move()
	call_deferred("_on_search_finished", move, generation)


func _on_search_finished(move: Dictionary, generation: int) -> void:
	_search_thread.wait_to_finish()
	_search_thread = null

	if generation != _generation:
		return

	var elapsed_sec := (Time.get_ticks_msec() - _search_started_msec) / 1000.0
	var remaining_sec := GameConfig.MIN_AI_RESPONSE_TIME_SEC - elapsed_sec
	if remaining_sec > 0.0:
		await get_tree().create_timer(remaining_sec).timeout
		if generation != _generation:
			return

	search_completed.emit(move)


func _run_analysis(generation: int, sfen: String) -> void:
	var moves: Array = _analysis_engine.search_top_moves(GameConfig.HINT_CANDIDATE_COUNT)
	call_deferred("_on_analysis_finished", moves, generation, sfen)


func _on_analysis_finished(moves: Array, generation: int, sfen: String) -> void:
	_analysis_thread.wait_to_finish()
	_analysis_thread = null

	# 最新局面の解析だけを通知
	var is_current := generation == _generation and sfen == _sfen_serializer.to_sfen()
	if is_current and not _analysis_pending and not _analysis_suspended:
		analysis_completed.emit(moves)

	if _analysis_pending:
		_analysis_pending = false
		request_analysis()
