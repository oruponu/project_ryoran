class_name InputController

extends RefCounted


signal move_submitted(state: PieceState, col: int, row: int, mode: PromotionMode.Type)


const OUTSIDE = Vector2i(-1, -1)


var _holding_state: PieceState = null
var _piece_moves: Array[Dictionary] = []

var _game_state: GameState
var _board: Board
var _board_view: BoardView
var _shogi_engine: ShogiEngine
var _sfen_serializer: SfenSerializer


func _init(
	game_state: GameState,
	board: Board,
	board_view: BoardView,
	shogi_engine: ShogiEngine,
	sfen_serializer: SfenSerializer
) -> void:
	_game_state = game_state
	_board = board
	_board_view = board_view
	_shogi_engine = shogi_engine
	_sfen_serializer = sfen_serializer


static func moves_for(moves: Array, state: PieceState) -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	for move: Dictionary in moves:
		if state.is_in_hand():
			if move.is_drop and move.piece_type == state.piece_type:
				result.append(move)
		elif not move.is_drop and move.from_col == state.current_col and move.from_row == state.current_row:
			result.append(move)
	return result


static func moves_to(moves: Array[Dictionary], coord: Vector2i) -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	for move in moves:
		if move.to_col == coord.x and move.to_row == coord.y:
			result.append(move)
	return result


static func destinations_of(moves: Array[Dictionary]) -> Array[Vector2i]:
	var result: Array[Vector2i] = []
	for move in moves:
		var coord := Vector2i(move.to_col, move.to_row)
		if not coord in result:
			result.append(coord)
	return result


func handle_tap(coord: Vector2i, state: PieceState) -> void:
	if _holding_state == null:
		if state != null:
			_pick_up(state)
		return

	var moves := moves_to(_piece_moves, coord)
	if not moves.is_empty():
		_submit(coord, moves)
		return

	_cancel_move()


func cancel_holding() -> void:
	if _holding_state == null:
		return

	_cancel_move(true)


func reset() -> void:
	_holding_state = null
	_piece_moves = []


func _pick_up(state: PieceState) -> void:
	if state.is_gote != _game_state.is_gote_turn():
		return

	_holding_state = state
	_board_view.hold(state)

	_piece_moves = moves_for(_shogi_engine.get_legal_moves(_sfen_serializer.to_sfen()), state)
	_board.show_guides(destinations_of(_piece_moves))


func _submit(coord: Vector2i, moves: Array[Dictionary]) -> void:
	var state := _holding_state
	_clear_holding()
	move_submitted.emit(state, coord.x, coord.y, PromotionMode.from_moves(moves))


func _cancel_move(immediate: bool = false) -> void:
	var state := _holding_state
	_clear_holding()
	_board_view.release(state, immediate)


func _clear_holding() -> void:
	_board.clear_guides()
	_holding_state = null
	_piece_moves = []
