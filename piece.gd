extends Area2D


var is_held = false


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(_delta: float) -> void:
	if is_held:
		global_position = get_global_mouse_position()


func _on_input_event(_viewport: Node, event: InputEvent, _shape_idx: int) -> void:
	if event is InputEventMouseButton and event.is_pressed():
		is_held = !is_held
		if is_held:
			z_index = 10
		else:
			z_index = 0
