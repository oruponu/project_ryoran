class_name PieceStand

extends Node2D


@export var is_gote: bool = false


const DISPLAY_ORDER: Array[PieceState.Type] = [
	PieceState.Type.PAWN,
	PieceState.Type.LANCE,
	PieceState.Type.KNIGHT,
	PieceState.Type.SILVER,
	PieceState.Type.GOLD,
	PieceState.Type.BISHOP,
	PieceState.Type.ROOK,
]
const COUNT_LABEL_SPACE = 20
const COUNT_LABEL_OFFSET = Vector2(35, 0)
const COUNT_LABEL_FONT_SIZE = 24


var is_horizontal: bool = false
var _labels: Dictionary[PieceState.Type, Label] = {}
var _layout_tweens: Array[Tween] = []


func _ready() -> void:
	queue_redraw()


func _draw() -> void:
	var rect := Rect2(Vector2.ZERO, get_stand_size())
	draw_rect(rect, GameConfig.BOARD_COLOR)
	draw_rect(rect, GameConfig.LINE_COLOR, false, 2.0)


func get_stand_size() -> Vector2:
	var slot_count := DISPLAY_ORDER.size()
	if is_horizontal:
		return Vector2((GameConfig.GRID_SIZE + COUNT_LABEL_SPACE) * slot_count, GameConfig.GRID_SIZE)
	return Vector2(GameConfig.GRID_SIZE + COUNT_LABEL_SPACE, GameConfig.GRID_SIZE * slot_count)


func set_horizontal(value: bool) -> void:
	if is_horizontal == value:
		return

	is_horizontal = value
	queue_redraw()
	update_layout(true)


func add_piece(piece: Piece, immediate: bool = false) -> void:
	piece.reparent(self)

	move_child(piece, 0)

	piece.refresh_display()

	update_layout(immediate)


func hide_count_labels() -> void:
	for label in _labels.values():
		label.visible = false


func piece_at(local_pos: Vector2) -> PieceState:
	var half := GameConfig.GRID_SIZE / 2.0
	var cell_size := Vector2(GameConfig.GRID_SIZE, GameConfig.GRID_SIZE)
	for child in get_children():
		if child is Piece and child.visible and not child.follows_mouse:
			if Rect2(child.position - Vector2(half, half), cell_size).has_point(local_pos):
				return child.state
	return null


func update_layout(immediate: bool = false) -> void:
	for tween in _layout_tweens:
		tween.kill()
	_layout_tweens.clear()

	var groups: Dictionary[PieceState.Type, Array] = {}
	for piece_type in DISPLAY_ORDER:
		groups[piece_type] = []

	for child in get_children():
		if child is Piece and not child.follows_mouse:
			groups[child.state.piece_type].append(child)

	for label in _labels.values():
		label.visible = false

	var stack_index := 0
	for piece_type in DISPLAY_ORDER:
		var pieces: Array = groups[piece_type]
		if pieces.is_empty():
			continue

		var target_pos := _slot_center(stack_index)

		var representative: Piece = pieces[0]
		representative.visible = true

		if immediate:
			representative.position = target_pos

			for i in range(1, pieces.size()):
				pieces[i].visible = false
				pieces[i].position = target_pos
		else:
			var distance := representative.position.distance_to(target_pos)
			var is_capturing := distance > GameConfig.GRID_SIZE

			var tween := create_tween()
			_layout_tweens.append(tween)
			tween.tween_property(representative, "position", target_pos, 0.2)

			if is_capturing:
				tween.tween_callback(update_layout)

			for i in range(1, pieces.size()):
				if not is_capturing:
					pieces[i].visible = false
					pieces[i].position = target_pos

		if pieces.size() > 1:
			var label := _get_label(piece_type)
			label.text = str(pieces.size())
			label.visible = true

			if is_gote:
				label.position = target_pos - COUNT_LABEL_OFFSET
				label.rotation_degrees = 180
			else:
				label.position = target_pos + COUNT_LABEL_OFFSET
				label.rotation_degrees = 0

		stack_index += 1


func _slot_center(stack_index: int) -> Vector2:
	var half := GameConfig.GRID_SIZE / 2.0
	var offset_x: float = half + COUNT_LABEL_SPACE if is_gote else half
	if is_horizontal:
		var slot_width: float = GameConfig.GRID_SIZE + COUNT_LABEL_SPACE
		var slot_left: float = stack_index * slot_width if is_gote else get_stand_size().x - (stack_index + 1) * slot_width
		return Vector2(slot_left + offset_x, half)

	var center_y: float = stack_index * GameConfig.GRID_SIZE + half if is_gote else get_stand_size().y - stack_index * GameConfig.GRID_SIZE - half
	return Vector2(offset_x, center_y)


func _get_label(piece_type: PieceState.Type) -> Label:
	if _labels.has(piece_type):
		return _labels[piece_type]

	var label := Label.new()
	label.add_theme_color_override("font_color", Color.BLACK)
	label.add_theme_font_size_override("font_size", COUNT_LABEL_FONT_SIZE)
	label.z_index = 5
	add_child(label)
	_labels[piece_type] = label
	return label
