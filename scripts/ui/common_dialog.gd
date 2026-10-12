class_name CommonDialog

extends CanvasLayer


signal decided(result: bool)


@onready var message_label: Label = $Overlay/PanelContainer/VBoxContainer/MessageLabel
@onready var yes_button: Button = $Overlay/PanelContainer/VBoxContainer/HBoxContainer/YesButton
@onready var no_button: Button = $Overlay/PanelContainer/VBoxContainer/HBoxContainer/NoButton


func _ready() -> void:
	yes_button.pressed.connect(_on_yes_pressed)
	no_button.pressed.connect(_on_no_pressed)
	hide()


func ask_user(message: String, yes_text: String = "はい", no_text: String = "いいえ") -> bool:
	message_label.text = message
	yes_button.text = yes_text
	no_button.text = no_text

	if no_text == "":
		no_button.hide()
	else:
		no_button.show()

	show()
	var result: bool = await decided
	return result


func set_min_button_height(height: float) -> void:
	yes_button.custom_minimum_size.y = height
	no_button.custom_minimum_size.y = height


func _on_yes_pressed() -> void:
	hide()
	decided.emit(true)


func _on_no_pressed() -> void:
	hide()
	decided.emit(false)
