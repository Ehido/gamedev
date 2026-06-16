shader_type canvas_item;
// Full-screen post pass: a soft vignette and subtle film grain over the
// rendered frame, for a lo-fi horror mood. Drawn under the HUD.

uniform sampler2D screen_tex : hint_screen_texture, filter_linear_mipmap;
uniform float vignette_amount = 0.35;
uniform float grain_amount = 0.035;

void fragment() {
	vec3 col = texture(screen_tex, SCREEN_UV).rgb;

	// Vignette: darken toward the edges.
	vec2 d = SCREEN_UV - vec2(0.5);
	float vig = smoothstep(0.75, 0.12, dot(d, d) * 2.2);
	col *= mix(1.0 - vignette_amount, 1.0, vig);

	// Animated film grain.
	float n = fract(sin(dot(SCREEN_UV * vec2(1213.0, 3221.0) + TIME,
		vec2(12.9898, 78.233))) * 43758.5453);
	col += (n - 0.5) * grain_amount;

	COLOR = vec4(col, 1.0);
}
