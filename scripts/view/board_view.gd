class_name BoardView

extends RefCounted


var _game_state: GameState
var _board: Board
var _player_piece_stand: PieceStand
var _enemy_piece_stand: PieceStand
var _piece_scene: PackedScene

var _nodes: Dictionary = {}


func _init(
	game_state: GameState,
	board: Board,
	player_piece_stand: PieceStand,
	enemy_piece_stand: PieceStand,
	piece_scene: PackedScene
) -> void:
	_game_state = game_state
	_board = board
	_player_piece_stand = player_piece_stand
	_enemy_piece_stand = enemy_piece_stand
	_piece_scene = piece_scene


func register(state: PieceState, piece: Piece) -> void:
	if _nodes.has(state):
		push_error("BoardView.register: 二重登録です")
	_nodes[state] = piece


func node_for(state: PieceState) -> Piece:
	var piece: Piece = _nodes.get(state)
	if piece == null:
		push_error("BoardView.node_for: 未登録のPieceStateです")
	return piece


func clear() -> void:
	_nodes.clear()
