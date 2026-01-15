extends AudioStreamPlayer


const SE_PLACE = preload("res://assets/audio/place.wav")
const SE_CHECK = preload("res://assets/audio/check.wav")


var _check_player: AudioStreamPlayer


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	_check_player = AudioStreamPlayer.new()
	add_child(_check_player)
	
	volume_db = -6.0
	_check_player.volume_db = volume_db


func play_place():
	stream = SE_PLACE
	play()


func play_check():
	_check_player.stream = SE_CHECK
	_check_player.play()
