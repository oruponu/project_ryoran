extends Node2D


@export var piece_scene: PackedScene


const BOARD_COLOR = Color(0.85, 0.7, 0.4)
const LINE_COLOR = Color(0.0, 0.0, 0.0)
const TEXT_COLOR = Color(0.0, 0.0, 0.0)
const GUIDE_COLOR = Color(0.0, 0.7, 1.0, 0.4)
const LAST_MOVE_COLOR = Color(1.0, 0.4, 0.0, 0.2)
const MARGIN = 22.5


var active_guides: Array[ColorRect] = []
var last_move_rect: ColorRect = null
var last_move_tween: Tween = null


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	queue_redraw()


func _draw() -> void:
	var board_width = GameConfig.GRID_SIZE * GameConfig.BOARD_COLS
	var board_height = GameConfig.GRID_SIZE * GameConfig.BOARD_ROWS
	var bg_rect = Rect2(
		Vector2(-MARGIN, -MARGIN),
		Vector2(board_width + MARGIN * 2, board_height + MARGIN * 2)
	)
	draw_rect(bg_rect, BOARD_COLOR)
	draw_rect(bg_rect, LINE_COLOR, false, 2.0)
	
	for x in range(GameConfig.BOARD_COLS + 1):
		var start_pos = Vector2(x * GameConfig.GRID_SIZE, 0)
		var end_pos = Vector2(x * GameConfig.GRID_SIZE, GameConfig.BOARD_ROWS * GameConfig.GRID_SIZE)
		draw_line(start_pos, end_pos, LINE_COLOR, 2.0)
	
	for y in range(GameConfig.BOARD_ROWS + 1):
		var start_pos = Vector2(0, y * GameConfig.GRID_SIZE)
		var end_pos = Vector2(GameConfig.BOARD_COLS * GameConfig.GRID_SIZE, y * GameConfig.GRID_SIZE)
		draw_line(start_pos, end_pos, LINE_COLOR, 2.0)
	
	_draw_coordinates()


func _draw_coordinates() -> void:
	var font = ThemeDB.get_fallback_font()
	var font_size = 16
	var offset_y = -5
	var offset_x = 3
	
	for x in range(GameConfig.BOARD_COLS):
		var text = GameConfig.ARABIC_NUMS[9 - x - 1]
		var pos_x = x * GameConfig.GRID_SIZE
		var pos = Vector2(pos_x, offset_y)
		draw_string(font, pos, text, HORIZONTAL_ALIGNMENT_CENTER, GameConfig.GRID_SIZE, font_size, TEXT_COLOR)
	
	for y in range(GameConfig.BOARD_ROWS):
		var text = GameConfig.KANJI_NUMS[y]
		var pos_x = GameConfig.BOARD_COLS * GameConfig.GRID_SIZE + offset_x
		var cell_center_y = y * GameConfig.GRID_SIZE + (GameConfig.GRID_SIZE / 2.0) + (font_size / 3.0)
		var pos = Vector2(pos_x, cell_center_y)
		draw_string(font, pos, text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, TEXT_COLOR)


func setup_starting_board(main: Node) -> void:
	for x in range(9):
		spawn_piece(x, 6, Piece.Type.PAWN, false, main)
		spawn_piece(8 - x, 2, Piece.Type.PAWN, true, main)
	
	spawn_piece(1, 7, Piece.Type.BISHOP, false, main)
	spawn_piece(7, 7, Piece.Type.ROOK, false, main)
	spawn_piece(7, 1, Piece.Type.BISHOP, true, main)
	spawn_piece(1, 1, Piece.Type.ROOK, true, main)
	
	var bottom_row_types = [
		Piece.Type.LANCE, Piece.Type.KNIGHT, Piece.Type.SILVER, Piece.Type.GOLD,
		Piece.Type.KING,
		Piece.Type.GOLD, Piece.Type.SILVER, Piece.Type.KNIGHT, Piece.Type.LANCE
	]
	for x in range(9):
		var type = bottom_row_types[x]
		spawn_piece(x, 8, type, false, main)
		spawn_piece(8 - x, 0, type, true, main)


func spawn_piece(x: int, y: int, type: Piece.Type, is_enemy: bool, main: Node) -> void:
	if piece_scene == null:
		push_error("Piece Scene が設定されていません")
		return
	
	var piece = piece_scene.instantiate()
	add_child(piece)
	piece.init_pos(x, y, type, is_enemy, main)


func clear_pieces() -> void:
	clear_guides()
	clear_last_move_highlight()
	
	for child in get_children():
		if child is Piece:
			child.queue_free()


func show_guides(coords_list: Array[Vector2i]) -> void:
	clear_guides()
	
	for coord in coords_list:
		var rect = ColorRect.new()
		rect.size = Vector2(GameConfig.GRID_SIZE, GameConfig.GRID_SIZE)
		rect.position = Vector2(coord.x * GameConfig.GRID_SIZE, coord.y * GameConfig.GRID_SIZE)
		rect.color = GUIDE_COLOR
		rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(rect)
		active_guides.append(rect)


func clear_guides() -> void:
	for rect in active_guides:
		rect.queue_free()
	active_guides.clear()


func update_last_move_highlight(col: int, row: int) -> void:
	if last_move_rect == null:
		last_move_rect = ColorRect.new()
		last_move_rect.size = Vector2(GameConfig.GRID_SIZE, GameConfig.GRID_SIZE)
		last_move_rect.color = LAST_MOVE_COLOR
		last_move_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(last_move_rect)
	
	last_move_rect.position = Vector2(col * GameConfig.GRID_SIZE, row * GameConfig.GRID_SIZE)
	
	if last_move_tween:
		last_move_tween.kill()
	
	last_move_rect.modulate.a = 1.0
	
	last_move_tween = create_tween()
	last_move_tween.set_loops()
	last_move_tween.tween_property(last_move_rect, "modulate:a", 0.2, 0.8).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	last_move_tween.tween_property(last_move_rect, "modulate:a", 1.0, 0.8).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)


func clear_last_move_highlight() -> void:
	if last_move_tween != null:
		last_move_tween.kill()
		last_move_tween = null
	
	if last_move_rect != null:
		last_move_rect.queue_free()
		last_move_rect = null
