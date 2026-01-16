extends Control


@onready var sente_bar = $LayoutContainer/MarginContainer/BarArea/SenteBar
@onready var sente_value_label = $LayoutContainer/SenteValueLabel
@onready var gote_value_label = $LayoutContainer/GoteValueLabel


var _tween: Tween


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	reset_bar()


func reset_bar():
	update_bar(0.5, true)


func update_bar(sente_win_rate: float, instant: bool = false):
	var target_ratio = clamp(sente_win_rate, 0.0, 1.0)
	
	var sente_percent = int(target_ratio * 100)
	var gote_percent = 100 - sente_percent
	
	sente_value_label.text = str(sente_percent)
	gote_value_label.text = str(gote_percent)
	
	if _tween:
		_tween.kill()
	
	if instant:
		sente_bar.anchor_right = target_ratio
	else:
		_tween = create_tween().set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
		_tween.tween_property(sente_bar, "anchor_right", target_ratio, 0.5)
