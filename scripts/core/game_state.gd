class_name GameState

extends RefCounted


var board_grid: Array[Array] = []
var move_count: int = 0
var move_history: Array[MoveRecord] = []
var sente_hand: Array[PieceState] = []
var gote_hand: Array[PieceState] = []


func reset() -> void:
	board_grid.clear()
	sente_hand.clear()
	gote_hand.clear()
	move_count = 0
	move_history.clear()

	for col in range(GameConfig.BOARD_COLS):
		var column: Array = []
		for row in range(GameConfig.BOARD_ROWS):
			column.append(null)
		board_grid.append(column)


func setup_starting_position() -> void:
	for col in range(GameConfig.BOARD_COLS):
		var mirrored_col := GameConfig.BOARD_COLS - 1 - col
		register_piece(PieceState.new(PieceState.Type.PAWN, false, col, 6))
		register_piece(PieceState.new(PieceState.Type.PAWN, true, mirrored_col, 2))

	register_piece(PieceState.new(PieceState.Type.BISHOP, false, 1, 7))
	register_piece(PieceState.new(PieceState.Type.ROOK, false, 7, 7))
	register_piece(PieceState.new(PieceState.Type.BISHOP, true, 7, 1))
	register_piece(PieceState.new(PieceState.Type.ROOK, true, 1, 1))

	var bottom_row_types: Array[PieceState.Type] = [
		PieceState.Type.LANCE, PieceState.Type.KNIGHT, PieceState.Type.SILVER, PieceState.Type.GOLD,
		PieceState.Type.KING,
		PieceState.Type.GOLD, PieceState.Type.SILVER, PieceState.Type.KNIGHT, PieceState.Type.LANCE
	]
	for col in range(GameConfig.BOARD_COLS):
		var mirrored_col := GameConfig.BOARD_COLS - 1 - col
		var piece_type := bottom_row_types[col]
		register_piece(PieceState.new(piece_type, false, col, 8))
		register_piece(PieceState.new(piece_type, true, mirrored_col, 0))


func is_gote_turn() -> bool:
	return move_count % 2 != 0


func push_move(record: MoveRecord) -> void:
	move_history.append(record)
	move_count += 1


func pop_move() -> MoveRecord:
	if move_history.is_empty():
		push_error("pop_move: 棋譜が空です")
		return null
	move_count -= 1
	return move_history.pop_back()


func get_piece(col: int, row: int) -> PieceState:
	return board_grid[col][row]


func register_piece(state: PieceState) -> void:
	if board_grid[state.current_col][state.current_row] != null:
		push_error("register_piece: 配置先が占有されています (%d, %d)" % [state.current_col, state.current_row])
	board_grid[state.current_col][state.current_row] = state


func capture(state: PieceState) -> void:
	if get_piece(state.current_col, state.current_row) != state:
		push_error("capture: 盤上の対象が一致しません (%d, %d)" % [state.current_col, state.current_row])
	board_grid[state.current_col][state.current_row] = null
	state.is_gote = not state.is_gote
	state.is_promoted = false
	state.current_col = -1
	state.current_row = -1
	_hand_of(state.is_gote).push_front(state)


func uncapture(state: PieceState, col: int, row: int, was_promoted: bool) -> void:
	var hand := _hand_of(state.is_gote)
	if not hand.has(state):
		push_error("uncapture: 持ち駒に対象がありません")
	hand.erase(state)
	state.is_gote = not state.is_gote
	state.is_promoted = was_promoted
	state.current_col = col
	state.current_row = row
	if board_grid[col][row] != null:
		push_error("uncapture: 配置先が占有されています (%d, %d)" % [col, row])
	board_grid[col][row] = state


func move_piece(state: PieceState, col: int, row: int) -> void:
	if not state.is_in_hand():
		if get_piece(state.current_col, state.current_row) != state:
			push_error("move_piece: 盤上の対象が一致しません (%d, %d)" % [state.current_col, state.current_row])
		board_grid[state.current_col][state.current_row] = null
	if board_grid[col][row] != null:
		push_error("move_piece: 配置先が占有されています (%d, %d)" % [col, row])
	board_grid[col][row] = state
	state.current_col = col
	state.current_row = row


func drop_piece(state: PieceState, col: int, row: int) -> void:
	var hand := _hand_of(state.is_gote)
	if not hand.has(state):
		push_error("drop_piece: 持ち駒に対象がありません")
	hand.erase(state)
	if board_grid[col][row] != null:
		push_error("drop_piece: 配置先が占有されています (%d, %d)" % [col, row])
	board_grid[col][row] = state
	state.current_col = col
	state.current_row = row


func return_to_hand(state: PieceState) -> void:
	if get_piece(state.current_col, state.current_row) != state:
		push_error("return_to_hand: 盤上の対象が一致しません (%d, %d)" % [state.current_col, state.current_row])
	board_grid[state.current_col][state.current_row] = null
	state.current_col = -1
	state.current_row = -1
	_hand_of(state.is_gote).push_front(state)


func set_promoted(state: PieceState, promoted: bool) -> void:
	state.is_promoted = promoted


func find_hand_piece(is_gote: bool, piece_type: PieceState.Type) -> PieceState:
	for state in _hand_of(is_gote):
		if state.piece_type == piece_type:
			return state
	push_error("find_hand_piece: 持ち駒が見つかりません (piece_type=%d)" % piece_type)
	return null


func _hand_of(is_gote: bool) -> Array[PieceState]:
	return gote_hand if is_gote else sente_hand
