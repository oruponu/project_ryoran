class_name BoardView

extends RefCounted


signal piece_spawned(piece: Piece)


var _game_state: GameState
var _board: Board
var _sente_piece_stand: PieceStand
var _gote_piece_stand: PieceStand
var _piece_scene: PackedScene

var _nodes: Dictionary[PieceState, Piece] = {}


func _init(
	game_state: GameState,
	board: Board,
	sente_piece_stand: PieceStand,
	gote_piece_stand: PieceStand,
	piece_scene: PackedScene
) -> void:
	_game_state = game_state
	_board = board
	_sente_piece_stand = sente_piece_stand
	_gote_piece_stand = gote_piece_stand
	_piece_scene = piece_scene


func _register(state: PieceState, piece: Piece) -> void:
	if _nodes.has(state):
		push_error("BoardView._register: 二重登録です")
	_nodes[state] = piece


func _node_for(state: PieceState) -> Piece:
	var piece: Piece = _nodes.get(state)
	if piece == null:
		push_error("BoardView._node_for: 未登録のPieceStateです")
	return piece


func _stand_for(state: PieceState) -> PieceStand:
	return _gote_piece_stand if state.is_gote else _sente_piece_stand


func _set_held(piece: Piece, is_held: bool) -> void:
	piece.is_held = is_held
	piece.z_index = 10 if is_held else 0


func _spawn_piece_for(state: PieceState) -> void:
	if _piece_scene == null:
		push_error("Piece Scene が設定されていません")
		return

	var piece: Piece = _piece_scene.instantiate()
	_board.add_child(piece)
	piece.state = state
	piece.refresh_display()
	piece.position = GameConfig.cell_to_position(state.current_col, state.current_row)
	_register(state, piece)
	piece_spawned.emit(piece)


func rebuild() -> void:
	_board.clear_pieces()
	_sente_piece_stand.clear_pieces()
	_gote_piece_stand.clear_pieces()
	_nodes.clear()

	if not _game_state.sente_hand.is_empty() or not _game_state.gote_hand.is_empty():
		push_error("BoardView.rebuild: 持ち駒ありの再構築は未対応です")

	for row in range(GameConfig.BOARD_ROWS):
		for col in range(GameConfig.BOARD_COLS):
			var state := _game_state.get_piece(col, row)
			if state != null:
				_spawn_piece_for(state)


func hold(state: PieceState) -> void:
	_set_held(_node_for(state), true)
	if state.is_in_hand():
		_stand_for(state).update_layout()


func release(state: PieceState, immediate: bool = false) -> void:
	var piece := _node_for(state)
	_set_held(piece, false)
	if state.is_in_hand():
		_stand_for(state).update_layout(immediate)
	else:
		piece.position = GameConfig.cell_to_position(state.current_col, state.current_row)


func position_of(state: PieceState) -> Vector2:
	return _board.to_local(_node_for(state).global_position)


func place_piece(state: PieceState) -> void:
	var piece := _node_for(state)
	_set_held(piece, false)
	piece.position = GameConfig.cell_to_position(state.current_col, state.current_row)


func capture_piece(state: PieceState) -> void:
	_stand_for(state).add_piece(_node_for(state))


func drop_piece(state: PieceState) -> void:
	var piece := _node_for(state)
	var source_stand := piece.get_parent()

	_set_held(piece, false)
	piece.reparent(_board)
	piece.visible = true

	piece.position = GameConfig.cell_to_position(state.current_col, state.current_row)

	if source_stand is PieceStand:
		source_stand.update_layout()


func return_to_stand(state: PieceState) -> void:
	_stand_for(state).add_piece(_node_for(state), true)


func revive_piece(state: PieceState) -> void:
	var piece := _node_for(state)
	var source_stand := piece.get_parent()

	piece.reparent(_board)
	piece.visible = true
	piece.refresh_display()

	piece.position = GameConfig.cell_to_position(state.current_col, state.current_row)

	if source_stand is PieceStand:
		source_stand.update_layout(true)


func refresh_display(state: PieceState) -> void:
	var piece := _node_for(state)
	piece.refresh_display()
