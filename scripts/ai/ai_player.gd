class_name AIPlayer

extends RefCounted


const PIECE_VALUES = {
	Piece.Type.PAWN: 1,
	Piece.Type.LANCE: 3,
	Piece.Type.KNIGHT: 4,
	Piece.Type.SILVER: 5,
	Piece.Type.GOLD: 6,
	Piece.Type.BISHOP: 8,
	Piece.Type.ROOK: 10,
	Piece.Type.KING: 99
}

const PROMOTION_BONUS_VALUES = {
	Piece.Type.PAWN: 5,
	Piece.Type.LANCE: 3,
	Piece.Type.KNIGHT: 2,
	Piece.Type.SILVER: 1,
	Piece.Type.GOLD: 0,
	Piece.Type.BISHOP: 3,
	Piece.Type.ROOK: 3,
	Piece.Type.KING: 0
}


var is_enemy_side: bool = true


func get_next_move(main_node: Node2D) -> Move:
	var legal_moves = _get_all_legal_moves(main_node)
	
	if legal_moves.is_empty():
		return null
		
	var best_score = 0
	var best_moves: Array[Move] = []
	
	for move in legal_moves:
		if _can_promote(move):
			move.is_promotion = true
		
		var score = _evaluate_move_score(move, main_node)
		
		if score > best_score:
			best_score = score
			best_moves = [move]
		elif score == best_score:
			best_moves.append(move)
	
	return best_moves.pick_random()


func _evaluate_move_score(move: Move, main_node: Node2D) -> int:
	var score = 0
	
	var target_piece = main_node.get_piece(move.to_col, move.to_row)
	if target_piece != null:
		score += PIECE_VALUES.get(target_piece.piece_type, 0)
	
	if move.is_promotion:
		score += PROMOTION_BONUS_VALUES.get(move.piece.piece_type, 0)
	
	return score


func _get_all_legal_moves(main_node: Node2D) -> Array[Move]:
	var moves: Array[Move] = []
	var pieces = []
	
	for col in range(GameConfig.BOARD_COLS):
		for row in range(GameConfig.BOARD_ROWS):
			var piece = main_node.get_piece(col, row)
			if piece != null and piece.is_enemy == is_enemy_side:
				pieces.append(piece)
	
	var stand = main_node.enemy_piece_stand if is_enemy_side else main_node.player_piece_stand
	for piece in stand.get_children():
		if piece is Piece:
			pieces.append(piece)
	
	for piece in pieces:
		var coords = []
		if piece.current_col == -1 and piece.current_row == -1:
			coords = piece.get_legal_drops()
		else:
			coords = piece.get_legal_moves()
		
		for coord in coords:
			if main_node.is_king_safe_after_move(piece, coord.x, coord.y):
				moves.append(Move.new(piece, coord.x, coord.y, false))
	
	return moves


func _can_promote(move: Move) -> bool:
	if move.is_drop: return false
	
	var piece = move.piece
	if piece.is_promoted or piece.piece_type == Piece.Type.KING or piece.piece_type == Piece.Type.GOLD:
		return false
	
	var to_row = move.to_row
	var from_row = piece.current_row
	
	if is_enemy_side:
		return to_row >= 6 or from_row >= 6
	else:
		return to_row <= 2 or from_row <= 2
