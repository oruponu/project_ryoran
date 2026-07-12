class_name SfenSerializer

extends RefCounted


const PIECE_CHARS: Array[String] = ["k", "r", "b", "g", "s", "n", "l", "p"]
const HAND_ORDER: Array[PieceState.Type] = [
	PieceState.Type.ROOK,
	PieceState.Type.BISHOP,
	PieceState.Type.GOLD,
	PieceState.Type.SILVER,
	PieceState.Type.KNIGHT,
	PieceState.Type.LANCE,
	PieceState.Type.PAWN,
]


var _game_state: GameState
var _player_piece_stand: PieceStand
var _enemy_piece_stand: PieceStand


func _init(game_state: GameState, player_piece_stand: PieceStand, enemy_piece_stand: PieceStand) -> void:
	_game_state = game_state
	_player_piece_stand = player_piece_stand
	_enemy_piece_stand = enemy_piece_stand


func to_sfen() -> String:
	var ranks: Array[String] = []
	for row in range(GameConfig.BOARD_ROWS):
		var rank := ""
		var empty_run := 0
		for col in range(GameConfig.BOARD_COLS):
			var piece := _game_state.get_piece(col, row)
			if piece == null:
				empty_run += 1
				continue

			if empty_run > 0:
				rank += str(empty_run)
				empty_run = 0

			var piece_char: String = PIECE_CHARS[piece.piece_type]
			if not piece.is_enemy:
				piece_char = piece_char.to_upper()
			if piece.is_promoted:
				piece_char = "+" + piece_char
			rank += piece_char

		if empty_run > 0:
			rank += str(empty_run)
		ranks.append(rank)

	var side := "w" if _game_state.is_gote_turn() else "b"

	var hands := _hand_sfen(_player_piece_stand, false) + _hand_sfen(_enemy_piece_stand, true)
	if hands.is_empty():
		hands = "-"

	return "%s %s %s %d" % ["/".join(ranks), side, hands, _game_state.current_turn + 1]


func _hand_sfen(stand: PieceStand, is_enemy: bool) -> String:
	var counts: Dictionary = {}
	for child in stand.get_children():
		if child is Piece and not child.is_queued_for_deletion():
			var piece := child as Piece
			counts[piece.piece_type] = counts.get(piece.piece_type, 0) + 1

	var result := ""
	for type in HAND_ORDER:
		var count: int = counts.get(type, 0)
		if count == 0:
			continue

		var piece_char: String = PIECE_CHARS[type]
		if not is_enemy:
			piece_char = piece_char.to_upper()
		if count > 1:
			result += str(count)
		result += piece_char

	return result
