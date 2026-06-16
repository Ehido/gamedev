extends Node3D
## Builds the test level in code: a dark room, scattered crates, a first-person
## player with a flashlight, drifting volumetric fog, and a HUD. Difficulty
## drives the fog density + flashlight reach, applied live.

const PlayerScript := preload("res://scripts/Player.gd")
const MovingFog := preload("res://shaders/moving_fog.gdshader")

const STAMINA_BAR_W := 180.0

var _env: Environment
var _fog_material: ShaderMaterial
var _hud: Label
var _player: CharacterBody3D
var _stamina_fill: ColorRect

func _ready() -> void:
	_build_environment()
	_build_room()
	_build_lights()
	_build_player()
	_build_fog_volume()
	_build_hud()
	Difficulty.changed.connect(_apply_difficulty)
	_apply_difficulty()

func _build_environment() -> void:
	_env = Environment.new()
	_env.background_mode = Environment.BG_COLOR
	_env.background_color = Color(0.04, 0.05, 0.07)
	_env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	_env.ambient_light_color = Color(0.45, 0.50, 0.62)
	_env.ambient_light_energy = 0.20
	_env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	_env.volumetric_fog_enabled = true
	_env.volumetric_fog_density = 0.035
	_env.volumetric_fog_albedo = Color(0.62, 0.64, 0.72)
	_env.volumetric_fog_length = 64.0
	# Smooth out froxel "slice" banding in the light shafts.
	_env.volumetric_fog_temporal_reprojection_enabled = true
	_env.volumetric_fog_temporal_reprojection_amount = 0.95
	# Let ambient + sky light actually illuminate the fog so it reads as fog
	# instead of a black void.
	_env.volumetric_fog_ambient_inject = 1.5
	_env.volumetric_fog_gi_inject = 0.0
	var world_env := WorldEnvironment.new()
	world_env.environment = _env
	add_child(world_env)

func _build_lights() -> void:
	# Dim, colored work-lamps scattered around: they give the space shape and,
	# crucially, give the volumetric fog something to catch so you can see it.
	_add_lamp(Vector3(-11, 3.0, -8), Color(1.0, 0.72, 0.42), 6.0, 16.0)
	_add_lamp(Vector3(12, 3.0, 7), Color(0.5, 0.72, 1.0), 5.0, 16.0)
	_add_lamp(Vector3(2, 3.2, -16), Color(1.0, 0.82, 0.55), 5.0, 15.0)
	_add_lamp(Vector3(-6, 3.0, 12), Color(0.7, 0.85, 1.0), 4.0, 14.0)

func _add_lamp(pos: Vector3, color: Color, energy: float, range_m: float) -> void:
	var lamp := OmniLight3D.new()
	lamp.position = pos
	lamp.light_color = color
	lamp.light_energy = energy
	lamp.omni_range = range_m
	lamp.set_meta("base_energy", energy)  # so difficulty can dim them
	lamp.add_to_group("lamp")
	add_child(lamp)

func _build_room() -> void:
	var wall_mat := StandardMaterial3D.new()
	wall_mat.albedo_color = Color(0.15, 0.16, 0.20)
	wall_mat.roughness = 1.0
	var s := 44.0
	_add_box(Vector3(s, 1, s), Vector3(0, -0.5, 0), wall_mat)        # floor
	_add_box(Vector3(s, 8, 1), Vector3(0, 4, -s * 0.5), wall_mat)    # wall -Z
	_add_box(Vector3(s, 8, 1), Vector3(0, 4, s * 0.5), wall_mat)     # wall +Z
	_add_box(Vector3(1, 8, s), Vector3(-s * 0.5, 4, 0), wall_mat)    # wall -X
	_add_box(Vector3(1, 8, s), Vector3(s * 0.5, 4, 0), wall_mat)     # wall +X

	var crate_mat := StandardMaterial3D.new()
	crate_mat.albedo_color = Color(0.30, 0.25, 0.18)
	crate_mat.roughness = 1.0
	var rng := RandomNumberGenerator.new()
	rng.seed = 20260616
	for i in 26:
		var c := rng.randf_range(0.8, 1.9)
		var x := rng.randf_range(-s * 0.5 + 3.0, s * 0.5 - 3.0)
		var z := rng.randf_range(-s * 0.5 + 3.0, s * 0.5 - 3.0)
		_add_box(Vector3(c, c, c), Vector3(x, c * 0.5, z), crate_mat)

func _add_box(box_size: Vector3, pos: Vector3, mat: Material) -> void:
	var b := CSGBox3D.new()
	b.size = box_size
	b.position = pos
	b.material = mat
	b.use_collision = true
	add_child(b)

func _build_player() -> void:
	var player := CharacterBody3D.new()
	player.position = Vector3(0, 1, 16)
	player.set_script(PlayerScript)
	player.add_to_group("player")

	var col := CollisionShape3D.new()
	var capsule := CapsuleShape3D.new()
	capsule.radius = 0.4
	capsule.height = 1.8
	col.shape = capsule
	col.position = Vector3(0, 0.9, 0)
	player.add_child(col)

	var cam := Camera3D.new()
	cam.name = "Camera"
	cam.position = Vector3(0, 1.6, 0)
	cam.current = true
	player.add_child(cam)

	var flashlight := SpotLight3D.new()
	flashlight.position = Vector3(0.25, -0.2, 0)
	flashlight.spot_range = 15.0
	flashlight.spot_angle = 35.0
	flashlight.light_energy = 5.0
	flashlight.shadow_enabled = true
	flashlight.add_to_group("flashlight")
	cam.add_child(flashlight)

	_player = player
	add_child(player)

func _build_fog_volume() -> void:
	var fog := FogVolume.new()
	fog.shape = RenderingServer.FOG_VOLUME_SHAPE_BOX
	fog.size = Vector3(48, 10, 48)
	fog.position = Vector3(0, 5, 0)
	_fog_material = ShaderMaterial.new()
	_fog_material.shader = MovingFog
	fog.material = _fog_material
	add_child(fog)

func _build_hud() -> void:
	var canvas := CanvasLayer.new()
	add_child(canvas)
	_hud = Label.new()
	_hud.position = Vector2(20, 18)
	_hud.add_theme_font_size_override("font_size", 22)
	canvas.add_child(_hud)
	var help := Label.new()
	help.position = Vector2(20, 50)
	help.text = "WASD move   -   Shift sprint   -   Mouse look   -   F flashlight   -   TAB difficulty   -   Esc/click cursor"
	canvas.add_child(help)

	var stam_label := Label.new()
	stam_label.position = Vector2(20, 78)
	stam_label.text = "STAMINA"
	canvas.add_child(stam_label)
	var stam_bg := ColorRect.new()
	stam_bg.position = Vector2(110, 80)
	stam_bg.size = Vector2(STAMINA_BAR_W, 14)
	stam_bg.color = Color(0.10, 0.10, 0.13, 0.8)
	canvas.add_child(stam_bg)
	_stamina_fill = ColorRect.new()
	_stamina_fill.position = Vector2(110, 80)
	_stamina_fill.size = Vector2(STAMINA_BAR_W, 14)
	_stamina_fill.color = Color(0.45, 0.78, 1.0, 0.9)
	canvas.add_child(_stamina_fill)

func _process(_delta: float) -> void:
	if _player and _stamina_fill:
		_stamina_fill.size.x = STAMINA_BAR_W * _player.stamina_ratio()

func _apply_difficulty() -> void:
	var d := Difficulty.current()
	_env.volumetric_fog_density = d["fog"]
	_env.volumetric_fog_albedo = d["fog_color"]
	_env.ambient_light_energy = d["ambient"]

	if _fog_material:
		_fog_material.set_shader_parameter("base_density", float(d["fog"]) * 0.6)
		var c: Color = d["fog_color"]
		_fog_material.set_shader_parameter("fog_albedo", Vector3(c.r, c.g, c.b))

	for light in get_tree().get_nodes_in_group("flashlight"):
		light.spot_range = d["light_range"]
		light.light_energy = d["light_energy"]

	var lamp_scale: float = d.get("lamp", 1.0)
	for lamp in get_tree().get_nodes_in_group("lamp"):
		lamp.light_energy = float(lamp.get_meta("base_energy")) * lamp_scale

	if _hud:
		_hud.text = "DIFFICULTY: %s    (fog %.3f  -  light reach %.0fm)" % [
			d["name"], d["fog"], d["light_range"]]
