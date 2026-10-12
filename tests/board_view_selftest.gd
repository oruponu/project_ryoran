extends SceneTree


var _failures := 0


func _initialize() -> void:
	var scene: PackedScene = load("res://scenes/main/main.tscn")
	var main: Node2D = scene.instantiate()
	root.add_child(main)
	await process_frame

	_check_input_controller(main)
	_check_tap_routing(main)
	_check_board_hold_release(main)
	_check_place_ends_holding(main)
	_check_hand_hold_release(main)
	_check_hold_in_place(main)
	_check_stand_routing(main)
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
	input.handle_tap(Vector2i(2, 2), gote_state)
	_expect("入力: 手番でない駒は持ち上げない", not board_view._node_for(gote_state).is_held)
	_expect("入力: 手番でない駒をクリックしてもガイドを出さない", board._active_guides.is_empty())

	input.handle_tap(Vector2i(4, 4), null)
	_expect("入力: 空きマスをクリックしても駒を持たない", input._holding_state == null)

	var state: PieceState = main.game_state.get_piece(2, 6)
	var piece: Piece = board_view._node_for(state)
	input.handle_tap(Vector2i(2, 6), state)
	_expect("入力: 手番の駒を持ち上げる", piece.is_held and piece.z_index == 10)
	_expect("入力: マウスで持った駒は追従する", piece.follows_mouse)
	_expect("入力: 7七歩を持ち上げるとガイドを1マス出す", board._active_guides.size() == 1)

	input.cancel_holding()
	_expect("入力: キャンセルで持ち上げを終える", not piece.is_held and piece.z_index == 0)
	_expect("入力: キャンセルでガイドを消す", board._active_guides.is_empty())
	_expect("入力: キャンセルで元のマスに戻す", piece.position == _cell_position(2, 6))

	var other_state: PieceState = main.game_state.get_piece(6, 6)
	input.handle_tap(Vector2i(2, 6), state)
	input.handle_tap(Vector2i(6, 6), other_state)
	_expect("入力: マウスでは自分の別の駒をクリックするとキャンセルする", not piece.is_held and not board_view._node_for(other_state).is_held)
	_expect("入力: マウスのキャンセルでガイドを消す", board._active_guides.is_empty())

	input.handle_tap(Vector2i(2, 6), state)
	input.handle_tap(Vector2i(-1, -1), null)
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
