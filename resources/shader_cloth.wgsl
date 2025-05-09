struct Uniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
	fov: f32,
	width: u32,
	height: u32,
};

struct Particle {
	position: vec3f,
	velocity: vec3f,
	force: vec3f,
	mass: f32,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var<storage, read> particles: array<Particle>;

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec4f,
};

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
	var mm = uniforms.modelMatrix;
	var vm = uniforms.viewMatrix;
	var pm = uniforms.projectionMatrix;

	var particle = particles[vertexIndex];
	var pos = particle.position;
	var v_pos = pm * vm * vec4f(pos, 1.0);
	//var color = vec4f(1.0, 1.0, 1.0, 1.0);
	//var z = clamp(abs(pos.y) / 4.0, 0.0, 1.0);
	//var vel = length(particle.velocity);
	//var color = vec4f(vel) * z;
	//var color = vec4f(vec3f(vel), 0.2);
	var color = vec4f(particle.velocity, 0.2);
	//var color = vec4f(0.3, 0.01, 0.025, 0.5);

	var out: VertexOutput;
	out.position =  v_pos;
	out.color = color;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	return in.color;
}
