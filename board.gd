extends Node2D


const GRID_SIZE = 70
const COLS = 9
const ROWS = 9
const BOARD_COLOR = Color(0.85, 0.7, 0.4)
const LINE_COLOR = Color(0.0, 0.0, 0.0)


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	queue_redraw()


func _draw() -> void:
	var board_rect = Rect2(0, 0, GRID_SIZE * COLS, GRID_SIZE * ROWS)
	draw_rect(board_rect, BOARD_COLOR)
	
	for x in range(COLS + 1):
		var start_pos = Vector2(x * GRID_SIZE, 0)
		var end_pos = Vector2(x * GRID_SIZE, ROWS * GRID_SIZE)
		draw_line(start_pos, end_pos, LINE_COLOR, 2.0)
	
	for y in range(ROWS + 1):
		var start_pos = Vector2(0, y * GRID_SIZE)
		var end_pos = Vector2(COLS * GRID_SIZE, y * GRID_SIZE)
		draw_line(start_pos, end_pos, LINE_COLOR, 2.0)
