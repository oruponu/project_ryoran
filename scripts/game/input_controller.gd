class_name InputController

extends RefCounted


signal move_submitted(piece: Piece, col: int, row: int)


var holding_piece: Piece = null
var current_legal_coords: Array[Vector2i] = []

var _game_state: GameState
var _board: Board
var _shogi_engine: ShogiEngine
var _serializer: SfenSerializer


func _init(game_state: GameState, board: Board, shogi_engine: ShogiEngine, serializer: SfenSerializer) -> void:
	_game_state = game_state
	_board = board
	_shogi_engine = shogi_engine
	_serializer = serializer


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


func reset() -> void:
	holding_piece = null
	current_legal_coords = []


func _pick_up(piece: Piece) -> void:
	if piece.is_enemy != _game_state.is_gote_turn():
		return

	holding_piece = piece
	piece.is_held = true
	piece.z_index = 10

	current_legal_coords = []
	var sfen := _serializer.to_sfen()
	if piece.is_in_hand():
		var stand := piece.get_parent()
		if stand is PieceStand:
			stand.update_layout()
		current_legal_coords = _shogi_engine.get_legal_drops(sfen, piece.piece_type)
	else:
		current_legal_coords = _shogi_engine.get_legal_moves(sfen, piece.current_col, piece.current_row)

	_board.show_guides(current_legal_coords)


func _attempt_place(piece: Piece) -> void:
	var target_pos := GameConfig.position_to_cell(_board.to_local(piece.global_position))

	# 合法手でないならキャンセル
	if not target_pos in current_legal_coords:
		_cancel_move(piece)
		return

	move_submitted.emit(piece, target_pos.x, target_pos.y)


func _cancel_move(piece: Piece) -> void:
	piece.is_held = false
	piece.z_index = 0

	_board.clear_guides()

	holding_piece = null

	if piece.is_in_hand():
		var stand := piece.get_parent()
		if stand is PieceStand:
			stand.update_layout()
	else:
		piece.position = GameConfig.cell_to_position(piece.current_col, piece.current_row)
