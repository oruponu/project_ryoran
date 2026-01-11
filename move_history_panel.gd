extends Panel


@onready var history_scroll = $HistoryScroll
@onready var history_list = $HistoryScroll/HistoryList


func add_game_start(current_turn: int) -> void:
	_append_entry_row(current_turn, "開始局面")


func add_resignation(current_turn: int) -> void:
	var is_player_turn = (current_turn - 1) % 2 == 0
	var marker = "▲" if is_player_turn else "△"
	var move_text = "%s投了" % marker
	_append_entry_row(current_turn, move_text)


func add_move(current_turn: int, record: MoveRecord, prev_record: MoveRecord) -> void:
	var move_text = _format_move_notation(current_turn, record, prev_record)
	_append_entry_row(current_turn, move_text)


func remove_last_move() -> void:
	var count = history_list.get_child_count()
	if count > 0:
		var last_row_container = history_list.get_child(count - 1)
		history_list.remove_child(last_row_container)
		last_row_container.queue_free()


func clear() -> void:
	for child in history_list.get_children():
		child.queue_free()


func _append_entry_row(current_turn: int, move_text: String) -> void:
	var turn_number_label = Label.new()
	turn_number_label.text = str(current_turn)
	turn_number_label.add_theme_font_size_override("font_size", 24)
	turn_number_label.custom_minimum_size = Vector2(50, 0)
	turn_number_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	
	var move_text_label = Label.new()
	move_text_label.text = move_text
	move_text_label.add_theme_font_size_override("font_size", 24)
	
	var row_container = HBoxContainer.new()
	row_container.add_theme_constant_override("separation", 16)
	row_container.add_child(turn_number_label)
	row_container.add_child(move_text_label)
	
	history_list.add_child(row_container)
	
	await get_tree().process_frame
	history_scroll.scroll_vertical = history_scroll.get_v_scroll_bar().max_value


func _format_move_notation(current_turn: int, record: MoveRecord, prev_record: MoveRecord) -> String:
	var is_player_turn = (current_turn - 1) % 2 == 0
	var marker = "▲" if is_player_turn else "△"
	
	var dest_col_str = GameConfig.ARABIC_NUMS[9 - record.to_col - 1]
	var dest_row_str = GameConfig.KANJI_NUMS[record.to_row]
	var coord_str = dest_col_str + dest_row_str
	
	if prev_record != null:
		if prev_record.to_col == record.to_col and prev_record.to_row == record.to_row:
			coord_str = "同　"
	
	var piece_name = _get_piece_name(record.piece, record.is_promotion)
	
	var action_str = ""
	if record.is_promotion:
		action_str = "成"
	elif record.from_col == -1 and record.from_row == -1:
		action_str = "打"
	
	return "%s%s%s%s" % [marker, coord_str, piece_name, action_str]


func _get_piece_name(piece: Piece, is_promotion: bool) -> String:
	var use_promoted_name = piece.is_promoted and not is_promotion
	match piece.piece_type:
		Piece.Type.PAWN: return "と" if use_promoted_name else "歩"
		Piece.Type.LANCE: return "成香" if use_promoted_name else "香"
		Piece.Type.KNIGHT: return "成桂" if use_promoted_name else "桂"
		Piece.Type.SILVER: return "成銀" if use_promoted_name else "銀"
		Piece.Type.GOLD: return "金"
		Piece.Type.BISHOP: return "馬" if use_promoted_name else "角"
		Piece.Type.ROOK: return "龍" if use_promoted_name else "飛"
		Piece.Type.KING: return "玉"
	return "？"
