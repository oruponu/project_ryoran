class_name MainLayout

extends RefCounted


signal layout_changed(is_portrait: bool)


const LANDSCAPE_SIZE = Vector2(1600, 900)
const PORTRAIT_SIZE = Vector2(720, 1280)
const PORTRAIT_BUTTON_HEIGHT = 80.0
const PORTRAIT_HEADER_BOTTOM = 140.0
const PORTRAIT_BOTTOM_MARGIN = 40.0
const PORTRAIT_STAND_GAP = 20.0
const PORTRAIT_WIN_RATE_GAP = 24.0
const PORTRAIT_LAST_MOVE_TOP = 96.0
const PORTRAIT_LAST_MOVE_SIZE = Vector2(400, 34)


var is_portrait_layout: bool = false

var _root: Node2D
var _board: Board
var _sente_stand: PieceStand
var _gote_stand: PieceStand
var _move_history_panel: MoveHistoryPanel
var _button_container: HBoxContainer
var _win_rate_bar: WinRateBar
var _last_move_label: Label
var _common_dialog: CommonDialog

var _landscape: Dictionary = {}
var _last_window_size := Vector2i.ZERO


func _init(
	root: Node2D,
	board: Board,
	sente_stand: PieceStand,
	gote_stand: PieceStand,
	move_history_panel: MoveHistoryPanel,
	button_container: HBoxContainer,
	win_rate_bar: WinRateBar,
	last_move_label: Label,
	common_dialog: CommonDialog
) -> void:
	_root = root
	_board = board
	_sente_stand = sente_stand
	_gote_stand = gote_stand
	_move_history_panel = move_history_panel
	_button_container = button_container
	_win_rate_bar = win_rate_bar
	_last_move_label = last_move_label
	_common_dialog = common_dialog

	_landscape = {
		"board": board.position,
		"sente_stand": sente_stand.position,
		"gote_stand": gote_stand.position,
		"buttons_position": button_container.position,
		"buttons_size": button_container.size,
		"win_rate_bar": win_rate_bar.position,
	}


static func is_portrait(window_size: Vector2) -> bool:
	return window_size.y > window_size.x


static func visible_size_for(window_size: Vector2) -> Vector2:
	if not is_portrait(window_size):
		return LANDSCAPE_SIZE

	var ratio := minf(window_size.x / PORTRAIT_SIZE.x, window_size.y / PORTRAIT_SIZE.y)
	return window_size / ratio


func update() -> void:
	var window := _root.get_tree().root
	var window_size := window.size
	if window_size.x <= 0 or window_size.y <= 0 or window_size == _last_window_size:
		return
	_last_window_size = window_size

	var portrait := is_portrait(Vector2(window_size))
	if portrait:
		window.content_scale_size = Vector2i(PORTRAIT_SIZE)
		window.content_scale_aspect = Window.CONTENT_SCALE_ASPECT_EXPAND
	else:
		window.content_scale_size = Vector2i(LANDSCAPE_SIZE)
		window.content_scale_aspect = Window.CONTENT_SCALE_ASPECT_KEEP
	apply(portrait, visible_size_for(Vector2(window_size)))


func apply(portrait: bool, visible_size: Vector2) -> void:
	var changed := portrait != is_portrait_layout
	is_portrait_layout = portrait

	_set_button_height(PORTRAIT_BUTTON_HEIGHT if portrait else 0.0)
	_last_move_label.visible = portrait
	_move_history_panel.visible = not portrait
	_sente_stand.set_horizontal(portrait)
	_gote_stand.set_horizontal(portrait)

	if portrait:
		_apply_portrait(visible_size)
	else:
		_apply_landscape()

	if changed:
		layout_changed.emit(portrait)


func _set_button_height(height: float) -> void:
	for child in _button_container.get_children():
		if child is Button:
			child.custom_minimum_size.y = height
	_common_dialog.set_min_button_height(height)


func _apply_portrait(visible_size: Vector2) -> void:
	var board_size: float = GameConfig.GRID_SIZE * GameConfig.BOARD_COLS + Board.MARGIN * 2
	var stand_size := _sente_stand.get_stand_size()

	_button_container.reset_size()
	var buttons_size := _button_container.size
	var footer_top := visible_size.y - PORTRAIT_BOTTOM_MARGIN - buttons_size.y

	var block_height := stand_size.y * 2 + PORTRAIT_STAND_GAP * 2 + board_size + PORTRAIT_WIN_RATE_GAP + _win_rate_bar.size.y
	var block_top := PORTRAIT_HEADER_BOTTOM + (footer_top - PORTRAIT_HEADER_BOTTOM - block_height) / 2.0
	var board_left := (visible_size.x - board_size) / 2.0
	var board_top := block_top + stand_size.y + PORTRAIT_STAND_GAP
	var sente_top := board_top + board_size + PORTRAIT_STAND_GAP

	_gote_stand.position = Vector2(board_left, block_top)
	_board.position = Vector2(board_left + Board.MARGIN, board_top + Board.MARGIN)
	_sente_stand.position = Vector2(board_left + board_size - stand_size.x, sente_top)
	_win_rate_bar.position = Vector2(board_left, sente_top + stand_size.y + PORTRAIT_WIN_RATE_GAP)
	_button_container.position = Vector2((visible_size.x - buttons_size.x) / 2.0, footer_top)

	_last_move_label.size = PORTRAIT_LAST_MOVE_SIZE
	_last_move_label.position = Vector2((visible_size.x - PORTRAIT_LAST_MOVE_SIZE.x) / 2.0, PORTRAIT_LAST_MOVE_TOP)


func _apply_landscape() -> void:
	_board.position = _landscape.board
	_sente_stand.position = _landscape.sente_stand
	_gote_stand.position = _landscape.gote_stand
	_win_rate_bar.position = _landscape.win_rate_bar
	_button_container.position = _landscape.buttons_position
	_button_container.size = _landscape.buttons_size
