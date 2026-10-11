class_name MoveHistoryPanel

extends Panel


@onready var history_scroll: ScrollContainer = $HistoryScroll
@onready var history_list: VBoxContainer = $HistoryScroll/HistoryList


func add_game_start(move_count: int) -> void:
	_append_entry_row(move_count, "開始局面")


func add_ending(move_count: int, ending: GameEnding.Type, is_sente_win: bool) -> void:
	var winner_marker := "▲" if is_sente_win else "△"
	var loser_marker := "△" if is_sente_win else "▲"
	var ending_text: String
	match ending:
		GameEnding.Type.RESIGNATION:
			ending_text = "%s投了" % loser_marker
		GameEnding.Type.SENNICHITE:
			ending_text = "千日手"
		GameEnding.Type.PERPETUAL_CHECK:
			ending_text = "%s反則勝ち" % winner_marker
	_append_entry_row(move_count, ending_text)


func add_move(move_count: int, record: MoveRecord, prev_record: MoveRecord) -> void:
	var move_text := _format_move_notation(move_count, record, prev_record)
	_append_entry_row(move_count, move_text)


func remove_last_move() -> void:
	var count := history_list.get_child_count()
	if count > 0:
		var last_row_container := history_list.get_child(count - 1)
		history_list.remove_child(last_row_container)
		last_row_container.queue_free()


func clear() -> void:
	for child in history_list.get_children():
		child.queue_free()


func _append_entry_row(move_count: int, move_text: String) -> void:
	var move_count_label := Label.new()
	move_count_label.text = str(move_count)
	move_count_label.add_theme_font_size_override("font_size", 24)
	move_count_label.custom_minimum_size = Vector2(50, 0)
	move_count_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT

	var move_text_label := Label.new()
	move_text_label.text = move_text
	move_text_label.add_theme_font_size_override("font_size", 24)

	var row_container := HBoxContainer.new()
	row_container.add_theme_constant_override("separation", 16)
	row_container.add_child(move_count_label)
	row_container.add_child(move_text_label)

	history_list.add_child(row_container)

	await get_tree().process_frame
	history_scroll.scroll_vertical = history_scroll.get_v_scroll_bar().max_value


func _format_move_notation(move_count: int, record: MoveRecord, prev_record: MoveRecord) -> String:
	var is_sente_turn := (move_count - 1) % 2 == 0
	var marker := "▲" if is_sente_turn else "△"

	var to_col_text := GameConfig.ARABIC_NUMS[9 - record.to_col - 1]
	var to_row_text := GameConfig.KANJI_NUMS[record.to_row]
	var coord_text := to_col_text + to_row_text

	if prev_record != null:
		if prev_record.to_col == record.to_col and prev_record.to_row == record.to_row:
			coord_text = "同　"

	var piece_name := _get_piece_name(record.state, record.is_promotion)

	var action_text := ""
	if record.is_promotion:
		action_text = "成"
	elif record.from_col == -1 and record.from_row == -1:
		action_text = "打"

	return "%s%s%s%s" % [marker, coord_text, piece_name, action_text]


func _get_piece_name(state: PieceState, is_promotion: bool) -> String:
	var use_promoted_name := state.is_promoted and not is_promotion
	match state.piece_type:
		PieceState.Type.PAWN: return "と" if use_promoted_name else "歩"
		PieceState.Type.LANCE: return "成香" if use_promoted_name else "香"
		PieceState.Type.KNIGHT: return "成桂" if use_promoted_name else "桂"
		PieceState.Type.SILVER: return "成銀" if use_promoted_name else "銀"
		PieceState.Type.GOLD: return "金"
		PieceState.Type.BISHOP: return "馬" if use_promoted_name else "角"
		PieceState.Type.ROOK: return "龍" if use_promoted_name else "飛"
		PieceState.Type.KING: return "玉"
	return "？"
