class_name PieceMovement

extends RefCounted


const MOVES = {
	Piece.Type.KING: [Vector2(0, -1), Vector2(1, -1), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1), Vector2(-1, 1), Vector2(-1, 0), Vector2(-1, -1)],
	Piece.Type.GOLD: [Vector2(0, -1), Vector2(1, -1), Vector2(1, 0), Vector2(0, 1), Vector2(-1, 0), Vector2(-1, -1)],
	Piece.Type.SILVER: [Vector2(0, -1), Vector2(1, -1), Vector2(1, 1), Vector2(-1, 1), Vector2(-1, -1)],
	Piece.Type.KNIGHT: [Vector2(-1, -2), Vector2(1, -2)],
	Piece.Type.PAWN: [Vector2(0, -1)]
}


static func get_legal_moves(board, piece) -> Array[Vector2i]:
	var legal_moves: Array[Vector2i] = []
	var piece_col = piece.current_col
	var piece_row = piece.current_row
	
	for board_col in range(GameConfig.BOARD_COLS):
		for board_row in range(GameConfig.BOARD_ROWS):
			if is_legal_move(board, piece, piece_col, piece_row, board_col, board_row):
				legal_moves.append(Vector2i(board_col, board_row))
	
	return legal_moves


static func get_legal_drops(board, piece) -> Array[Vector2i]:
	var legal_drops: Array[Vector2i] = []
	
	for col in range(GameConfig.BOARD_COLS):
		for row in range(GameConfig.BOARD_ROWS):
			if is_legal_drop(board, piece, col, row):
				legal_drops.append(Vector2i(col, row))
	
	# TODO: 打ち歩詰めを禁止
	return legal_drops


static func is_legal_move(board, piece, current_col: int, current_row: int, target_col: int, target_row: int) -> bool:
	# 盤面の範囲外には移動不可
	if not _is_inside_board(target_col, target_row):
		return false
	
	# 現在地と同じ場所には移動不可
	if target_col == current_col and target_row == current_row:
		return false
	
	# ルールで認められていない場所には移動不可
	if not can_move_geometry(board, piece.piece_type, piece.is_enemy, piece.is_promoted, current_col, current_row, target_col, target_row):
		return false
	
	# 味方の駒がある場所には移動不可
	var target_piece = board.get_piece(target_col, target_row)
	if target_piece != null:
		if target_piece.is_enemy == piece.is_enemy:
			return false
	
	return true


static func is_legal_drop(board, piece, target_col: int, target_row: int) -> bool:
	# 盤面の範囲外には配置不可
	if not _is_inside_board(target_col, target_row):
		return false
	
	# すでに駒がある場所には配置不可
	if board.get_piece(target_col, target_row) != null:
		return false
	
	# 行き所のない場所には配置不可
	if _is_dead_end(piece.piece_type, piece.is_enemy, target_row):
		return false
	
	# 二歩になる場所には配置不可
	if _is_nifu(board, piece.piece_type, piece.is_enemy, target_col):
		return false
	
	return true


static func can_move_geometry(board, piece_type: Piece.Type, is_enemy: bool, is_promoted: bool, current_col: int, current_row, target_col: int, target_row: int) -> bool:
	var dx = target_col - current_col
	var dy = target_row - current_row
	
	if is_enemy:
		dx = -dx
		dy = -dy
	
	var relative_pos = Vector2(dx, dy)
	var effective_type = piece_type
	
	if is_promoted:
		match piece_type:
			Piece.Type.SILVER, Piece.Type.KNIGHT, Piece.Type.LANCE, Piece.Type.PAWN:
				effective_type = Piece.Type.GOLD
	
	match effective_type:
		Piece.Type.ROOK:
			if dx == 0 or dy == 0:
				if _is_path_blocked(board, current_col, current_row, target_col, target_row):
					return false
				return true
			if is_promoted and abs(dx) <= 1 and abs(dy) <= 1:
				return true
			return false
		Piece.Type.BISHOP:
			if abs(dx) == abs(dy):
				if _is_path_blocked(board, current_col, current_row, target_col, target_row):
					return false
				return true
			if is_promoted and (abs(dx) + abs(dy) <= 1):
				return true
			return false
		Piece.Type.LANCE:
			if dx == 0 and dy < 0:
				if _is_path_blocked(board, current_col, current_row, target_col, target_row):
					return false
				return true
			return false
		Piece.Type.KNIGHT:
			return relative_pos in MOVES[Piece.Type.KNIGHT]
		_:
			if MOVES.has(effective_type):
				return relative_pos in MOVES[effective_type]
	return false


static func _is_inside_board(col: int, row: int) -> bool:
	return col >= 0 and col < GameConfig.BOARD_COLS and row >= 0 and row < GameConfig.BOARD_ROWS


static func _is_path_blocked(board, current_col: int, current_row: int, target_col: int, target_row: int) -> bool:
	var dx = target_col - current_col
	var dy = target_row - current_row
	var steps = max(abs(dx), abs(dy))
	
	var step_x = sign(dx)
	var step_y = sign(dy)
	
	# 現在地と目的地の間にあるマスを確認
	for i in range(1, steps):
		var check_col = current_col + (step_x * i)
		var check_row = current_row + (step_y * i)
		
		if board.get_piece(check_col, check_row) != null:
			return true
	return false


static func _is_dead_end(piece_type: Piece.Type, is_enemy: bool, target_row: int) -> bool:
	var relative_row = target_row if not is_enemy else GameConfig.BOARD_ROWS - 1 - target_row
	match piece_type:
		Piece.Type.PAWN, Piece.Type.LANCE:
			return relative_row == 0
		Piece.Type.KNIGHT:
			return relative_row <= 1
	
	return false


static func _is_nifu(board, piece_type: Piece.Type, is_enemy: bool, target_col: int) -> bool:
	if piece_type != Piece.Type.PAWN:
		return false
	
	for row in range(GameConfig.BOARD_ROWS):
		var target = board.get_piece(target_col, row)
		if target != null:
			if target.is_enemy == is_enemy and target.piece_type == Piece.Type.PAWN and not target.is_promoted:
				return true
	
	return false
