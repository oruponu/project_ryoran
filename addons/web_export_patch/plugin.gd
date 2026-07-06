@tool
extends EditorPlugin

var _plugin := preload("export_plugin.gd").new()


func _enter_tree() -> void:
	add_export_plugin(_plugin)


func _exit_tree() -> void:
	remove_export_plugin(_plugin)
