extends SceneTree


var _failures := 0


func _initialize() -> void:
	var scene: PackedScene = load("res://scenes/main/main.tscn")
	var main: Node2D = scene.instantiate()
	root.add_child(main)
	await process_frame

	_check_input_controller(main)
	_check_tap_routing(main)
	_check_touch_input(main)
	_check_board_hold_release(main)
	_check_place_ends_holding(main)
	_check_hand_hold_release(main)
	_check_hold_in_place(main)
	_check_stand_routing(main)
	_check_touch_hand(main)
	_check_horizontal_stand(main)
	await _check_flip_during_tween(main)
	await _check_rebuild(main)

	main.queue_free()
	await process_frame

	print("== board_view_selftest: %s ==" % ("FAIL (%d)" % _failures if _failures > 0 else "ALL PASS"))
	quit(1 if _failures > 0 else 0)


func _cell_position(col: int, row: int) -> Vector2:
	return root.get_node("GameConfig").cell_to_position(col, row)


func _expect(label: String, condition: bool) -> void:
	if condition:
		print("PASS: ", label)
	else:
		_failures += 1
		print("FAIL: ", label)


func _check_input_controller(main: Node2D) -> void:
	var input = main.input_controller
	var board_view = main.board_view
	var board: Board = main.board

	var gote_state: PieceState = main.game_state.get_piece(2, 2)
	input.handle_tap(Vector2i(2, 2), gote_state, false)
	_expect("入力: 手番でない駒は持ち上げない", not board_view._node_for(gote_state).is_held)
	_expect("入力: 手番でない駒をクリックしてもガイドを出さない", board._active_guides.is_empty())

	input.handle_tap(Vector2i(4, 4), null, false)
	_expect("入力: 空きマスをクリックしても駒を持たない", input._holding_state == null)

	var state: PieceState = main.game_state.get_piece(2, 6)
	var piece: Piece = board_view._node_for(state)
	input.handle_tap(Vector2i(2, 6), state, false)
	_expect("入力: 手番の駒を持ち上げる", piece.is_held and piece.z_index == 10)
	_expect("入力: マウスで持った駒は追従する", piece.follows_mouse)
	_expect("入力: マウスで持っても選択枠は出さない", board._selection_rect == null or not board._selection_rect.visible)
	_expect("入力: 7七歩を持ち上げるとガイドを1マス出す", board._active_guides.size() == 1)

	input.cancel_holding()
	_expect("入力: キャンセルで持ち上げを終える", not piece.is_held and piece.z_index == 0)
	_expect("入力: キャンセルでガイドを消す", board._active_guides.is_empty())
	_expect("入力: キャンセルで元のマスに戻す", piece.position == _cell_position(2, 6))

	var other_state: PieceState = main.game_state.get_piece(6, 6)
	input.handle_tap(Vector2i(2, 6), state, false)
	input.handle_tap(Vector2i(6, 6), other_state, false)
	_expect("入力: マウスでは自分の別の駒をクリックするとキャンセルする", not piece.is_held and not board_view._node_for(other_state).is_held)
	_expect("入力: マウスのキャンセルでガイドを消す", board._active_guides.is_empty())

	input.handle_tap(Vector2i(2, 6), state, false)
	input.handle_tap(Vector2i(-1, -1), null, false)
	_expect("入力: マウスで盤の外をクリックするとキャンセルする", not piece.is_held)
	_expect("入力: マウスのキャンセルで元のマスに戻す", piece.position == _cell_position(2, 6))


func _check_board_hold_release(main: Node2D) -> void:
	var board_view = main.board_view
	var state: PieceState = main.game_state.get_piece(0, 6)
	var piece: Piece = board_view._node_for(state)

	board_view.hold(state)
	_expect("hold: 盤上の駒を持ち上げる", piece.is_held and piece.z_index == 10)

	piece.position = Vector2(-100, -100)
	board_view.release(state)
	_expect("release: 盤上の駒の持ち上げを終える", not piece.is_held and piece.z_index == 0)
	_expect("release: 盤上の駒を元のマスに戻す", piece.position == _cell_position(0, 6))
	_expect("position_of: 盤上の駒はマスの中心を返す", board_view.position_of(state) == _cell_position(0, 6))


func _check_place_ends_holding(main: Node2D) -> void:
	var board_view = main.board_view
	var state: PieceState = main.game_state.get_piece(1, 6)
	var piece: Piece = board_view._node_for(state)

	board_view.hold(state)
	main.game_state.move_piece(state, 1, 5)
	board_view.place_piece(state)
	_expect("place_piece: 置いた駒は持ち上げを終える", not piece.is_held and piece.z_index == 0)
	_expect("place_piece: 移動先のマスに置く", piece.position == _cell_position(1, 5))


func _check_hand_hold_release(main: Node2D) -> void:
	var board_view = main.board_view
	var state: PieceState = main.game_state.get_piece(0, 2)
	main.game_state.capture(state)
	board_view.capture_piece(state)
	var piece: Piece = board_view._node_for(state)

	board_view.hold(state)
	_expect("hold: 持ち駒を持ち上げる", piece.is_held and piece.z_index == 10)

	board_view.release(state, true)
	_expect("release: 持ち駒の持ち上げを終える", not piece.is_held and piece.z_index == 0)
	_expect("release: 持ち駒は駒台に残る", piece.get_parent() == main.sente_piece_stand)
	_expect("position_of: 持ち駒は駒ノードの位置を盤の座標で返す", board_view.position_of(state) == main.board.to_local(piece.global_position))


func _check_rebuild(main: Node2D) -> void:
	var board_view = main.board_view
	var stand: PieceStand = main.sente_piece_stand

	for col in [5, 6]:
		var state: PieceState = main.game_state.get_piece(col, 2)
		main.game_state.capture(state)
		board_view.capture_piece(state)
	stand.update_layout(true)
	main.board.update_last_move_highlight(0, 0)
	_expect("作り直し前: 駒台の歩の枚数を表示している", stand._labels[PieceState.Type.PAWN].visible)

	var old_pieces: Array = board_view._nodes.values()
	main._reset_game()
	await process_frame

	_expect("作り直し: 元の駒ノードをすべて解放する", old_pieces.all(func(piece): return not is_instance_valid(piece)))
	_expect("作り直し: 40枚の駒ノードを作る", board_view._nodes.size() == 40)
	_expect("作り直し: 駒台の歩の枚数を表示しない", not stand._labels[PieceState.Type.PAWN].visible)
	_expect("作り直し: 最終手のハイライトを消す", main.board._last_move_rect == null)


func _check_hold_in_place(main: Node2D) -> void:
	var board_view = main.board_view
	var board: Board = main.board

	var state: PieceState = main.game_state.get_piece(8, 6)
	var piece: Piece = board_view._node_for(state)
	var base_scale := piece.scale

	board_view.hold(state, false)
	_expect("その場で持つ: 駒を持ち上げる", piece.is_held and piece.z_index == 10)
	_expect("その場で持つ: マウスに追従しない", not piece.follows_mouse)
	_expect("その場で持つ: 拡大する", piece.scale.is_equal_approx(base_scale * Piece.SELECTED_SCALE))
	board_view.release(state, true)
	_expect("その場で持つ: 離すと元の大きさに戻る", piece.scale.is_equal_approx(base_scale))
	_expect("その場で持つ: 離しても元のマスのまま", piece.position == _cell_position(8, 6))

	board_view.hold(state)
	_expect("追従して持つ: マウスに追従する", piece.follows_mouse)
	_expect("追従して持つ: 大きさは変えない", piece.scale.is_equal_approx(base_scale))
	board_view.release(state, true)
	_expect("追従して持つ: 離すと追従をやめる", not piece.follows_mouse)

	var hand_state: PieceState = main.game_state.find_hand_piece(false, PieceState.Type.PAWN)
	var hand_piece: Piece = board_view._node_for(hand_state)
	var hand_position := hand_piece.position
	board_view.hold(hand_state, false)
	_expect("その場で持つ: 持ち駒は駒台の同じ位置に残る", hand_piece.get_parent() == main.sente_piece_stand and hand_piece.position == hand_position and hand_piece.visible)
	board_view.release(hand_state, true)
	_expect("その場で持つ: 持ち駒を離すと元の大きさに戻る", hand_piece.scale.is_equal_approx(base_scale))

	board.show_selection(Vector2i(3, 4))
	_expect("選択枠: 表示する", board._selection_rect != null and board._selection_rect.visible)
	_expect("選択枠: マスの位置に置く", board._selection_rect.position == Vector2(3 * 70, 4 * 70))
	board.clear_selection()
	_expect("選択枠: 消す", not board._selection_rect.visible)
	board.show_selection(Vector2i(0, 0))
	board.clear_overlays()
	_expect("選択枠: clear_overlays で消す", not board._selection_rect.visible)


func _check_tap_routing(main: Node2D) -> void:
	var board: Board = main.board
	var event := InputEventMouseButton.new()
	event.button_index = MOUSE_BUTTON_LEFT
	event.pressed = true

	event.position = board.to_global(_cell_position(2, 6))
	_expect("振り分け: 盤のマスを求める", main._tap_coord(event) == Vector2i(2, 6))
	_expect("振り分け: 盤のマスの駒を求める", main._tap_state(event, Vector2i(2, 6)) == main.game_state.get_piece(2, 6))

	event.position = board.to_global(Vector2(-30, 100))
	_expect("振り分け: 盤の外は OUTSIDE", main._tap_coord(event) == Vector2i(-1, -1))

	event.position = board.to_global(_cell_position(2, 6))
	main.is_ai_thinking = true
	main._unhandled_input(event)
	_expect("振り分け: AI の思考中はクリックを無視する", main.input_controller._holding_state == null)
	main.is_ai_thinking = false

	main.is_game_active = false
	main._unhandled_input(event)
	_expect("振り分け: 終局後はクリックを無視する", main.input_controller._holding_state == null)
	main.is_game_active = true


func _check_stand_routing(main: Node2D) -> void:
	var board_view = main.board_view
	var stand: PieceStand = main.sente_piece_stand

	var second: PieceState = main.game_state.get_piece(1, 2)
	main.game_state.capture(second)
	board_view.capture_piece(second)
	stand.update_layout(true)

	var visible_pieces: Array[Piece] = []
	for child in stand.get_children():
		if child is Piece and child.visible:
			visible_pieces.append(child)
	_expect("駒台: 歩が2枚でも表示している駒は1枚", visible_pieces.size() == 1)

	var visible_piece: Piece = visible_pieces[0]
	var visible_position := visible_piece.position
	_expect("駒台: piece_at は表示中の代表の駒を返す", stand.piece_at(visible_position) == visible_piece.state)
	_expect("駒台: piece_at は駒のない場所で null", stand.piece_at(visible_position + Vector2(0, -70 * 3)) == null)

	var event := InputEventMouseButton.new()
	event.button_index = MOUSE_BUTTON_LEFT
	event.pressed = true
	event.position = stand.to_global(visible_position)
	_expect("振り分け: 駒台の駒を求める", main._tap_state(event, main._tap_coord(event)) == visible_piece.state)


func _check_touch_input(main: Node2D) -> void:
	var input = main.input_controller
	var board_view = main.board_view
	var board: Board = main.board

	var gote_state: PieceState = main.game_state.get_piece(2, 2)
	input.handle_tap(Vector2i(2, 2), gote_state, true)
	_expect("タッチ: 手番でない駒は選ばない", input._holding_state == null)

	var state: PieceState = main.game_state.get_piece(2, 6)
	var piece: Piece = board_view._node_for(state)
	var base_scale := piece.scale
	input.handle_tap(Vector2i(2, 6), state, true)
	_expect("タッチ: 駒を選ぶ", piece.is_held and not piece.follows_mouse)
	_expect("タッチ: 選んだ駒を拡大する", piece.scale.is_equal_approx(base_scale * Piece.SELECTED_SCALE))
	_expect("タッチ: 選んだ駒のマスに選択枠を出す", board._selection_rect.visible and board._selection_rect.position == Vector2(2 * 70, 6 * 70))
	_expect("タッチ: ガイドを出す", board._active_guides.size() == 1)

	var other_state: PieceState = main.game_state.get_piece(6, 6)
	var other_piece: Piece = board_view._node_for(other_state)
	input.handle_tap(Vector2i(6, 6), other_state, true)
	_expect("タッチ: 自分の別の駒をタップすると選択を切り替える", other_piece.is_held and not piece.is_held)
	_expect("タッチ: 切り替え前の駒は元の大きさに戻る", piece.scale.is_equal_approx(base_scale))
	_expect("タッチ: 選択枠も切り替えた駒のマスに移る", board._selection_rect.position == Vector2(6 * 70, 6 * 70))

	input.handle_tap(Vector2i(6, 6), other_state, true)
	_expect("タッチ: 選択中の駒をもう一度タップすると解除する", not other_piece.is_held and other_piece.scale.is_equal_approx(base_scale))
	_expect("タッチ: 解除で選択枠を消す", not board._selection_rect.visible)
	_expect("タッチ: 解除でガイドを消す", board._active_guides.is_empty())

	input.handle_tap(Vector2i(2, 6), state, true)
	input.handle_tap(Vector2i(4, 4), null, true)
	_expect("タッチ: 行き先でないマスをタップすると解除する", not piece.is_held)

	input.handle_tap(Vector2i(2, 6), state, true)
	input.handle_tap(Vector2i(-1, -1), null, true)
	_expect("タッチ: 盤の外をタップすると解除する", not piece.is_held)

	input.handle_tap(Vector2i(2, 6), state, true)
	input.cancel_holding()
	_expect("タッチ: ボタンなどからのキャンセルで解除する", not piece.is_held and piece.scale.is_equal_approx(base_scale))
	_expect("タッチ: ボタンなどからのキャンセルで選択枠を消す", not board._selection_rect.visible)

	var submitted: Array = []
	var on_submitted := func(s: PieceState, col: int, row: int, _mode: PromotionMode.Type) -> void:
		submitted.append([s, col, row])
	var main_handler := Callable(main, "_on_move_submitted")
	input.move_submitted.disconnect(main_handler)
	input.move_submitted.connect(on_submitted)

	input.handle_tap(Vector2i(2, 6), state, true)
	input.handle_tap(Vector2i(2, 5), null, true)
	_expect("タッチ: 行き先をタップすると着手する", submitted.size() == 1 and submitted[0] == [state, 2, 5])
	_expect("タッチ: 着手で選択を終える", input._holding_state == null and not board._selection_rect.visible)
	_expect("タッチ: 着手でガイドを消す", board._active_guides.is_empty())

	input.move_submitted.disconnect(on_submitted)
	input.move_submitted.connect(main_handler)
	board_view.release(state, true)


func _check_touch_hand(main: Node2D) -> void:
	var input = main.input_controller
	var stand: PieceStand = main.sente_piece_stand

	var visible_piece: Piece = null
	for child in stand.get_children():
		if child is Piece and child.visible:
			visible_piece = child
	var visible_position := visible_piece.position

	input.handle_tap(Vector2i(-1, -1), visible_piece.state, true)
	_expect("タッチ: 駒台の駒を選ぶ", visible_piece.is_held and not visible_piece.follows_mouse)
	_expect("タッチ: 選んだ持ち駒は駒台の同じ位置に表示したまま", visible_piece.visible and visible_piece.position == visible_position)
	_expect("タッチ: 持ち駒には選択枠を出さない", not main.board._selection_rect.visible)

	input.handle_tap(Vector2i(-1, -1), null, true)
	_expect("タッチ: 盤の外をタップすると持ち駒の選択を解除する", not visible_piece.is_held)
	_expect("タッチ: 解除後も代表の駒を表示している", visible_piece.visible and visible_piece.position == visible_position)


func _check_horizontal_stand(main: Node2D) -> void:
	var sente_stand: PieceStand = main.sente_piece_stand
	var gote_stand: PieceStand = main.gote_piece_stand

	_expect("駒台: 縦並びの大きさを返す", sente_stand.get_stand_size() == Vector2(90, 490))

	var gote_hand: PieceState = main.game_state.get_piece(6, 6)
	main.game_state.capture(gote_hand)
	main.board_view.capture_piece(gote_hand)

	sente_stand.set_horizontal(true)
	gote_stand.set_horizontal(true)
	_expect("駒台: 横並びの大きさを返す", sente_stand.get_stand_size() == Vector2(630, 70))

	var sente_pawn: Piece = main.board_view._node_for(main.game_state.find_hand_piece(false, PieceState.Type.PAWN))
	var sente_label: Label = sente_stand._labels[PieceState.Type.PAWN]
	_expect("横並び: 先手の歩は右端の枠", sente_pawn.position == Vector2(630 - 90 + 35, 35))
	_expect("横並び: 先手の枚数ラベルは駒の右", sente_label.visible and sente_label.position == sente_pawn.position + Vector2(35, 0))
	_expect("横並び: piece_at は先手の歩の位置で駒を返す", sente_stand.piece_at(sente_pawn.position) != null)

	var gote_pawn: Piece = main.board_view._node_for(gote_hand)
	_expect("横並び: 後手の歩は左端の枠", gote_pawn.position == Vector2(55, 35))

	sente_stand.set_horizontal(false)
	gote_stand.set_horizontal(false)
	_expect("縦並びに戻す: 先手の歩は下端", sente_pawn.position == Vector2(35, 490 - 35))
	_expect("縦並びに戻す: 後手の歩は上端", gote_pawn.position == Vector2(55, 35))


func _check_flip_during_tween(main: Node2D) -> void:
	var stand: PieceStand = main.sente_piece_stand
	var visible_piece: Piece = null
	for child in stand.get_children():
		if child is Piece and child.visible:
			visible_piece = child

	var vertical_position := visible_piece.position
	visible_piece.position = vertical_position - Vector2(0, 30)
	stand.update_layout()
	stand.set_horizontal(true)
	var horizontal_position := visible_piece.position
	await create_timer(0.3).timeout
	_expect("横並び: 並べ直しの途中で切り替えても横並びの位置にとどまる", visible_piece.position == horizontal_position and horizontal_position != vertical_position)

	stand.set_horizontal(false)
	_expect("縦並びに戻す: 並べ直しの途中で切り替えた駒も元の位置に来る", visible_piece.position == vertical_position)
