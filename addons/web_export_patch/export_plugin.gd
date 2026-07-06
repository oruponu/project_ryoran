@tool
extends EditorExportPlugin

const TARGET := "INITIAL_MEMORY=Module[\"INITIAL_MEMORY\"]||33554432"
const PATCHED := "INITIAL_MEMORY=536870912"

var _js_path := ""


func _get_name() -> String:
	return "WebExportPatch"


func _export_begin(features: PackedStringArray, _is_debug: bool, path: String, _flags: int) -> void:
	_js_path = path.get_base_dir().path_join("index.js") if features.has("web") else ""


func _export_end() -> void:
	if _js_path.is_empty():
		return
	var text := FileAccess.get_file_as_string(_js_path)
	if not text.contains(TARGET):
		if not text.contains(PATCHED):
			push_error("[WebExportPatch] INITIAL_MEMORY not found: " + _js_path)
		return
	FileAccess.open(_js_path, FileAccess.WRITE).store_string(text.replace(TARGET, PATCHED))
	print("[WebExportPatch] Patched INITIAL_MEMORY to 512MiB: " + _js_path)
