class_name RepetitionTracker

extends RefCounted


enum Sennichite { DRAW, TURN_PERPETUAL, OPPONENT_PERPETUAL }

var _hashes: Array[int] = []
var _in_checks: Array[bool] = []


func reset(initial_hash: int, in_check: bool) -> void:
	_hashes = [initial_hash]
	_in_checks = [in_check]


func record(hash: int, in_check: bool) -> int:
	_hashes.append(hash)
	_in_checks.append(in_check)
	return _hashes.count(hash)


func undo() -> void:
	if _hashes.size() > 1:
		_hashes.pop_back()
		_in_checks.pop_back()


func classify_sennichite() -> Sennichite:
	var n := _hashes.size() - 1
	var p := n - 2
	while p >= 0:
		if _hashes[p] == _hashes[n]:
			var turn_perpetual := true
			var q := p + 1
			while q <= n - 1:
				if not _in_checks[q]:
					turn_perpetual = false
					break
				q += 2
			var opponent_perpetual := true
			q = p + 2
			while q <= n:
				if not _in_checks[q]:
					opponent_perpetual = false
					break
				q += 2
			if turn_perpetual:
				return Sennichite.TURN_PERPETUAL
			if opponent_perpetual:
				return Sennichite.OPPONENT_PERPETUAL
			return Sennichite.DRAW
		p -= 2
	return Sennichite.DRAW


func history_hashes() -> PackedInt64Array:
	return PackedInt64Array(_hashes.slice(0, _hashes.size() - 1))


func history_in_checks() -> PackedByteArray:
	var result := PackedByteArray()
	for i in _in_checks.size() - 1:
		result.append(1 if _in_checks[i] else 0)
	return result
