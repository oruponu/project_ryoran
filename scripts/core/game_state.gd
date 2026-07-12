class_name GameState

extends RefCounted


var board_grid: Array = []
var current_turn: int = 0
var move_history: Array[MoveRecord] = []
var repetition_tracker := RepetitionTracker.new()
var player_hand: Array[PieceState] = []
var enemy_hand: Array[PieceState] = []


func reset() -> void:
	board_grid.clear()
	player_hand.clear()
	enemy_hand.clear()
	current_turn = 0
	move_history.clear()

	for x in range(GameConfig.BOARD_COLS):
		var column: Array = []
		for y in range(GameConfig.BOARD_ROWS):
			column.append(null)
		board_grid.append(column)


func is_gote_turn() -> bool:
	return current_turn % 2 != 0


func get_piece(col: int, row: int) -> PieceState:
	return board_grid[col][row]


func register_piece(state: PieceState) -> void:
	if board_grid[state.current_col][state.current_row] != null:
		push_error("register_piece: 配置先が占有されています (%d, %d)" % [state.current_col, state.current_row])
	board_grid[state.current_col][state.current_row] = state


func capture(target: PieceState) -> void:
	if get_piece(target.current_col, target.current_row) != target:
		push_error("capture: 盤上の対象が一致しません (%d, %d)" % [target.current_col, target.current_row])
	board_grid[target.current_col][target.current_row] = null
	target.is_enemy = not target.is_enemy
	target.is_promoted = false
	target.current_col = -1
	target.current_row = -1
	_hand_of(target.is_enemy).push_front(target)


func uncapture(state: PieceState, col: int, row: int, was_promoted: bool) -> void:
	var hand := _hand_of(state.is_enemy)
	if not hand.has(state):
		push_error("uncapture: 持ち駒に対象がありません")
	hand.erase(state)
	state.is_enemy = not state.is_enemy
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
	var hand := _hand_of(state.is_enemy)
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
	_hand_of(state.is_enemy).push_front(state)


func set_promoted(state: PieceState, promoted: bool) -> void:
	state.is_promoted = promoted


func find_hand_piece(is_enemy: bool, piece_type: PieceState.Type) -> PieceState:
	for state in _hand_of(is_enemy):
		if state.piece_type == piece_type:
			return state
	push_error("find_hand_piece: 持ち駒が見つかりません (piece_type=%d)" % piece_type)
	return null


func _hand_of(is_enemy: bool) -> Array[PieceState]:
	return enemy_hand if is_enemy else player_hand
