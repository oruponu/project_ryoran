extends SceneTree


const BOARD_SIZE = 675.0
const BOARD_MARGIN = 22.5


var _failures := 0


func _initialize() -> void:
	var scene: PackedScene = load("res://scenes/main/main.tscn")
	var main: Node2D = scene.instantiate()
	root.add_child(main)
	await process_frame

	var layout_script: GDScript = load("res://scripts/main/main_layout.gd")
	_check_static(layout_script)
	var snapshot := _snapshot(main)
	_check_cancel_on_change(main)
	for size in [Vector2(720, 1280), Vector2(720, 1558), Vector2(756, 1280)]:
		_check_fit(main, size)
	_check_button_heights(main, 80.0)
	_check_portrait_visibility(main)
	_check_history(main)
	_check_restore(main, snapshot)

	main.queue_free()
	await process_frame

	print("== layout_selftest: %s ==" % ("FAIL (%d)" % _failures if _failures > 0 else "ALL PASS"))
	quit(1 if _failures > 0 else 0)


func _expect(label: String, condition: bool) -> void:
	if condition:
		print("PASS: ", label)
	else:
		_failures += 1
		print("FAIL: ", label)


func _check_static(layout_script: GDScript) -> void:
	_expect("判定: 縦長の画面は縦", layout_script.is_portrait(Vector2(390, 844)))
	_expect("判定: 横長の画面は横", not layout_script.is_portrait(Vector2(1600, 900)))
	_expect("判定: 正方形の画面は横", not layout_script.is_portrait(Vector2(800, 800)))
	_expect("見える範囲: 縦長の機種では高さが伸びる", layout_script.visible_size_for(Vector2(390, 844)).is_equal_approx(Vector2(720, 844 * 720 / 390.0)))
	_expect("見える範囲: 高さが削られると幅が伸びる", layout_script.visible_size_for(Vector2(390, 660)).is_equal_approx(Vector2(390 * 1280 / 660.0, 1280)))
	_expect("見える範囲: 横は 1600×900", layout_script.visible_size_for(Vector2(1920, 1080)) == Vector2(1600, 900))


func _snapshot(main: Node2D) -> Dictionary:
	return {
		"board": main.board.position,
		"sente_stand": main.sente_piece_stand.position,
		"gote_stand": main.gote_piece_stand.position,
		"history_position": main.move_history_panel.position,
		"history_size": main.move_history_panel.size,
		"buttons_position": main.button_container.position,
		"buttons_size": main.button_container.size,
		"win_rate_bar": main.win_rate_bar.position,
		"button_height": main.new_game_button.custom_minimum_size.y,
		"dialog_button_height": main.common_dialog.yes_button.custom_minimum_size.y,
	}


func _check_cancel_on_change(main: Node2D) -> void:
	var state: PieceState = main.game_state.get_piece(2, 6)
	var piece: Piece = main.board_view._node_for(state)
	var base_scale := piece.scale
	main.input_controller.handle_tap(Vector2i(2, 6), state, true)
	var arrows: Array[Dictionary] = [{"from": Vector2(35, 35), "to": Vector2(105, 105)}]
	main.board.show_hint_arrows(arrows)

	main.layout.apply(true, Vector2(720, 1280))
	_expect("切り替え: 縦になる", main.layout.is_portrait_layout)
	_expect("切り替え: 選択を解除する", not piece.is_held and piece.scale.is_equal_approx(base_scale))
	_expect("切り替え: 選択枠を消す", not main.board._selection_rect.visible)
	_expect("切り替え: ヒントの矢印を消す", main.board._hint_arrows.is_empty())


func _rects(main: Node2D) -> Dictionary:
	return {
		"後手の駒台": Rect2(main.gote_piece_stand.position, main.gote_piece_stand.get_stand_size()),
		"盤": Rect2(main.board.position - Vector2(BOARD_MARGIN, BOARD_MARGIN), Vector2(BOARD_SIZE, BOARD_SIZE)),
		"先手の駒台": Rect2(main.sente_piece_stand.position, main.sente_piece_stand.get_stand_size()),
		"勝率バー": Rect2(main.win_rate_bar.position, main.win_rate_bar.size),
		"ボタン": Rect2(main.button_container.position, main.button_container.size),
		"直前の指し手": Rect2(main.last_move_label.position, main.last_move_label.size),
	}


func _check_fit(main: Node2D, size: Vector2) -> void:
	main.layout.apply(true, size)
	var screen := Rect2(Vector2.ZERO, size)
	var rects := _rects(main)
	var names: Array = rects.keys()
	for name in names:
		_expect("%s: %s が画面に収まる" % [size, name], screen.encloses(rects[name]))
	for i in range(names.size()):
		for j in range(i + 1, names.size()):
			_expect("%s: %s と %s が重ならない" % [size, names[i], names[j]], not rects[names[i]].intersects(rects[names[j]]))
	_expect("%s: 盤が左右中央にある" % size, is_equal_approx(rects["盤"].get_center().x, size.x / 2.0))
	_expect("%s: 後手の駒台の左端を盤にそろえる" % size, is_equal_approx(rects["後手の駒台"].position.x, rects["盤"].position.x))
	_expect("%s: 先手の駒台の右端を盤にそろえる" % size, is_equal_approx(rects["先手の駒台"].end.x, rects["盤"].end.x))
	_expect("%s: 後手の駒台は盤の上にある" % size, rects["後手の駒台"].end.y <= rects["盤"].position.y)
	_expect("%s: 先手の駒台は盤の下にある" % size, rects["先手の駒台"].position.y >= rects["盤"].end.y)
	_expect("%s: ボタンは最下部にある" % size, is_equal_approx(rects["ボタン"].end.y, size.y - 40.0))


func _check_button_heights(main: Node2D, height: float) -> void:
	for child in main.button_container.get_children():
		if child is Button:
			_expect("ボタンの最小の高さ: %s が %s" % [child.name, height], child.custom_minimum_size.y == height)
	_expect("ボタンの最小の高さ: ダイアログの「はい」が %s" % height, main.common_dialog.yes_button.custom_minimum_size.y == height)
	_expect("ボタンの最小の高さ: ダイアログの「いいえ」が %s" % height, main.common_dialog.no_button.custom_minimum_size.y == height)


func _check_portrait_visibility(main: Node2D) -> void:
	_expect("縦: 駒台を横並びにする", main.sente_piece_stand.is_horizontal and main.gote_piece_stand.is_horizontal)
	_expect("縦: 棋譜ボタンを表示する", main.history_button.visible)
	_expect("縦: 直前の指し手を表示する", main.last_move_label.visible)
	_expect("縦: 棋譜は閉じている", not main.move_history_panel.visible and not main.history_backdrop.visible)


func _check_history(main: Node2D) -> void:
	main.layout.set_history_open(true)
	_expect("棋譜: 開くとパネルと背景を表示する", main.layout.is_history_open() and main.move_history_panel.visible and main.history_backdrop.visible)
	_expect("棋譜: パネルを背景より前面に出す", main.move_history_panel.z_index > main.history_backdrop.z_index and main.history_backdrop.z_index >= 20)
	_expect("棋譜: パネルを最後の子に移して入力を最前面で受ける", main.get_child(main.get_child_count() - 1) == main.move_history_panel)

	var click := InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.pressed = true
	main._on_history_backdrop_gui_input(click)
	_expect("棋譜: 背景をクリックすると閉じる", not main.layout.is_history_open() and not main.history_backdrop.visible)

	main._on_history_button_pressed()
	_expect("棋譜: ボタンで開く", main.layout.is_history_open())
	main.layout.apply(true, Vector2(720, 1280))
	_expect("棋譜: レイアウトを適用し直すと閉じる", not main.layout.is_history_open())


func _check_restore(main: Node2D, snapshot: Dictionary) -> void:
	main.layout.apply(false, Vector2(1600, 900))
	var now := _snapshot(main)
	for key in snapshot:
		_expect("横に戻す: %s が元に戻る" % key, now[key] == snapshot[key])
	_expect("横に戻す: 駒台を縦並びにする", not main.sente_piece_stand.is_horizontal and not main.gote_piece_stand.is_horizontal)
	_expect("横に戻す: 棋譜パネルを表示する", main.move_history_panel.visible)
	_expect("横に戻す: 棋譜ボタンを隠す", not main.history_button.visible)
	_expect("横に戻す: 直前の指し手を隠す", not main.last_move_label.visible)
	main.layout.set_history_open(false)
	_expect("横に戻す: 横では棋譜を閉じる操作を無視する", main.move_history_panel.visible)
