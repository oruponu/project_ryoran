extends SceneTree


const START_SFEN := "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
const START_SFEN_W := "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"


var _failures: int = 0
var _input_controller: GDScript


func _initialize() -> void:
	var engine := ShogiEngine.new()
	_input_controller = load("res://scripts/game/input_controller.gd")

	_test_serializer()
	_test_piece_type_values()
	_test_initial_position(engine)
	_test_hash_turn_bit(engine)
	_test_legal_move_keys(engine)
	_test_pinned_gold(engine)
	_test_destinations_dedup(engine)
	_test_pawn_drops(engine)
	_test_knight_drops(engine)
	_test_uchifuzume(engine)
	_test_double_check(engine)
	_test_mate(engine)
	_test_not_mate(engine)
	_test_block_drop(engine)
	_test_block_escape(engine)
	_test_stalemate(engine)
	_test_multidigit_hand(engine)
	_test_invalid_input(engine)
	_test_search(engine)
	_test_promotion_mode(engine)
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
	game_state.move_count = 1

	game_state.register_piece(_piece_state(PieceState.Type.KING, true, false, 4, 0))
	game_state.register_piece(_piece_state(PieceState.Type.ROOK, true, true, 3, 2))
	game_state.register_piece(_piece_state(PieceState.Type.PAWN, false, false, 4, 6))
	game_state.register_piece(_piece_state(PieceState.Type.SILVER, false, true, 0, 8))
	game_state.register_piece(_piece_state(PieceState.Type.KING, false, false, 4, 8))

	game_state.sente_hand.append(PieceState.new(PieceState.Type.PAWN, false))
	game_state.sente_hand.append(PieceState.new(PieceState.Type.PAWN, false))
	game_state.gote_hand.append(PieceState.new(PieceState.Type.GOLD, true))

	var serializer := SfenSerializer.new(game_state)
	var expected := "4k4/9/3+r5/9/9/9/4P4/9/+S3K4 w 2Pg 2"
	_check("シリアライザー: 盤・成駒・持ち駒・後手番・手数", serializer.to_sfen() == expected)


func _test_piece_type_values() -> void:
	var pairs := {
		PieceState.Type.KING: ShogiEngine.PIECE_KING,
		PieceState.Type.ROOK: ShogiEngine.PIECE_ROOK,
		PieceState.Type.BISHOP: ShogiEngine.PIECE_BISHOP,
		PieceState.Type.GOLD: ShogiEngine.PIECE_GOLD,
		PieceState.Type.SILVER: ShogiEngine.PIECE_SILVER,
		PieceState.Type.KNIGHT: ShogiEngine.PIECE_KNIGHT,
		PieceState.Type.LANCE: ShogiEngine.PIECE_LANCE,
		PieceState.Type.PAWN: ShogiEngine.PIECE_PAWN,
	}
	for piece_type in pairs:
		_check("駒種の値: %s" % PieceState.Type.keys()[piece_type], piece_type == pairs[piece_type])


func _piece_state(piece_type: PieceState.Type, is_gote: bool, is_promoted: bool, col: int, row: int) -> PieceState:
	var state := PieceState.new(piece_type, is_gote, col, row)
	state.is_promoted = is_promoted
	return state


func _destinations(engine: ShogiEngine, sfen: String, col: int, row: int) -> Array[Vector2i]:
	var state := PieceState.new(PieceState.Type.PAWN, false, col, row)
	return _input_controller.destinations_of(_input_controller.moves_for(engine.get_legal_moves(sfen), state))


func _drop_targets(engine: ShogiEngine, sfen: String, piece_type: PieceState.Type) -> Array[Vector2i]:
	var state := PieceState.new(piece_type, false)
	return _input_controller.destinations_of(_input_controller.moves_for(engine.get_legal_moves(sfen), state))


func _promotion_mode(engine: ShogiEngine, sfen: String, from: Vector2i, to: Vector2i) -> PromotionMode.Type:
	var state := PieceState.new(PieceState.Type.PAWN, false, from.x, from.y)
	var moves: Array[Dictionary] = _input_controller.moves_for(engine.get_legal_moves(sfen), state)
	return PromotionMode.from_moves(_input_controller.moves_to(moves, to))


func _test_initial_position(engine: ShogiEngine) -> void:
	var position: Dictionary = engine.inspect_position(START_SFEN)
	_check("開始局面: 先手王手なし", not position["in_check"])
	_check("開始局面: 後手王手なし", not engine.inspect_position(START_SFEN_W)["in_check"])
	_check("開始局面: 合法手あり", position["has_legal_move"])

	var pawn_moves := _destinations(engine, START_SFEN, 4, 6)
	_check("開始局面: 5七歩は5六のみ", pawn_moves.size() == 1 and pawn_moves[0] == Vector2i(4, 5))


func _test_hash_turn_bit(engine: ShogiEngine) -> void:
	var hash_b: int = engine.inspect_position(START_SFEN)["hash"]
	_check("ハッシュ: 手番が違えば異なる", hash_b != engine.inspect_position(START_SFEN_W)["hash"])
	_check("ハッシュ: 同一 SFEN で一致", hash_b == engine.inspect_position(START_SFEN)["hash"])


func _test_legal_move_keys(engine: ShogiEngine) -> void:
	var moves: Array = engine.get_legal_moves(START_SFEN)
	_check("合法手の一覧: 開始局面は30手", moves.size() == 30)
	var keys := ["from_col", "from_row", "to_col", "to_row", "piece_type", "is_drop", "is_promotion"]
	_check("合法手の一覧: キーは7つ", moves.size() > 0 and moves[0].keys().size() == 7 and moves[0].has_all(keys))

	var drops: Array = engine.get_legal_moves("8k/9/9/9/9/9/9/9/4K4 b G 1").filter(func(move): return move.is_drop)
	_check("合法手の一覧: 打つ手の移動元は -1", drops.size() > 0 and drops.all(func(move): return move.from_col == -1 and move.from_row == -1))


func _test_pinned_gold(engine: ShogiEngine) -> void:
	var sfen := "4r3k/9/9/9/4G4/9/9/9/4K4 b - 1"
	var moves := _destinations(engine, sfen, 4, 4)
	var ok := moves.size() == 2 and Vector2i(4, 3) in moves and Vector2i(4, 5) in moves
	_check("王手放置になる手の除外: 飛車と玉の間にいる金は縦にしか動けない", ok)


func _test_destinations_dedup(engine: ShogiEngine) -> void:
	var sfen := "4k4/9/9/4P4/9/9/9/9/4K4 b - 1"
	var state := PieceState.new(PieceState.Type.PAWN, false, 4, 3)
	var moves: Array[Dictionary] = _input_controller.moves_for(engine.get_legal_moves(sfen), state)
	_check("駒の合法手: 4段目の歩は成と不成の2手", moves.size() == 2)
	var destinations: Array[Vector2i] = _input_controller.destinations_of(moves)
	_check("ガイド: 成と不成の両方がある移動先も1マスだけ表示する", destinations.size() == 1 and destinations[0] == Vector2i(4, 2))


func _test_pawn_drops(engine: ShogiEngine) -> void:
	var sfen := "8k/9/9/9/9/9/4P4/9/4K4 b P 1"
	var drops := _drop_targets(engine, sfen, PieceState.Type.PAWN)
	var ok := drops.size() == 64
	for drop in drops:
		if drop.y == 0 or drop.x == 4:
			ok = false
	_check("歩打ち: 二歩になる筋と1段目を除いて64マス", ok)


func _test_knight_drops(engine: ShogiEngine) -> void:
	var sfen := "8k/9/9/9/9/9/4P4/9/4K4 b NP 1"
	var drops := _drop_targets(engine, sfen, PieceState.Type.KNIGHT)
	var ok := drops.size() == 61
	for drop in drops:
		if drop.y <= 1:
			ok = false
	_check("桂打ち: 1・2段目を除いて61マス", ok)


func _test_uchifuzume(engine: ShogiEngine) -> void:
	var sfen := "7lk/7p1/8S/9/9/9/9/9/4K4 b P 1"
	var drops := _drop_targets(engine, sfen, PieceState.Type.PAWN)
	_check("打ち歩詰め: 1二への歩打ちが除外される", not Vector2i(8, 1) in drops)
	_check("打ち歩詰め: ほかのマスには歩を打てる", drops.size() > 0)


func _test_double_check(engine: ShogiEngine) -> void:
	var sfen := "4r3k/9/9/9/b8/9/9/9/4K4 b R 1"
	var position: Dictionary = engine.inspect_position(sfen)
	_check("両王手: 王手検出", position["in_check"])
	_check("両王手: 合駒はどこにも打てない", _drop_targets(engine, sfen, PieceState.Type.ROOK).is_empty())
	_check("両王手: 玉の逃げ道はある", position["has_legal_move"])


func _test_mate(engine: ShogiEngine) -> void:
	var position: Dictionary = engine.inspect_position("4k4/4G4/4G4/9/9/9/9/9/4K4 w - 1")
	_check("頭金: 王手", position["in_check"])
	_check("頭金: 合法手なし (詰み)", not position["has_legal_move"])


func _test_not_mate(engine: ShogiEngine) -> void:
	var position: Dictionary = engine.inspect_position("4k4/4G4/9/9/9/9/9/9/4K4 w - 1")
	_check("不詰: 支えのない金は玉で取れる", position["has_legal_move"])


func _test_block_drop(engine: ShogiEngine) -> void:
	var sfen := "4k4/9/9/9/9/9/9/9/4R3K w p 1"
	var drops := _drop_targets(engine, sfen, PieceState.Type.PAWN)
	var ok := drops.size() == 7
	for drop in drops:
		if drop.x != 4:
			ok = false
	_check("合駒: 飛車筋を塞ぐ歩打ちのみ合法 (7マス)", ok)


func _test_block_escape(engine: ShogiEngine) -> void:
	var position: Dictionary = engine.inspect_position("8k/6G2/9/9/9/9/9/9/4K3R w p 1")
	_check("合駒で受ける: 王手検出", position["in_check"])
	_check("合駒で受ける: 歩を打てば王手を逃れられる", position["has_legal_move"])
	_check("合駒で受ける: 持ち駒がなければ詰み", not engine.inspect_position("8k/6G2/9/9/9/9/9/9/4K3R w - 1")["has_legal_move"])


func _test_stalemate(engine: ShogiEngine) -> void:
	var position: Dictionary = engine.inspect_position("8k/6G2/7G1/9/9/9/9/9/4K4 w - 1")
	_check("ステイルメイト: 王手なし", not position["in_check"])
	_check("ステイルメイト: 合法手なし", not position["has_legal_move"])


func _test_multidigit_hand(engine: ShogiEngine) -> void:
	var drops := _drop_targets(engine, "4k4/9/9/9/9/9/9/9/4K4 b 18P 1", PieceState.Type.PAWN)
	_check("二桁の持ち駒: 歩18枚なら打てるのは71マス", drops.size() == 71)


func _test_invalid_input(engine: ShogiEngine) -> void:
	_check("不正 SFEN: 駒の文字が誤りなら空盤面 (合法手なし)", not engine.inspect_position("xyz b - 1")["has_legal_move"])
	_check("不正 SFEN: 段数不足で空盤面 (王手なし)", not engine.inspect_position("lnsgkgsnl/9 b - 1")["in_check"])
	_check("不正 SFEN: 持ち駒の0枚指定で空盤面", not engine.inspect_position("4k4/9/9/9/9/9/9/9/4K4 b 0P 1")["has_legal_move"])
	_check("不正 SFEN: 手数欠落で空盤面", not engine.inspect_position("4k4/9/9/9/9/9/9/9/4K4 b -")["has_legal_move"])
	_check("不正 SFEN: 合法手の一覧は空", engine.get_legal_moves("xyz b - 1").is_empty())


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
	_check("探索: 打つ手の移動元は -1", move.get("from_col") == -1 and move.get("from_row") == -1)

	engine.update_state_from_sfen("4k4/9/9/9/9/9/4g4/9/4K4 w g 1")
	var gote_moves: Array = engine.search_top_moves(1)
	var found_gote_mate: bool = (
		gote_moves.size() == 1
		and gote_moves[0].is_drop
		and gote_moves[0].piece_type == PieceState.Type.GOLD
		and gote_moves[0].to_col == 4
		and gote_moves[0].to_row == 7
	)
	_check("探索: 後手番の1手詰めの5八金打を見つける", found_gote_mate)
	_check("探索: 勝率は手番側から見た値 (後手番で詰ませる手は 1.0)", found_gote_mate and gote_moves[0].win_rate == 1.0)

	engine.update_state_from_sfen("4k4/4G4/4G4/9/9/9/9/9/4K4 w - 1")
	_check("探索: 詰まされた側は投了 (空 Dictionary)", engine.search_best_move().is_empty())

	engine.update_state_from_sfen("xyz b - 1")
	_check("探索: 不正 SFEN は空盤面として扱い投了する", engine.search_best_move().is_empty())


func _test_promotion_mode(engine: ShogiEngine) -> void:
	_check("成りの扱い: 敵陣に入る歩は確認する",
			_promotion_mode(engine, "4k4/9/9/4P4/9/9/9/9/4K4 b - 1", Vector2i(4, 3), Vector2i(4, 2)) == PromotionMode.Type.ASK_USER)
	_check("成りの扱い: 敵陣から出る銀も確認する",
			_promotion_mode(engine, "4k4/9/4S4/9/9/9/9/9/4K4 b - 1", Vector2i(4, 2), Vector2i(3, 3)) == PromotionMode.Type.ASK_USER)
	_check("成りの扱い: 1段目に入る歩は強制で成る",
			_promotion_mode(engine, "4k4/P8/9/9/9/9/9/9/4K4 b - 1", Vector2i(0, 1), Vector2i(0, 0)) == PromotionMode.Type.FORCE_PROMOTE)
	_check("成りの扱い: 2段目に入る桂は強制で成る",
			_promotion_mode(engine, "4k4/9/9/N8/9/9/9/9/4K4 b - 1", Vector2i(0, 3), Vector2i(1, 1)) == PromotionMode.Type.FORCE_PROMOTE)
	_check("成りの扱い: 敵陣の外での移動は成らない",
			_promotion_mode(engine, START_SFEN, Vector2i(4, 6), Vector2i(4, 5)) == PromotionMode.Type.FORCE_STAY)
	_check("成りの扱い: と金は敵陣の中でも確認しない",
			_promotion_mode(engine, "4k4/9/4+P4/9/9/9/9/9/4K4 b - 1", Vector2i(4, 2), Vector2i(4, 1)) == PromotionMode.Type.FORCE_STAY)
	_check("成りの扱い: 後手の歩が7段目に入るときも確認する",
			_promotion_mode(engine, "4k4/9/9/9/9/4p4/9/9/4K4 w - 1", Vector2i(4, 5), Vector2i(4, 6)) == PromotionMode.Type.ASK_USER)


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
