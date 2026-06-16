extends CharacterBody3D
## First-person walker. Mouse looks, WASD moves, TAB cycles difficulty so you
## can watch the fog/light change live, Esc frees the cursor. The flashlight is
## a child of the camera, so it always points where you look.

@export var speed: float = 4.5
@export var mouse_sensitivity: float = 0.0025
@export var gravity: float = 16.0

var _camera: Camera3D
var _pitch: float = 0.0

func _ready() -> void:
	_camera = get_node_or_null("Camera")
	# Don't grab the mouse in headless validation runs (no window to draw to).
	if DisplayServer.get_name() != "headless":
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		rotate_y(-event.relative.x * mouse_sensitivity)
		_pitch = clamp(_pitch - event.relative.y * mouse_sensitivity, -1.4, 1.4)
		if _camera:
			_camera.rotation.x = _pitch
	elif event is InputEventMouseButton and event.pressed:
		# Click back into the window to re-capture the mouse and look around again.
		if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	elif event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_ESCAPE:
			_toggle_mouse()
		elif event.keycode == KEY_TAB:
			Difficulty.cycle()

func _toggle_mouse() -> void:
	# Esc frees the cursor; Esc again (or a click) grabs it back so you can keep looking.
	if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	else:
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _physics_process(delta: float) -> void:
	if is_on_floor():
		if velocity.y < 0.0:
			velocity.y = 0.0
	else:
		velocity.y -= gravity * delta

	var input_dir := Vector3.ZERO
	if Input.is_physical_key_pressed(KEY_W):
		input_dir.z -= 1.0
	if Input.is_physical_key_pressed(KEY_S):
		input_dir.z += 1.0
	if Input.is_physical_key_pressed(KEY_A):
		input_dir.x -= 1.0
	if Input.is_physical_key_pressed(KEY_D):
		input_dir.x += 1.0

	var dir := transform.basis * input_dir.normalized()
	dir.y = 0.0
	dir = dir.normalized()
	velocity.x = dir.x * speed
	velocity.z = dir.z * speed
	move_and_slide()
