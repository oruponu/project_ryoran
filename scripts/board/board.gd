class_name Board

extends Node2D


@export var piece_scene: PackedScene


const TEXT_COLOR = Color(0.0, 0.0, 0.0)
const GUIDE_COLOR = Color(0.0, 0.7, 1.0, 0.4)
const LAST_MOVE_COLOR = Color(1.0, 0.4, 0.0, 0.2)
const SELECTION_COLOR = Color(1.0, 0.55, 0.0)
const SELECTION_WIDTH = 4.0
const MARGIN = 22.5


var _active_guides: Array[ColorRect] = []
var _last_move_rect: ColorRect = null
var _last_move_tween: Tween = null
var _hint_arrows: Array[HintArrow] = []
var _selection_rect: ReferenceRect = null


func _ready() -> void:
	queue_redraw()


func _draw() -> void:
	var board_width := GameConfig.GRID_SIZE * GameConfig.BOARD_COLS
	var board_height := GameConfig.GRID_SIZE * GameConfig.BOARD_ROWS
	var background_rect := Rect2(
		Vector2(-MARGIN, -MARGIN),
		Vector2(board_width + MARGIN * 2, board_height + MARGIN * 2)
	)
	draw_rect(background_rect, GameConfig.BOARD_COLOR)
	draw_rect(background_rect, GameConfig.LINE_COLOR, false, 2.0)

	for col in range(GameConfig.BOARD_COLS + 1):
		var start_pos := Vector2(col * GameConfig.GRID_SIZE, 0)
		var end_pos := Vector2(col * GameConfig.GRID_SIZE, GameConfig.BOARD_ROWS * GameConfig.GRID_SIZE)
		draw_line(start_pos, end_pos, GameConfig.LINE_COLOR, 2.0)

	for row in range(GameConfig.BOARD_ROWS + 1):
		var start_pos := Vector2(0, row * GameConfig.GRID_SIZE)
		var end_pos := Vector2(GameConfig.BOARD_COLS * GameConfig.GRID_SIZE, row * GameConfig.GRID_SIZE)
		draw_line(start_pos, end_pos, GameConfig.LINE_COLOR, 2.0)

	_draw_coordinates()


func _draw_coordinates() -> void:
	var font := ThemeDB.get_fallback_font()
	var font_size := 16
	var offset_y := -5
	var offset_x := 3

	for col in range(GameConfig.BOARD_COLS):
		var text := GameConfig.COL_LABELS[col]
		var pos_x := col * GameConfig.GRID_SIZE
		var pos := Vector2(pos_x, offset_y)
		draw_string(font, pos, text, HORIZONTAL_ALIGNMENT_CENTER, GameConfig.GRID_SIZE, font_size, TEXT_COLOR)

	for row in range(GameConfig.BOARD_ROWS):
		var text := GameConfig.ROW_LABELS[row]
		var pos_x := GameConfig.BOARD_COLS * GameConfig.GRID_SIZE + offset_x
		var cell_center_y := row * GameConfig.GRID_SIZE + (GameConfig.GRID_SIZE / 2.0) + (font_size / 3.0)
		var pos := Vector2(pos_x, cell_center_y)
		draw_string(font, pos, text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, TEXT_COLOR)


func clear_overlays() -> void:
	clear_guides()
	clear_selection()
	clear_last_move_highlight()
	clear_hint_arrows()


func show_guides(coords_list: Array[Vector2i]) -> void:
	clear_guides()

	for coord in coords_list:
		var rect := ColorRect.new()
		rect.size = Vector2(GameConfig.GRID_SIZE, GameConfig.GRID_SIZE)
		rect.position = Vector2(coord.x * GameConfig.GRID_SIZE, coord.y * GameConfig.GRID_SIZE)
		rect.color = GUIDE_COLOR
		rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(rect)
		_active_guides.append(rect)


func clear_guides() -> void:
	for rect in _active_guides:
		rect.queue_free()
	_active_guides.clear()


func show_selection(coord: Vector2i) -> void:
	if _selection_rect == null:
		_selection_rect = ReferenceRect.new()
		_selection_rect.editor_only = false
		_selection_rect.border_color = SELECTION_COLOR
		_selection_rect.border_width = SELECTION_WIDTH
		_selection_rect.size = Vector2(GameConfig.GRID_SIZE, GameConfig.GRID_SIZE)
		_selection_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
		_selection_rect.z_index = 11
		add_child(_selection_rect)

	_selection_rect.position = Vector2(coord * GameConfig.GRID_SIZE)
	_selection_rect.visible = true


func clear_selection() -> void:
	if _selection_rect != null:
		_selection_rect.visible = false


func update_last_move_highlight(col: int, row: int) -> void:
	if _last_move_rect == null:
		_last_move_rect = ColorRect.new()
		_last_move_rect.size = Vector2(GameConfig.GRID_SIZE, GameConfig.GRID_SIZE)
		_last_move_rect.color = LAST_MOVE_COLOR
		_last_move_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(_last_move_rect)

	_last_move_rect.position = Vector2(col * GameConfig.GRID_SIZE, row * GameConfig.GRID_SIZE)

	if _last_move_tween:
		_last_move_tween.kill()

	_last_move_rect.modulate.a = 1.0

	_last_move_tween = create_tween()
	_last_move_tween.set_loops()
	_last_move_tween.tween_property(_last_move_rect, "modulate:a", 0.2, 0.8).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	_last_move_tween.tween_property(_last_move_rect, "modulate:a", 1.0, 0.8).set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)


func clear_last_move_highlight() -> void:
	if _last_move_tween != null:
		_last_move_tween.kill()
		_last_move_tween = null

	if _last_move_rect != null:
		_last_move_rect.queue_free()
		_last_move_rect = null


func show_hint_arrows(entries: Array[Dictionary]) -> void:
	clear_hint_arrows()

	for entry in entries:
		var arrow := HintArrow.new()
		arrow.z_index = 6
		add_child(arrow)
		arrow.setup(entry.from, entry.to)
		_hint_arrows.append(arrow)


func clear_hint_arrows() -> void:
	for arrow in _hint_arrows:
		arrow.queue_free()
	_hint_arrows.clear()
