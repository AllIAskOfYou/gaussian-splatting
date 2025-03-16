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
	transform: mat4x4f,
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
    @location(0) position: vec3f,
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
	var vm = uniforms.viewMatrix;
	var mm = uniforms.modelMatrix;
	var wt = vm * mm;

	var instance = splats[sortedIndex[instanceIndex]];
	var sm = instance.transform;
	var splat_view = wt * sm;
	var splat_center = splat_view[3];
	var splat_xyz = mat3x3f(splat_view[0].xyz, splat_view[1].xyz, splat_view[2].xyz);
	var splat_xy = mat2x2f(splat_view[0].xy, splat_view[1].xy);
	var splat_xz = vec2f(splat_view[2].xy);
	var splat_yz = vec2f(splat_view[0].z, splat_view[1].z);
	var splat_zz = splat_view[2][2];
	var out_prod = mat2x2f(splat_xz * splat_yz.x, splat_xz * splat_yz.y);
	var splat_variance = splat_xy - out_prod * (1 / splat_zz);
	//var splat_variance = splat_xy;
	var v: mat3x3f;
	v[0] = vec3f(splat_variance[0], 0.0);
	v[1] = vec3f(splat_variance[1], 0.0);
	v[2] = vec3f(0, 0, 1);
	splat_view[0] = vec4f(v[0], 0.0);
	splat_view[1] = vec4f(v[1], 0.0);
	splat_view[2] = vec4f(v[2], 0.0);

	var z = splat_center.z;
	
	//var pos3d = vec3f(vertex.position, 0.0);
	//pos3d = (splat_xyz * pos3d);
	//var pos2d = vec2f(pos3d.y, pos3d.z);
	//var pos2d = splat_variance * vertex.position;
	//var local_pos = vec4f(pos2d*s, 0.0, 0.0);
	//pos.x *= variance[0][0];
	//pos.y *= variance[1][1];
	//pos.z *= variance[2][2];
	//var local_pos = vec4f(splat_xyz * vertex.position, 0.0);
	var local_pos = splat_view * vec4f(vertex.position, 1.0);
	var vertex_pos = local_pos;// + splat_center;
	//center = uniforms.projectionMatrix * center;
	//pos = uniforms.projectionMatrix * pos;
	//pos.w = 0.0;
	//out.position =  pos + center;
	var out: VertexOutput; // create the output struct
	out.position = uniforms.projectionMatrix * vertex_pos;
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
