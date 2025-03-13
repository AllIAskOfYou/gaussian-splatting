/**
 * A structure for the uniforms
 */
struct TransfromMatrix {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
};

struct Splat {
	color: vec4f,
	rotation: vec4f,
	position: vec3f,
	_pad1: f32,
	scale: vec3f,
	_pad2: f32,
};

@group(0) @binding(0) var<uniform> transform: TransfromMatrix;
@group(0) @binding(1) var<storage, read> splats: array<Splat>;
@group(0) @binding(2) var<storage, read> sortedIndex: array<u32>;

/**
 * A structure with fields labeled with vertex attribute locations can be used
 * as input to the entry point of a shader.
 */

struct VertexInput {
    @location(0) position: vec2f,
    @location(1) uv: vec2f,
};

/**
 * A structure with fields labeled with builtins and locations can also be used
 * as *output* of the vertex shader, which is also the input of the fragment
 * shader.
 */
struct VertexOutput {
	@builtin(position) position: vec4f,
	// The location here does not refer to a vertex attribute, it just means
	// that this field must be handled by the rasterizer.
	// (It can also refer to another field of another struct that would be used
	// as input to the fragment shader.)
	@location(0) color: vec4f,
    @location(1) uv: vec2f,
};

@vertex
fn vs_main(@builtin(instance_index) instanceIndex: u32, vertex: VertexInput) -> VertexOutput {
	var out: VertexOutput; // create the output struct
	var s = 0.05;

	var instance = splats[sortedIndex[instanceIndex]];
	var center = transform.viewMatrix * 
					transform.modelMatrix * 
					vec4f(instance.position, 1.0);
	var z = center.z;
	center = transform.projectionMatrix * center;
	var pos = vec4f(vertex.position * s, 0.0, 1);
	pos = transform.projectionMatrix * pos;
	pos.w = 0.0;
	out.position =  pos + center;
	out.color = instance.color; 
    out.uv = vertex.uv;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	var uv = in.uv * 2.0 - 1.0;
	uv = uv * 3.0;
	var dist = uv.x * uv.x + uv.y * uv.y;
	dist = sqrt(dist);
	var falloff = exp(- 0.5*dist * dist);
	var color = vec4f(in.color.xyz, in.color.w * falloff);
	return color; // use the interpolated color coming from the vertex shader
}
