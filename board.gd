extends Node2D


@export var piece_scene: PackedScene


const BOARD_COLOR = Color(0.85, 0.7, 0.4)
const LINE_COLOR = Color(0.0, 0.0, 0.0)


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
		spawn_piece(x, 6, "pawn")
	
	spawn_piece(1, 7, "bishop")
	spawn_piece(7, 7, "rook")
	
	var bottom_row_types = [
		"lance", "knight", "silver", "gold", "king",
		"gold", "silver", "knight", "lance"
	]
	for x in range(9):
		spawn_piece(x, 8, bottom_row_types[x])


func spawn_piece(x: int, y: int, type: String) -> void:
	if piece_scene == null:
		push_error("Piece Scene が設定されていません")
		return
	
	var piece = piece_scene.instantiate()
	add_child(piece)
	piece.init_pos(x, y, type)
