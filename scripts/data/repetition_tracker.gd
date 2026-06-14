class_name RepetitionTracker

extends RefCounted


enum { DRAW, STM_PERPETUAL, OPP_PERPETUAL }

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


func classify_sennichite() -> int:
	var n := _hashes.size() - 1
	var p := n - 2
	while p >= 0:
		if _hashes[p] == _hashes[n]:
			var stm_perpetual := true
			var q := p + 1
			while q <= n - 1:
				if not _in_checks[q]:
					stm_perpetual = false
					break
				q += 2
			var opp_perpetual := true
			q = p + 2
			while q <= n:
				if not _in_checks[q]:
					opp_perpetual = false
					break
				q += 2
			if stm_perpetual:
				return STM_PERPETUAL
			if opp_perpetual:
				return OPP_PERPETUAL
			return DRAW
		p -= 2
	return DRAW
