class_name RepetitionTracker

extends RefCounted


var _keys: Array[int] = []


func reset(initial_key: int) -> void:
	_keys = [initial_key]


func record(key: int) -> int:
	_keys.append(key)
	return _keys.count(key)


func undo() -> void:
	if _keys.size() > 1:
		_keys.pop_back()
