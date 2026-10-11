class_name GameReferee

extends RefCounted


enum Outcome {
	CONTINUE,
	CHECK,
	CHECKMATE,
	SENNICHITE,
	SENTE_WINS_BY_PERPETUAL_CHECK,
	GOTE_WINS_BY_PERPETUAL_CHECK
}


var repetition_tracker := RepetitionTracker.new()

var _game_state: GameState
var _sfen_serializer: SfenSerializer
var _shogi_engine: ShogiEngine


func _init(game_state: GameState, sfen_serializer: SfenSerializer, shogi_engine: ShogiEngine) -> void:
	_game_state = game_state
	_sfen_serializer = sfen_serializer
	_shogi_engine = shogi_engine


func start() -> void:
	var position := _inspect_current_position()
	repetition_tracker.reset(position["hash"], position["in_check"])


func commit(record: MoveRecord) -> Outcome:
	_game_state.push_move(record)

	var position := _inspect_current_position()
	var in_check: bool = position["in_check"]
	var repetition_count := repetition_tracker.record(position["hash"], in_check)

	if repetition_count >= GameConfig.SENNICHITE_COUNT:
		return _classify_sennichite()

	if in_check:
		if not position["has_legal_move"]:
			return Outcome.CHECKMATE
		return Outcome.CHECK

	return Outcome.CONTINUE


func rollback() -> MoveRecord:
	repetition_tracker.undo()
	return _game_state.pop_move()


func _inspect_current_position() -> Dictionary:
	return _shogi_engine.inspect_position(_sfen_serializer.to_sfen())


func _classify_sennichite() -> Outcome:
	var is_gote_turn := _game_state.is_gote_turn()
	match repetition_tracker.classify_sennichite():
		RepetitionTracker.Sennichite.TURN_PERPETUAL:
			return Outcome.SENTE_WINS_BY_PERPETUAL_CHECK if is_gote_turn else Outcome.GOTE_WINS_BY_PERPETUAL_CHECK
		RepetitionTracker.Sennichite.OPPONENT_PERPETUAL:
			return Outcome.GOTE_WINS_BY_PERPETUAL_CHECK if is_gote_turn else Outcome.SENTE_WINS_BY_PERPETUAL_CHECK
	return Outcome.SENNICHITE
