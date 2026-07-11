class_name EngineWorker

extends Node


const AI_IS_ENEMY := true


signal search_completed(move: Dictionary)
signal analysis_completed(win_rate: float)


var _serializer: SfenSerializer
var _tracker: RepetitionTracker
var _ai_engine: ShogiEngine = ShogiEngine.new()
var _ai_thread: Thread
var _eval_engine: ShogiEngine = ShogiEngine.new()
var _eval_thread: Thread
var _analysis_pending: bool = false
var _analysis_suspended: bool = false
var _generation: int = 0


func _exit_tree() -> void:
	if _ai_thread != null and _ai_thread.is_started():
		_ai_thread.wait_to_finish()
		_ai_thread = null

	if _eval_thread != null and _eval_thread.is_started():
		_eval_thread.wait_to_finish()
		_eval_thread = null


func setup(serializer: SfenSerializer, tracker: RepetitionTracker) -> void:
	_serializer = serializer
	_tracker = tracker


func request_search() -> bool:
	if _ai_thread != null:
		push_error("AI探索スレッドを起動できません（既に実行中です）")
		return false

	_ai_engine.update_state_from_sfen(_serializer.to_sfen())
	_ai_engine.set_game_history(_tracker.history_hashes(), _tracker.history_in_checks())

	var generation := _generation
	_ai_thread = Thread.new()
	var err := _ai_thread.start(_run_search.bind(generation))
	if err != OK:
		_ai_thread = null
		push_error("AI探索スレッドを起動できません（%s）" % error_string(err))
		return false

	return true


func request_analysis() -> void:
	if _analysis_suspended or _eval_thread != null:
		_analysis_pending = true
		return

	_eval_engine.update_state_from_sfen(_serializer.to_sfen())
	_eval_engine.set_game_history(_tracker.history_hashes(), _tracker.history_in_checks())

	var generation := _generation
	_eval_thread = Thread.new()
	var err := _eval_thread.start(_run_analysis.bind(generation))
	if err != OK:
		_eval_thread = null
		push_error("評価解析スレッドを起動できません（%s）" % error_string(err))


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
	var move: Dictionary = _ai_engine.search_best_move()
	call_deferred("_on_search_finished", move, generation)


func _on_search_finished(move: Dictionary, generation: int) -> void:
	_ai_thread.wait_to_finish()
	_ai_thread = null

	if generation == _generation:
		search_completed.emit(move)


func _run_analysis(generation: int) -> void:
	var move: Dictionary = _eval_engine.search_best_move()
	call_deferred("_on_analysis_finished", move, generation)


func _on_analysis_finished(move: Dictionary, generation: int) -> void:
	_eval_thread.wait_to_finish()
	_eval_thread = null

	# 最新局面の解析だけを通知
	if generation == _generation and not _analysis_pending and not _analysis_suspended:
		analysis_completed.emit(move.get("win_rate", 0.0))

	if _analysis_pending:
		_analysis_pending = false
		request_analysis()
