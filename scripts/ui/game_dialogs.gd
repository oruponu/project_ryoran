class_name GameDialogs

extends RefCounted


var _common_dialog: CommonDialog


func _init(common_dialog: CommonDialog) -> void:
	_common_dialog = common_dialog


func request_new_game() -> bool:
	return await _common_dialog.ask_user("対局をはじめますか？", "はい", "いいえ")


func request_resign() -> bool:
	return await _common_dialog.ask_user("投了しますか？", "投了する", "投了しない")


func request_checkmate(is_gote_mated: bool) -> bool:
	var mated_text := "後手" if is_gote_mated else "先手"
	var message := "%sの玉が詰まされました。\n投了しますか？" % mated_text
	return await _common_dialog.ask_user(message, "投了する", "待った")


func request_promotion() -> bool:
	return await _common_dialog.ask_user("成りますか？", "成る", "成らない")


func show_result(move_count: int, ending: GameEnding.Type, is_sente_win: bool) -> void:
	var winner_text := "先手" if is_sente_win else "後手"
	var message: String
	match ending:
		GameEnding.Type.RESIGNATION:
			message = "まで、%d手で%sの勝ち。" % [move_count, winner_text]
		GameEnding.Type.SENNICHITE:
			message = "まで、%d手で千日手。引き分け。" % move_count
		GameEnding.Type.PERPETUAL_CHECK:
			message = "まで、%d手で連続王手の千日手。%sの反則勝ち。" % [move_count, winner_text]
	await _common_dialog.ask_user(message, "OK", "")
