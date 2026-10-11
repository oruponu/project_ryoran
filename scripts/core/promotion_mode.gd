class_name PromotionMode

extends RefCounted


enum Type {
	ASK_USER,
	FORCE_PROMOTE,
	FORCE_STAY
}


static func from_moves(moves: Array[Dictionary]) -> Type:
	var can_promote := false
	var can_stay := false
	for move in moves:
		if move.is_promotion:
			can_promote = true
		else:
			can_stay = true

	if can_promote and can_stay:
		return Type.ASK_USER
	if can_promote:
		return Type.FORCE_PROMOTE
	return Type.FORCE_STAY
