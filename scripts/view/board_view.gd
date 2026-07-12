class_name BoardView

extends RefCounted


var _game_state: GameState
var _board: Board
var _player_piece_stand: PieceStand
var _enemy_piece_stand: PieceStand
var _piece_scene: PackedScene

var _nodes: Dictionary = {}


func _init(
	game_state: GameState,
	board: Board,
	player_piece_stand: PieceStand,
	enemy_piece_stand: PieceStand,
	piece_scene: PackedScene
) -> void:
	_game_state = game_state
	_board = board
	_player_piece_stand = player_piece_stand
	_enemy_piece_stand = enemy_piece_stand
	_piece_scene = piece_scene


func register(state: PieceState, piece: Piece) -> void:
	if _nodes.has(state):
		push_error("BoardView.register: 二重登録です")
	_nodes[state] = piece


func node_for(state: PieceState) -> Piece:
	var piece: Piece = _nodes.get(state)
	if piece == null:
		push_error("BoardView.node_for: 未登録のPieceStateです")
	return piece


func clear() -> void:
	_nodes.clear()


func place_piece(state: PieceState) -> void:
	var piece := node_for(state)
	_sync_mirror(piece, state)
	piece.position = GameConfig.cell_to_position(state.current_col, state.current_row)


func capture_piece(state: PieceState) -> void:
	var piece := node_for(state)
	_sync_mirror(piece, state)
	var stand := _enemy_piece_stand if state.is_enemy else _player_piece_stand
	stand.add_piece(piece)


func drop_piece(state: PieceState) -> void:
	var piece := node_for(state)
	var source_stand := piece.get_parent()

	piece.reparent(_board)
	piece.visible = true

	_sync_mirror(piece, state)
	piece.position = GameConfig.cell_to_position(state.current_col, state.current_row)

	if source_stand is PieceStand:
		source_stand.update_layout()


func return_to_stand(state: PieceState) -> void:
	var piece := node_for(state)
	_sync_mirror(piece, state)

	if state.is_enemy:
		_enemy_piece_stand.add_piece(piece, true)
	else:
		_player_piece_stand.add_piece(piece, true)


func revive_piece(state: PieceState) -> void:
	var piece := node_for(state)
	var source_stand := piece.get_parent()

	piece.reparent(_board)
	piece.visible = true
	_sync_mirror(piece, state)
	piece.rotation_degrees = 180 if state.is_enemy else 0

	if state.is_promoted:
		piece.set_promoted(true)

	piece.position = GameConfig.cell_to_position(state.current_col, state.current_row)

	if source_stand is PieceStand:
		source_stand.update_layout(true)


func refresh_display(state: PieceState) -> void:
	var piece := node_for(state)
	_sync_mirror(piece, state)
	piece.refresh_display()


func _sync_mirror(piece: Piece, state: PieceState) -> void:
	piece.piece_type = state.piece_type
	piece.is_enemy = state.is_enemy
	piece.is_promoted = state.is_promoted
	piece.current_col = state.current_col
	piece.current_row = state.current_row
