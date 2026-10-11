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


var _labels: Dictionary[PieceState.Type, Label] = {}


func _ready() -> void:
	queue_redraw()


func _draw() -> void:
	var rect := Rect2(0, 0, GameConfig.GRID_SIZE + COUNT_LABEL_SPACE, GameConfig.GRID_SIZE * DISPLAY_ORDER.size())
	draw_rect(rect, GameConfig.BOARD_COLOR)
	draw_rect(rect, GameConfig.LINE_COLOR, false, 2.0)


func add_piece(piece: Piece, immediate: bool = false) -> void:
	piece.reparent(self)

	move_child(piece, 0)

	piece.refresh_display()

	update_layout(immediate)


func hide_count_labels() -> void:
	for label in _labels.values():
		label.visible = false


func update_layout(immediate: bool = false) -> void:
	var groups: Dictionary[PieceState.Type, Array] = {}
	for piece_type in DISPLAY_ORDER:
		groups[piece_type] = []

	for child in get_children():
		if child is Piece and not child.is_held:
			groups[child.state.piece_type].append(child)

	for label in _labels.values():
		label.visible = false

	var stack_index := 0
	var total_height := GameConfig.GRID_SIZE * DISPLAY_ORDER.size()

	var center_x := 0.0
	if is_gote:
		center_x = GameConfig.GRID_SIZE / 2.0 + COUNT_LABEL_SPACE
	else:
		center_x = GameConfig.GRID_SIZE / 2.0

	for piece_type in DISPLAY_ORDER:
		var pieces: Array = groups[piece_type]
		if pieces.is_empty():
			continue

		var center_y := 0.0
		if is_gote:
			center_y = (stack_index * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
		else:
			center_y = total_height - (stack_index * GameConfig.GRID_SIZE) - (GameConfig.GRID_SIZE / 2.0)

		var target_pos := Vector2(center_x, center_y)

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
