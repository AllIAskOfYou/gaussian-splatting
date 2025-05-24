struct Uniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
	viewMatrixInverse: mat4x4f,
    modelMatrix: mat4x4f,
	ambientColor: vec4f,
	fov: f32,
	width: u32,
	height: u32,
};

struct Particle {
	position: vec3f,
	velocity: vec3f,
	force: vec3f,
	normal: vec3f,
	uv: vec2f,
	mass: f32,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var<storage, read> particles: array<Particle>;

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec4f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
	@location(3) worldPosition: vec3f,
};

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
	var mm = uniforms.modelMatrix;
	var vm = uniforms.viewMatrix;
	var pm = uniforms.projectionMatrix;

	var particle = particles[vertexIndex];
	var pos = particle.position;
	var v_pos = pm * vm * vec4f(pos, 1.0);
	var color = vec4f(0.3, 0.01, 0.025, 0.8);
	//var z = clamp(abs(pos.y) / 4.0, 0.0, 1.0);
	//var vel = length(particle.velocity);
	//var color = vec4f(vel) * z;
	//var color = vec4f(vec3f(1.0), vel);
	//var color = vec4f(particle.velocity+0.5, 0.2);
	//var color = vec4f(0.3, 0.01, 0.025, 0.5);

	var out: VertexOutput;
	out.position =  v_pos;
	out.color = color;
	out.normal = particle.normal;
	out.uv = particle.uv;
	out.worldPosition = pos;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	//var color = vec4f(in.uv.x, in.uv.y, 0.0, 1.0);

	//var color = in.color;

	var vld = uniforms.viewMatrix * normalize(vec4f(-1.0, -1.0, -1.0, 0.0));
	var vn = uniforms.viewMatrix * vec4f(in.normal, 0.0);
	var vnt = vn;
	//if (vn.z < 0.0) {
	//	vnt = -vn;
	//}
	var cameraPos = uniforms.viewMatrixInverse[3].xyz;
	var cameraDir = normalize(cameraPos - in.worldPosition.xyz);
	if (dot(in.normal, cameraDir) < 0.0) {
		vnt = -vn;
	}


	var lightColor = vec3f(1.0, 1.0, 1.0);
	var ambientColor = uniforms.ambientColor.xyz;//vec3f(0.466, 0.137, 0.0) / 2.0;
	var albedo = in.color.xyz;
	var pattern = (in.uv - 0.5) * 2.0;
	var dist2 = pattern.x * pattern.x + pattern.y * pattern.y;
	//var dist2 = abs(pattern.x) + abs(pattern.y);
	var circle = (step(0.3, dist2) + step(0.95, 1.0 - dist2));
	
	//var k1 = 0.8;
	//var k2 = 1.5;
	//var circle = 
	//	step(0.0, k1 * pattern.x + pattern.y + 1.0 - k1)
	//	+ (1.0 - step(0.0, k2 * pattern.x + pattern.y + 1.0 - k2));
	//circle = 1 - 0.5 * circle;

	albedo = circle * albedo + (1.0 - circle) * albedo * 0.2;
	var diffuse = max(dot(vnt, -vld), 0.0);
	var specular = pow(max(dot(reflect(vld, vnt), vec4f(0.0, 0.0, 1.0, 0.0)), 0.0), 8.0);
	var tr = 1 - uniforms.ambientColor.w;
	var color =
		(1-tr) * vec4f(albedo, 1.0) * vec4f(ambientColor + (diffuse + specular) * lightColor, 1.0)
		+ tr * vec4f(albedo, 1-tr);
	//var color = vec4f(albedo, 1-tr) * vec4f(ambientColor + (diffuse + specular) * lightColor, 1.0);
	return color;
}
