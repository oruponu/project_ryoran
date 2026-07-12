class_name StateSfenSerializer

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


func _init(game_state: GameState) -> void:
	_game_state = game_state


func to_sfen() -> String:
	var ranks: Array[String] = []
	for row in range(GameConfig.BOARD_ROWS):
		var rank := ""
		var empty_run := 0
		for col in range(GameConfig.BOARD_COLS):
			var state := _game_state.get_piece_state(col, row)
			if state == null:
				empty_run += 1
				continue

			if empty_run > 0:
				rank += str(empty_run)
				empty_run = 0

			var piece_char: String = PIECE_CHARS[state.piece_type]
			if not state.is_enemy:
				piece_char = piece_char.to_upper()
			if state.is_promoted:
				piece_char = "+" + piece_char
			rank += piece_char

		if empty_run > 0:
			rank += str(empty_run)
		ranks.append(rank)

	var side := "w" if _game_state.is_gote_turn() else "b"

	var hands := _hand_sfen(_game_state.player_hand, false) + _hand_sfen(_game_state.enemy_hand, true)
	if hands.is_empty():
		hands = "-"

	return "%s %s %s %d" % ["/".join(ranks), side, hands, _game_state.current_turn + 1]


func _hand_sfen(hand: Array[PieceState], is_enemy: bool) -> String:
	var counts: Dictionary = {}
	for state in hand:
		counts[state.piece_type] = counts.get(state.piece_type, 0) + 1

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
