extends SceneTree


const START_SFEN := "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"


var _failures: int = 0


func _initialize() -> void:
	var engine := ShogiEngine.new()

	_test_serializer()
	_test_initial_position(engine)
	_test_hash_turn_bit(engine)
	_test_pinned_gold(engine)
	_test_pawn_drops(engine)
	_test_knight_drops(engine)
	_test_uchifuzume(engine)
	_test_double_check(engine)
	_test_mate(engine)
	_test_not_mate(engine)
	_test_block_drop(engine)
	_test_block_escape(engine)
	_test_stalemate(engine)
	_test_dead_end(engine)
	_test_multidigit_hand(engine)
	_test_invalid_input(engine)
	_test_search(engine)
	_test_can_promote(engine)
	_test_search_gote_promotion(engine)

	if _failures == 0:
		print("SFEN selftest: all passed")
	else:
		printerr("SFEN selftest: %d failed" % _failures)

	quit(1 if _failures > 0 else 0)


func _check(test_name: String, condition: bool) -> void:
	if condition:
		print("ok: " + test_name)
	else:
		_failures += 1
		printerr("FAILED: " + test_name)


func _test_serializer() -> void:
	var game_state := GameState.new()
	game_state.reset()
	game_state.current_turn = 1

	game_state.register_piece(_piece_state(PieceState.Type.KING, true, false, 4, 0))
	game_state.register_piece(_piece_state(PieceState.Type.ROOK, true, true, 3, 2))
	game_state.register_piece(_piece_state(PieceState.Type.PAWN, false, false, 4, 6))
	game_state.register_piece(_piece_state(PieceState.Type.SILVER, false, true, 0, 8))
	game_state.register_piece(_piece_state(PieceState.Type.KING, false, false, 4, 8))

	game_state.player_hand.append(PieceState.new(PieceState.Type.PAWN, false))
	game_state.player_hand.append(PieceState.new(PieceState.Type.PAWN, false))
	game_state.enemy_hand.append(PieceState.new(PieceState.Type.GOLD, true))

	var serializer := SfenSerializer.new(game_state)
	var expected := "4k4/9/3+r5/9/9/9/4P4/9/+S3K4 w 2Pg 2"
	_check("シリアライザー: 盤・成駒・持ち駒・後手番・手数", serializer.to_sfen() == expected)


func _piece_state(type: PieceState.Type, is_enemy: bool, is_promoted: bool, col: int, row: int) -> PieceState:
	var state := PieceState.new(type, is_enemy, col, row)
	state.is_promoted = is_promoted
	return state


func _test_initial_position(engine: ShogiEngine) -> void:
	_check("開始局面: 先手王手なし", not engine.is_king_in_check(START_SFEN, false))
	_check("開始局面: 後手王手なし", not engine.is_king_in_check(START_SFEN, true))
	_check("開始局面: 合法手あり", engine.has_any_legal_move(START_SFEN))

	var pawn_moves: Array = engine.get_legal_moves(START_SFEN, 4, 6)
	_check("開始局面: 5七歩は5六のみ", pawn_moves.size() == 1 and pawn_moves[0] == Vector2i(4, 5))


func _test_hash_turn_bit(engine: ShogiEngine) -> void:
	var start_sfen_w := "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"
	var hash_b: int = engine.get_position_hash(START_SFEN)
	_check("ハッシュ: 手番が違えば異なる", hash_b != engine.get_position_hash(start_sfen_w))
	_check("ハッシュ: 同一 SFEN で一致", hash_b == engine.get_position_hash(START_SFEN))


func _test_pinned_gold(engine: ShogiEngine) -> void:
	var sfen := "4r3k/9/9/9/4G4/9/9/9/4K4 b - 1"
	var moves: Array = engine.get_legal_moves(sfen, 4, 4)
	var ok := moves.size() == 2 and Vector2i(4, 3) in moves and Vector2i(4, 5) in moves
	_check("ピン: 串刺しにされた金は縦にしか動けない", ok)


func _test_pawn_drops(engine: ShogiEngine) -> void:
	var sfen := "8k/9/9/9/9/9/4P4/9/4K4 b P 1"
	var drops: Array = engine.get_legal_drops(sfen, PieceState.Type.PAWN)
	var ok := drops.size() == 64
	for drop in drops:
		if drop.y == 0 or drop.x == 4:
			ok = false
	_check("歩打ち: 二歩になる筋と1段目を除いて64マス", ok)


func _test_knight_drops(engine: ShogiEngine) -> void:
	var sfen := "8k/9/9/9/9/9/4P4/9/4K4 b NP 1"
	var drops: Array = engine.get_legal_drops(sfen, PieceState.Type.KNIGHT)
	var ok := drops.size() == 61
	for drop in drops:
		if drop.y <= 1:
			ok = false
	_check("桂打ち: 1・2段目を除外して61マス", ok)


func _test_uchifuzume(engine: ShogiEngine) -> void:
	var sfen := "7lk/7p1/8S/9/9/9/9/9/4K4 b P 1"
	var drops: Array = engine.get_legal_drops(sfen, PieceState.Type.PAWN)
	_check("打ち歩詰め: 1二への歩打ちが除外される", not Vector2i(8, 1) in drops)
	_check("打ち歩詰め: ほかのマスには歩を打てる", drops.size() > 0)


func _test_double_check(engine: ShogiEngine) -> void:
	var sfen := "4r3k/9/9/9/b8/9/9/9/4K4 b R 1"
	_check("両王手: 王手検出", engine.is_king_in_check(sfen, false))
	_check("両王手: 合駒はどこにも打てない", engine.get_legal_drops(sfen, PieceState.Type.ROOK).is_empty())
	_check("両王手: 玉の逃げ道はある", engine.has_any_legal_move(sfen))


func _test_mate(engine: ShogiEngine) -> void:
	var sfen := "4k4/4G4/4G4/9/9/9/9/9/4K4 w - 1"
	_check("頭金: 王手", engine.is_king_in_check(sfen, true))
	_check("頭金: 合法手なし (詰み)", not engine.has_any_legal_move(sfen))


func _test_not_mate(engine: ShogiEngine) -> void:
	var sfen := "4k4/4G4/9/9/9/9/9/9/4K4 w - 1"
	_check("不詰: 支えのない金は玉で取れる", engine.has_any_legal_move(sfen))


func _test_block_drop(engine: ShogiEngine) -> void:
	var sfen := "4k4/9/9/9/9/9/9/9/4R3K w p 1"
	var drops: Array = engine.get_legal_drops(sfen, PieceState.Type.PAWN)
	var ok := drops.size() == 7
	for drop in drops:
		if drop.x != 4:
			ok = false
	_check("合駒: 飛車筋を塞ぐ歩打ちのみ合法 (7マス)", ok)


func _test_block_escape(engine: ShogiEngine) -> void:
	var sfen := "8k/6G2/9/9/9/9/9/9/4K3R w p 1"
	_check("合駒で受ける: 王手検出", engine.is_king_in_check(sfen, true))
	_check("合駒で受ける: 歩を打てば王手を逃れられる", engine.has_any_legal_move(sfen))
	_check("合駒で受ける: 持ち駒がなければ詰み", not engine.has_any_legal_move("8k/6G2/9/9/9/9/9/9/4K3R w - 1"))


func _test_stalemate(engine: ShogiEngine) -> void:
	var sfen := "8k/6G2/7G1/9/9/9/9/9/4K4 w - 1"
	_check("ステイルメイト: 王手なし", not engine.is_king_in_check(sfen, true))
	_check("ステイルメイト: 合法手なし", not engine.has_any_legal_move(sfen))


func _test_dead_end(engine: ShogiEngine) -> void:
	_check("行き所なし: 先手歩の1段目", engine.is_dead_end(PieceState.Type.PAWN, false, 0))
	_check("行き所なし: 先手歩の2段目は可", not engine.is_dead_end(PieceState.Type.PAWN, false, 1))
	_check("行き所なし: 先手桂の2段目", engine.is_dead_end(PieceState.Type.KNIGHT, false, 1))
	_check("行き所なし: 後手歩の9段目", engine.is_dead_end(PieceState.Type.PAWN, true, 8))


func _test_multidigit_hand(engine: ShogiEngine) -> void:
	var drops: Array = engine.get_legal_drops("4k4/9/9/9/9/9/9/9/4K4 b 18P 1", PieceState.Type.PAWN)
	_check("二桁の持ち駒: 歩18枚なら打てるのは71マス", drops.size() == 71)


func _test_invalid_input(engine: ShogiEngine) -> void:
	_check("不正 SFEN: 駒の文字が誤りなら空盤面 (合法手なし)", not engine.has_any_legal_move("xyz b - 1"))
	_check("不正 SFEN: 段数不足で空盤面 (王手なし)", not engine.is_king_in_check("lnsgkgsnl/9 b - 1", false))
	_check("不正 SFEN: 持ち駒の0枚指定で空盤面", not engine.has_any_legal_move("4k4/9/9/9/9/9/9/9/4K4 b 0P 1"))
	_check("不正 SFEN: 手数欠落で空盤面", not engine.has_any_legal_move("4k4/9/9/9/9/9/9/9/4K4 b -"))
	_check("境界検証: 盤外座標で空配列", engine.get_legal_moves(START_SFEN, -1, 4).is_empty())
	_check("境界検証: 範囲外 piece_type で空配列", engine.get_legal_drops(START_SFEN, 99).is_empty())


func _test_search(engine: ShogiEngine) -> void:
	engine.update_state_from_sfen("4k4/9/4G4/9/9/9/9/9/4K4 b G 1")
	var move: Dictionary = engine.search_best_move()
	var found_mate: bool = (
		not move.is_empty()
		and move.is_drop
		and move.piece_type == PieceState.Type.GOLD
		and move.to_col == 4
		and move.to_row == 1
	)
	_check("探索: 1手詰めの5二金打を見つける", found_mate)

	engine.update_state_from_sfen("4k4/4G4/4G4/9/9/9/9/9/4K4 w - 1")
	_check("探索: 詰まされた側は投了 (空 Dictionary)", engine.search_best_move().is_empty())

	engine.update_state_from_sfen("xyz b - 1")
	_check("探索: 不正 SFEN は空盤面として扱い投了する", engine.search_best_move().is_empty())


func _test_can_promote(engine: ShogiEngine) -> void:
	_check("成りゾーン: 先手 to がゾーン内 (4段目→3段目)", engine.can_promote(PieceState.Type.PAWN, false, false, 3, 2))
	_check("成りゾーン: 先手 from がゾーン内 (3段目→4段目)", engine.can_promote(PieceState.Type.PAWN, false, false, 2, 3))
	_check("成りゾーン: 先手 両方ゾーン外 (5段目→4段目)", not engine.can_promote(PieceState.Type.PAWN, false, false, 4, 3))
	_check("成りゾーン: 後手 to がゾーン内 (6段目→7段目)", engine.can_promote(PieceState.Type.PAWN, false, true, 5, 6))
	_check("成りゾーン: 後手 from がゾーン内 (7段目→6段目)", engine.can_promote(PieceState.Type.PAWN, false, true, 6, 5))
	_check("成りゾーン: 後手 両方ゾーン外 (5段目→6段目)", not engine.can_promote(PieceState.Type.PAWN, false, true, 4, 5))
	_check("成りゾーン: 先手ゾーンは後手には無効 (4段目→3段目)", not engine.can_promote(PieceState.Type.PAWN, false, true, 3, 2))
	_check("成りゾーン: 銀もゾーン内で成れる", engine.can_promote(PieceState.Type.SILVER, false, false, 3, 2))
	_check("成りゾーン: 成駒は成れない", not engine.can_promote(PieceState.Type.PAWN, true, false, 3, 2))
	_check("成りゾーン: 玉は成れない", not engine.can_promote(PieceState.Type.KING, false, false, 3, 2))
	_check("成りゾーン: 金は成れない", not engine.can_promote(PieceState.Type.GOLD, false, false, 3, 2))
	_check("成りゾーン: 範囲外 piece_type は false", not engine.can_promote(99, false, false, 3, 2))
	_check("成りゾーン: from/to 両方範囲外は false", not engine.can_promote(PieceState.Type.PAWN, false, false, -1, 9))
	_check("成りゾーン: 片方範囲外でも他方がゾーン内なら true", engine.can_promote(PieceState.Type.PAWN, false, false, -1, 2))


func _test_search_gote_promotion(engine: ShogiEngine) -> void:
	engine.update_state_from_sfen("4k4/9/9/9/9/9/4p4/4R4/K8 w - 1")
	var move: Dictionary = engine.search_best_move()
	var found: bool = (
		not move.is_empty()
		and not move.is_drop
		and move.piece_type == PieceState.Type.PAWN
		and move.to_col == 4
		and move.to_row == 7
		and move.is_promotion
	)
	_check("探索: 後手の歩が飛車を取って成る (5八歩成)", found)
