shader_type spatial;
// Molten lava: domain-warped fbm "heat" field, animated, mapped through a
// blackbody-style colour ramp (dark crust -> red -> orange -> yellow -> white
// hot) and pushed through EMISSION so the bright cracks glow. The domain warp
// makes it swirl and flow rather than sit flat.
render_mode cull_disabled;

uniform float flow_speed = 0.06;
uniform float scale = 5.0;
uniform float emission_strength = 3.0;

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
	for (int i = 0; i < 5; i++) {
		v += amp * noise(p);
		p *= 2.0;
		amp *= 0.5;
	}
	return v;
}

// Domain-warped heat field: fbm of fbm of fbm -> swirling, flowing structure.
float heat(vec2 p, float t) {
	vec2 q = vec2(fbm(p + vec2(0.0, t)), fbm(p + vec2(5.2, 1.3 + t * 0.8)));
	vec2 r = vec2(fbm(p + 4.0 * q + vec2(1.7, 9.2)), fbm(p + 4.0 * q + vec2(8.3, 2.8)));
	return fbm(p + 4.0 * r + vec2(t * 0.2, 0.0));
}

// Blackbody-ish gradient from cool crust to white-hot.
vec3 lava_color(float h) {
	vec3 crust = vec3(0.04, 0.01, 0.0);
	vec3 deep  = vec3(0.5, 0.06, 0.0);
	vec3 mid   = vec3(1.0, 0.35, 0.03);
	vec3 hot   = vec3(1.0, 0.72, 0.18);
	vec3 white = vec3(1.0, 0.95, 0.75);
	vec3 c = mix(crust, deep, smoothstep(0.0, 0.32, h));
	c = mix(c, mid, smoothstep(0.32, 0.55, h));
	c = mix(c, hot, smoothstep(0.55, 0.78, h));
	c = mix(c, white, smoothstep(0.82, 1.0, h));
	return c;
}

void fragment() {
	vec2 uv = UV * scale;
	float t = TIME * flow_speed;
	float h = heat(uv, t);
	vec3 col = lava_color(h);
	float crackle = smoothstep(0.4, 0.68, h);   // bright veins between crust
	ALBEDO = col * 0.4;
	EMISSION = col * emission_strength * (0.15 + crackle * 1.2);
	ROUGHNESS = mix(0.85, 0.35, crackle);
	METALLIC = 0.0;
}
