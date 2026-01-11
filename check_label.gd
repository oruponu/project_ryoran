extends Label


var _tween: Tween = null


func play_animation() -> void:
	if _tween != null and _tween.is_valid():
		_tween.kill()
	
	visible = true
	modulate.a = 1.0
	
	var viewport_rect = get_viewport().get_visible_rect()
	var screen_width = viewport_rect.size.x
	var label_width = size.x
	
	var start_pos_x = screen_width + label_width
	var center_pos_x = screen_width / 2.0 - label_width / 2.0
	var end_pos_x = -label_width * 1.5
	
	var current_y = position.y
	
	position = Vector2(start_pos_x, current_y)
	
	_tween = create_tween()
	_tween.tween_property(self, "position:x", center_pos_x, 0.5).set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	_tween.tween_interval(1)
	_tween.tween_property(self, "position:x", end_pos_x, 0.5).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_IN)
	_tween.parallel().tween_property(self, "modulate:a", 0.0, 0.5)
	
	_tween.tween_callback(func(): visible = false)
