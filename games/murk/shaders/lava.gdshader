shader_type spatial;
// Animated, self-lit lava: scrolling fbm noise blended between a cool and a hot
// colour, pushed through EMISSION so it glows in the dark.
render_mode cull_disabled;

uniform float flow_speed = 0.08;
uniform vec3 hot_color : source_color = vec3(1.0, 0.55, 0.08);
uniform vec3 cool_color : source_color = vec3(0.45, 0.05, 0.0);
uniform float emission_strength = 2.2;

float hash(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

float noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = hash(i);
	float b = hash(i + vec2(1.0, 0.0));
	float c = hash(i + vec2(0.0, 1.0));
	float d = hash(i + vec2(1.0, 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
	float v = 0.0;
	float amp = 0.5;
	for (int i = 0; i < 4; i++) {
		v += amp * noise(p);
		p *= 2.0;
		amp *= 0.5;
	}
	return v;
}

void fragment() {
	vec2 uv = UV * 8.0;
	float t = TIME * flow_speed;
	float n = fbm(uv + vec2(t, t * 0.6));
	n += 0.3 * fbm(uv * 2.0 - vec2(t * 0.8, t));
	float m = smoothstep(0.3, 0.95, n);
	vec3 col = mix(cool_color, hot_color, m);
	ALBEDO = col;
	EMISSION = col * emission_strength * (0.4 + m);
	ROUGHNESS = 0.5;
	METALLIC = 0.0;
}
