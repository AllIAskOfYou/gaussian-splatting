/**
 * A structure for the uniforms
 */
struct Uniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
	_pad: vec3f,
	splatSize: f32,
};

struct Splat {
	variance: mat4x4f,
	position: vec4f,
	color: vec4f,
};

/*
struct Splat {
	variance: mat3x3f,
	color: vec4f,
	position: vec3f,
	constant: f32,
	_pad: vec3f,
};
*/

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
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
/*fn vs_main(@builtin(instance_index) instanceIndex: u32, vertex: VertexInput) -> VertexOutput {
	var out: VertexOutput; // create the output struct
	var s = uniforms.splatSize;
	//var s = 0.05;
	var instance = splats[sortedIndex[instanceIndex]];
	var center = uniforms.viewMatrix * 
					uniforms.modelMatrix * 
					vec4f(instance.position, 1.0);
	var z = center.z;
	var pos = vec4f(vertex.position * s, 0.0, 0.0);
	pos = pos + center;
	//center = uniforms.projectionMatrix * center;
	//pos = uniforms.projectionMatrix * pos;
	//pos.w = 0.0;
	//out.position =  pos + center;
	out.position = uniforms.projectionMatrix * pos;
	out.color = instance.color; 
    out.uv = vertex.uv;
	return out;
}*/

fn vs_main(@builtin(instance_index) instanceIndex: u32, vertex: VertexInput) -> VertexOutput {
	var s = uniforms.splatSize;
	var instance = splats[sortedIndex[instanceIndex]];
	var position = instance.position.xyz;
	//var constant = instance.position.w;
	var variance = mat3x3f(instance.variance[0].xyz, instance.variance[1].xyz, instance.variance[2].xyz);
	var vm = uniforms.viewMatrix;
	var mm = uniforms.modelMatrix;
	var wt = vm * mm;
	var w = mat3x3f(wt[0].xyz, wt[1].xyz, wt[2].xyz);
	var variance_view = w * variance * transpose(w);
	var v = mat2x2f(variance_view[0].xy, variance_view[1].xy);

	var center = wt * vec4f(position, 1.0);
	var z = center.z;
	var pos2d = transpose(v) * vertex.position;
	var pos = vec4f(pos2d*s, 0.0, 0.0);
	//pos.x *= variance[0][0];
	//pos.y *= variance[1][1];
	//pos.z *= variance[2][2];
	pos = pos + center;
	//center = uniforms.projectionMatrix * center;
	//pos = uniforms.projectionMatrix * pos;
	//pos.w = 0.0;
	//out.position =  pos + center;
	var out: VertexOutput; // create the output struct
	out.position = uniforms.projectionMatrix * pos;
	out.color = instance.color; 
    out.uv = vertex.uv;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	var uv = in.uv * 2.0 - 1.0;
	uv = uv * 4.0;
	var dist = uv.x * uv.x + uv.y * uv.y;
	dist = sqrt(dist);
	var falloff = exp(- 0.5*dist * dist);
	var color = vec4f(in.color.xyz, in.color.w * falloff);
	color = vec4f(in.color.xyz, 1);
	return color; // use the interpolated color coming from the vertex shader
}
