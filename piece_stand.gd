class_name PieceStand

extends Node2D


@export var is_enemy: bool = false


const BOARD_COLOR = Color(0.85, 0.7, 0.4)
const LINE_COLOR = Color(0.0, 0.0, 0.0)
const DISPLAY_ORDER = [
	Piece.Type.PAWN,
	Piece.Type.LANCE,
	Piece.Type.KNIGHT,
	Piece.Type.SILVER,
	Piece.Type.GOLD,
	Piece.Type.BISHOP,
	Piece.Type.ROOK,
]


var _labels: Dictionary = {}


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	queue_redraw()


func _draw() -> void:
	var rect = Rect2(0, 0, GameConfig.GRID_SIZE + 20, GameConfig.GRID_SIZE * DISPLAY_ORDER.size())
	draw_rect(rect, BOARD_COLOR)
	draw_rect(rect, LINE_COLOR, false, 2.0)


func add_piece(piece: Piece) -> void:
	piece.reparent(self)
	
	piece.current_col = -1
	piece.current_row = -1
	piece.is_enemy = is_enemy
	piece.is_promoted = false
	piece._update_display()
	
	update_layout()


func update_layout() -> void:
	var groups = {}
	for type in DISPLAY_ORDER:
		groups[type] = []
	
	for child in get_children():
		if child is Piece and not child.is_held:
			groups[child.piece_type].append(child)
	
	for label in _labels.values():
		label.visible = false
	
	var stack_index = 0
	var total_height = GameConfig.GRID_SIZE * DISPLAY_ORDER.size()
	
	var center_x = 0.0
	if is_enemy:
		center_x = GameConfig.GRID_SIZE / 2.0 + 20
	else:
		center_x = GameConfig.GRID_SIZE / 2.0
	
	for type in DISPLAY_ORDER:
		var pieces = groups[type] as Array
		if pieces.is_empty():
			continue
		
		var center_y = 0.0
		if is_enemy:
			center_y = (stack_index * GameConfig.GRID_SIZE) + (GameConfig.GRID_SIZE / 2.0)
		else:
			center_y = total_height - (stack_index * GameConfig.GRID_SIZE) - (GameConfig.GRID_SIZE / 2.0)
		
		var target_pos = Vector2(center_x, center_y)
		
		var representative = pieces[0]
		representative.visible = true

		var tween = create_tween()
		tween.tween_property(representative, "position", target_pos, 0.1)
		
		representative.rotation_degrees = 180 if is_enemy else 0
		
		for i in range(1, pieces.size()):
			pieces[i].visible = false
			pieces[i].position = target_pos
		
		if pieces.size() > 1:
			var label = _get_label(type)
			label.text = str(pieces.size())
			label.visible = true
			
			var label_offset = Vector2(35, 0)
			if is_enemy:
				label.position = target_pos - label_offset
				label.rotation_degrees = 180
			else:
				label.position = target_pos + label_offset
				label.rotation_degrees = 0
		
		stack_index += 1


func _get_label(type: Piece.Type) -> Label:
	if _labels.has(type):
		return _labels[type]
	
	var label = Label.new()
	label.add_theme_color_override("font_color", Color.BLACK)
	label.add_theme_font_size_override("font_size", 24)
	label.z_index = 5
	add_child(label)
	_labels[type] = label
	return label
