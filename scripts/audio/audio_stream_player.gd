extends AudioStreamPlayer


const SE_PLACE = preload("res://assets/audio/place.wav")
const SE_CHECK = preload("res://assets/audio/check.wav")


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	volume_db = -6.0


func play_place():
	stream = SE_PLACE
	play()


func play_check():
	stream = SE_CHECK
	play()
