extends CanvasLayer


signal decision_mode(result: bool)


@onready var message_label = $Overlay/PanelContainer/VBoxContainer/MessageLabel
@onready var yes_button = $Overlay/PanelContainer/VBoxContainer/HBoxContainer/YesButton
@onready var no_button = $Overlay/PanelContainer/VBoxContainer/HBoxContainer/NoButton


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	yes_button.pressed.connect(_on_yes_pressed)
	no_button.pressed.connect(_on_no_pressed)
	hide()


func ask_user(message: String, yes_text: String = "はい", no_text: String = "いいえ") -> bool:
	message_label.text = message
	yes_button.text = yes_text
	no_button.text = no_text
	
	show()
	var result = await decision_mode
	return result


func _on_yes_pressed() -> void:
	hide()
	decision_mode.emit(true)


func _on_no_pressed() -> void:
	hide()
	decision_mode.emit(false)
