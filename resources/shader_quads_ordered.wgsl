struct Uniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
	splatSize: f32,
	cutOff: f32,
	fov: f32,
	bayerSize: u32,
	bayerScale: f32,
};

struct Splat {
	transform: mat4x4f,
	color: vec4f,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var<storage, read> splats: array<Splat>;
@group(0) @binding(2) var<storage, read> sortedIndex: array<u32>;

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec4f,
    @location(1) uv: vec2f,
	@location(2) instanceIndex: u32,
};

fn eigenvectors(a: mat2x2<f32>) -> mat2x2<f32> {
    let a11 = a[0][0];
    let a22 = a[1][1];
    let a1221 = a[0][1] * a[1][0];
    let det = sqrt(a11*a11 + 4 * a1221 - 2 * a11*a22 + a22*a22);
    let lam1 = 0.5 * (a11 + a22 + det);
    let lam2 = 0.5 * (a11 + a22 - det);
    let phi = 0.5 * atan2(2 * a[0][1], a11 - a22);
    return mat2x2f(
        vec2f(cos(phi), sin(phi)) * sqrt(lam1),
        vec2f(-sin(phi), cos(phi)) * sqrt(lam2)
    );
}

@vertex
fn vs_main(@builtin(instance_index) instanceIndex: u32, @location(0) quadPosition: vec2f) -> VertexOutput {
	var s = uniforms.splatSize;
	var cut_off = uniforms.cutOff;
	var vm = uniforms.viewMatrix;
	var mm = uniforms.modelMatrix;
	var wt = vm * mm;
	var wt_xyz = mat3x3f(wt[0].xyz, wt[1].xyz, wt[2].xyz);

	var instance = splats[sortedIndex[instanceIndex]];
	var sm = instance.transform;
	var s_center = wt * sm[3];
	var s_rm = mat3x3f(sm[0].xyz, sm[1].xyz, sm[2].xyz);

	var s_wt = wt_xyz * s_rm;
	var s_var_t = s_wt * transpose(s_wt);
	var s_var_t_xy = mat2x2f(s_var_t[0].xy, s_var_t[1].xy);

	var s_var_proj = eigenvectors(s_var_t_xy);

	var v_offset = vec4f(s_var_proj * quadPosition * cut_off * s, 0.0, 0.0);				// Uncomment for non-uniform splats
	//var v_offset = vec4f(vertex.position * s, 0.0, 0.0);							// Comment for non-uniform splats
	var v_pos = s_center + v_offset;

	var out: VertexOutput; // create the output struct
	out.position =  uniforms.projectionMatrix * v_pos;// + v_offset * v_pos.w;
	out.color = instance.color;
	out.uv = quadPosition * cut_off;
	out.instanceIndex = instanceIndex;
	return out;
}

fn generate_permutation(state: u32) -> vec4<u32> {
	var s = state;
	var fac_state = array<u32, 3>(0, 0, 0);
	fac_state[0] = s / 6u;
	s = s - fac_state[0] * 6u;
	fac_state[1] = s / 2u;
	s = s - fac_state[1] * 2u;
	fac_state[2] = s;

	var init_array = vec4<u32>(0, 1, 2, 3);
	for (var i = 0u; i < 3u; i = i + 1u) {
		var tmp = init_array[i];
		init_array[i] = init_array[fac_state[i] + i];
		init_array[fac_state[i] + i] = tmp;
	}
	return init_array;
}

fn generate_bayer_value(x: u32, y: u32, bayer_size: u32, init_array: vec4<u32>) -> u32 {
	var value = 0u;
	var current_size = bayer_size;
	var current_x = x;
	var current_y = y;
	var k = 1u;

	while (current_size > 1u) {
		current_size = current_size / 2u;
		var cell_x = current_x / current_size;
		var cell_y = current_y / current_size;
		current_x = current_x % current_size;
		current_y = current_y % current_size;
		var index = cell_x * 2u + cell_y;
		value = value + init_array[index] * k;
		k = k * 4u;
	}
	return value;
}

fn generate_bayer(x: u32, y: u32, bayer_size: u32, init_state: u32) -> u32 {
	var init_array = generate_permutation(init_state % 24);
	var value = generate_bayer_value(x % bayer_size, y % bayer_size, bayer_size, init_array);
	return value;
}


@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	var uv = in.uv;
	var dist = uv.x * uv.x + uv.y * uv.y;
	var falloff = exp(- 0.5*dist);
	var alpha = in.color.w * falloff;

	var BAYER_SIZE = u32(uniforms.bayerSize);
	if (BAYER_SIZE == 1) {
		return vec4f(in.color.xyz, alpha);
	}
	if (uniforms.bayerSize == 10) {
		alpha = select(0.0, 1.0, alpha > uniforms.bayerScale);
		return vec4f(in.color.xyz, alpha);
	}

	// hardcoded 4x4 bayer table
	//var bayerTable = array<u32, 16>
	//	(0, 8, 2, 10,
	//	12, 4, 14, 6,
	//	3, 11, 1, 9,
	//	15, 7, 13, 5);
	
	//var x = in.position.x;
	//var y = in.position.y;
	//var bayerValue: f32 = (x % 4.0) * 4.0 + (y % 4.0);
	//alpha = select(0.0, 1.0, alpha * (16.0 + 1.0) > (bayerValue + 1.0));

	//var x = u32(in.position.x);
	//var y = u32(in.position.y);
	//var bayerValue = ((x & 3u) << 2) + (y & 3u);
	//alpha = select(0.0, 1.0, (alpha) * f32(16 + 1) > f32(bayerValue) + 1);
	//return vec4f(in.color.xyz, alpha);
	var x = in.position.x;
	var y = in.position.y;
	var bayerValue = f32(generate_bayer(u32(x), u32(y), BAYER_SIZE, u32(in.instanceIndex)));
	if ((alpha*uniforms.bayerScale) * f32(BAYER_SIZE * BAYER_SIZE + 1) < bayerValue + 1) {
		discard;
	}
	var color = vec4f(in.color.xyz, 1.0f);
	return color;
}
