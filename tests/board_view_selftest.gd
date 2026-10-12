extends SceneTree


var _failures := 0


func _initialize() -> void:
	var scene: PackedScene = load("res://scenes/main/main.tscn")
	var main: Node2D = scene.instantiate()
	root.add_child(main)
	await process_frame

	_check_input_controller(main)
	_check_board_hold_release(main)
	_check_place_ends_holding(main)
	_check_hand_hold_release(main)
	_check_hold_in_place(main)
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
	input.handle_click(gote_state)
	_expect("入力: 手番でない駒は持ち上げない", not board_view._node_for(gote_state).is_held)
	_expect("入力: 手番でない駒をクリックしてもガイドを出さない", board._active_guides.is_empty())

	var state: PieceState = main.game_state.get_piece(2, 6)
	var piece: Piece = board_view._node_for(state)
	input.handle_click(state)
	_expect("入力: 手番の駒を持ち上げる", piece.is_held and piece.z_index == 10)
	_expect("入力: 7七歩を持ち上げるとガイドを1マス出す", board._active_guides.size() == 1)

	input.cancel_holding()
	_expect("入力: キャンセルで持ち上げを終える", not piece.is_held and piece.z_index == 0)
	_expect("入力: キャンセルでガイドを消す", board._active_guides.is_empty())
	_expect("入力: キャンセルで元のマスに戻す", piece.position == _cell_position(2, 6))


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
