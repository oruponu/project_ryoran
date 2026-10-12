extends SceneTree


var _failures := 0


func _initialize() -> void:
	var scene: PackedScene = load("res://scenes/main/main.tscn")
	var main: Node2D = scene.instantiate()
	root.add_child(main)
	await process_frame

	_check_last_entry(main)
	await _check_scroll(main)

	main.queue_free()
	await process_frame

	print("== history_selftest: %s ==" % ("FAIL (%d)" % _failures if _failures > 0 else "ALL PASS"))
	quit(1 if _failures > 0 else 0)


func _expect(label: String, condition: bool) -> void:
	if condition:
		print("PASS: ", label)
	else:
		_failures += 1
		print("FAIL: ", label)


func _make_record(main: Node2D) -> MoveRecord:
	var state: PieceState = main.game_state.get_piece(2, 6)
	return MoveRecord.new(state, 2, 6, 2, 5)


func _check_last_entry(main: Node2D) -> void:
	var panel: MoveHistoryPanel = main.move_history_panel
	var label: Label = main.last_move_label

	_expect("最終行: 開始局面の行を返す", panel.last_entry_text() == "開始局面")
	_expect("ラベル: 開始局面を表示する", label.text == "開始局面")

	panel.add_move(1, _make_record(main), null)
	_expect("最終行: 指し手の行は手数を付けて返す", panel.last_entry_text() == "1 ▲７六歩")
	_expect("ラベル: 指し手を表示する", label.text == "1 ▲７六歩")

	panel.add_ending(2, GameEnding.Type.RESIGNATION, true)
	_expect("最終行: 終局の行は手数を付けずに返す", panel.last_entry_text() == "△投了")

	panel.remove_last_move()
	_expect("ラベル: 最終行を削除すると前の行に戻る", label.text == "1 ▲７六歩")

	panel.clear()
	_expect("最終行: clear の直後は空", panel.last_entry_text() == "")
	_expect("ラベル: clear の直後は空", label.text == "")

	panel.add_game_start(0)


func _check_scroll(main: Node2D) -> void:
	var panel: MoveHistoryPanel = main.move_history_panel
	var bar: VScrollBar = panel.history_scroll.get_v_scroll_bar()
	var record := _make_record(main)

	panel.visible = false
	for i in range(60):
		panel.add_move(i + 1, record, null)
	panel.visible = true
	panel.scroll_to_latest()
	await process_frame
	await process_frame
	_expect("スクロール: 隠している間に行が増えても表示すると最下部までスクロールする", bar.max_value > bar.page and bar.value >= bar.max_value - bar.page - 1.0)
