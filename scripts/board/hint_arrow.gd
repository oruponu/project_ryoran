class_name HintArrow

extends Node2D


const COLOR = Color(0.1, 0.6, 0.25, 0.85)
const LINE_WIDTH = 12.0
const HEAD_LENGTH = 36.0
const HEAD_WIDTH = 30.0


var _from := Vector2.ZERO
var _to := Vector2.ZERO


func setup(from: Vector2, to: Vector2) -> void:
	_from = from
	_to = to
	queue_redraw()


func _draw() -> void:
	if _from == _to:
		return

	var dir := (_to - _from).normalized()
	var shaft_end := _to - dir * HEAD_LENGTH
	draw_line(_from, shaft_end, COLOR, LINE_WIDTH)

	var side := dir.orthogonal() * (HEAD_WIDTH / 2.0)
	var points := PackedVector2Array([_to, shaft_end + side, shaft_end - side])
	draw_polygon(points, PackedColorArray([COLOR]))
