extends Node2D


@export var piece_scene: PackedScene


const BOARD_COLOR = Color(0.85, 0.7, 0.4)
const LINE_COLOR = Color(0.0, 0.0, 0.0)
const GUIDE_COLOR = Color(0.0, 0.7, 1.0, 0.4)


var active_guides: Array[ColorRect] = []


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	queue_redraw()
	setup_starting_board()


func _draw() -> void:
	var board_rect = Rect2(0, 0, GameConfig.GRID_SIZE * GameConfig.BOARD_COLS, GameConfig.GRID_SIZE * GameConfig.BOARD_ROWS)
	draw_rect(board_rect, BOARD_COLOR)
	
	for x in range(GameConfig.BOARD_COLS + 1):
		var start_pos = Vector2(x * GameConfig.GRID_SIZE, 0)
		var end_pos = Vector2(x * GameConfig.GRID_SIZE, GameConfig.BOARD_ROWS * GameConfig.GRID_SIZE)
		draw_line(start_pos, end_pos, LINE_COLOR, 2.0)
	
	for y in range(GameConfig.BOARD_ROWS + 1):
		var start_pos = Vector2(0, y * GameConfig.GRID_SIZE)
		var end_pos = Vector2(GameConfig.BOARD_COLS * GameConfig.GRID_SIZE, y * GameConfig.GRID_SIZE)
		draw_line(start_pos, end_pos, LINE_COLOR, 2.0)


func setup_starting_board() -> void:
	for x in range(9):
		spawn_piece(x, 6, Piece.Type.PAWN, false)
		spawn_piece(8 - x, 2, Piece.Type.PAWN, true)
	
	spawn_piece(1, 7, Piece.Type.BISHOP, false)
	spawn_piece(7, 7, Piece.Type.ROOK, false)
	spawn_piece(7, 1, Piece.Type.BISHOP, true)
	spawn_piece(1, 1, Piece.Type.ROOK, true)
	
	var bottom_row_types = [
		Piece.Type.LANCE, Piece.Type.KNIGHT, Piece.Type.SILVER, Piece.Type.GOLD,
		Piece.Type.KING,
		Piece.Type.GOLD, Piece.Type.SILVER, Piece.Type.KNIGHT, Piece.Type.LANCE
	]
	for x in range(9):
		var type = bottom_row_types[x]
		spawn_piece(x, 8, type, false)
		spawn_piece(8 - x, 0, type, true)
		


func spawn_piece(x: int, y: int, type: Piece.Type, is_enemy: bool) -> void:
	if piece_scene == null:
		push_error("Piece Scene が設定されていません")
		return
	
	var piece = piece_scene.instantiate()
	add_child(piece)
	piece.init_pos(x, y, type, is_enemy)
	piece.request_show_guides.connect(show_guides)
	piece.request_clear_guides.connect(clear_guides)


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
