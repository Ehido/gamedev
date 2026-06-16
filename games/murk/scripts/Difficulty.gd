extends Node
## Difficulty is expressed through VISIBILITY: harder = thicker fog and a
## shorter flashlight reach. This autoload is the single source of truth; the
## level listens to `changed` and re-applies fog + light settings live.

signal changed

enum Level { EASY, NORMAL, HARD, NIGHTMARE }

var level: int = Level.NORMAL

const PRESETS := {
	Level.EASY: {
		"name": "EASY",
		"fog": 0.015, "fog_color": Color(0.55, 0.60, 0.66),
		"light_range": 22.0, "light_energy": 6.0, "ambient": 0.060,
	},
	Level.NORMAL: {
		"name": "NORMAL",
		"fog": 0.035, "fog_color": Color(0.46, 0.49, 0.56),
		"light_range": 15.0, "light_energy": 5.0, "ambient": 0.030,
	},
	Level.HARD: {
		"name": "HARD",
		"fog": 0.060, "fog_color": Color(0.40, 0.42, 0.50),
		"light_range": 11.0, "light_energy": 4.5, "ambient": 0.015,
	},
	Level.NIGHTMARE: {
		"name": "NIGHTMARE",
		"fog": 0.092, "fog_color": Color(0.34, 0.36, 0.45),
		"light_range": 8.0, "light_energy": 4.0, "ambient": 0.005,
	},
}

func current() -> Dictionary:
	return PRESETS[level]

func cycle() -> void:
	level = (level + 1) % PRESETS.size()
	changed.emit()
