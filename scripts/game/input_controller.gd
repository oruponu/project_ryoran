class_name InputController

extends RefCounted


signal move_submitted(piece: Piece, col: int, row: int, mode: PromotionMode.Type)


var holding_piece: Piece = null

var _piece_moves: Array[Dictionary] = []
var _game_state: GameState
var _board: Board
var _shogi_engine: ShogiEngine
var _sfen_serializer: SfenSerializer


func _init(game_state: GameState, board: Board, shogi_engine: ShogiEngine, sfen_serializer: SfenSerializer) -> void:
	_game_state = game_state
	_board = board
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


func handle_click(piece: Piece) -> void:
	if holding_piece == null:
		_pick_up(piece)
	else:
		_attempt_place(holding_piece)


func release_holding(piece: Piece) -> void:
	piece.is_held = false
	piece.z_index = 0

	_board.clear_guides()

	holding_piece = null


func cancel_holding() -> void:
	if holding_piece == null:
		return

	_cancel_move(holding_piece, true)


func reset() -> void:
	holding_piece = null
	_piece_moves = []


func _pick_up(piece: Piece) -> void:
	if piece.state.is_gote != _game_state.is_gote_turn():
		return

	holding_piece = piece
	piece.is_held = true
	piece.z_index = 10

	if piece.state.is_in_hand():
		var stand := piece.get_parent()
		if stand is PieceStand:
			stand.update_layout()

	_piece_moves = moves_for(_shogi_engine.get_legal_moves(_sfen_serializer.to_sfen()), piece.state)
	_board.show_guides(destinations_of(_piece_moves))


func _attempt_place(piece: Piece) -> void:
	var target_coord := GameConfig.position_to_cell(_board.to_local(piece.global_position))
	var moves := moves_to(_piece_moves, target_coord)

	# 合法手でないならキャンセル
	if moves.is_empty():
		_cancel_move(piece)
		return

	move_submitted.emit(piece, target_coord.x, target_coord.y, PromotionMode.from_moves(moves))


func _cancel_move(piece: Piece, immediate: bool = false) -> void:
	piece.is_held = false
	piece.z_index = 0

	_board.clear_guides()

	holding_piece = null

	if piece.state.is_in_hand():
		var stand := piece.get_parent()
		if stand is PieceStand:
			stand.update_layout(immediate)
	else:
		piece.position = GameConfig.cell_to_position(piece.state.current_col, piece.state.current_row)
