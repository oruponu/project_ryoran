extends Node


const GRID_SIZE = 70
const BOARD_COLS = 9
const BOARD_ROWS = 9
const BOARD_COLOR = Color(0.85, 0.7, 0.4)
const LINE_COLOR = Color(0.0, 0.0, 0.0)
const KANJI_NUMS = ["一", "二", "三", "四", "五", "六", "七", "八", "九"]
const ARABIC_NUMS = ["１", "２", "３", "４", "５", "６", "７", "８", "９"]
const SENNICHITE_COUNT = 4
const MIN_AI_RESPONSE_TIME_SEC = 0.5


func cell_to_position(col: int, row: int) -> Vector2:
	return Vector2(
		(col * GRID_SIZE) + (GRID_SIZE / 2.0),
		(row * GRID_SIZE) + (GRID_SIZE / 2.0)
	)


func position_to_cell(local_pos: Vector2) -> Vector2i:
	return Vector2i(
		floori(local_pos.x / GRID_SIZE),
		floori(local_pos.y / GRID_SIZE)
	)
