#include "resource_importer_supersplat_ply.h"

#include "core/io/file_access.h"
#include "core/io/resource_saver.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/multimesh.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

#include <string>

String ResourceImporterSupersplatPly::get_importer_name() const {
    return "supersplat_ply";
}

String ResourceImporterSupersplatPly::get_visible_name() const {
    return "SuperSplat PLY";
}

void ResourceImporterSupersplatPly::get_recognized_extensions(List<String> *p_extensions) const {
    p_extensions->push_back("ply");
}

String ResourceImporterSupersplatPly::get_save_extension() const {
    return "multimesh";
}

String ResourceImporterSupersplatPly::get_resource_type() const {
    return "MultiMesh";
}

void ResourceImporterSupersplatPly::get_import_options(const String &p_path, List<ImportOption> *r_options,
    int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "instance_count_cap"), -1));
}

bool ResourceImporterSupersplatPly::get_option_visibility(const String &p_path, const String &p_option,
    const HashMap<StringName, Variant> &p_options) const {
    return true;
}

Error ResourceImporterSupersplatPly::import(ResourceUID::ID p_source_id, const String &p_source_file,
    const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants,
    List<String> *r_gen_files, Variant *r_metadata) {

    const Ref<FileAccess> file = FileAccess::open(p_source_file, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(file.is_null(), ERR_CANT_OPEN, "Cannot open source PLY file: " + p_source_file);

    const String magic = file->get_line().strip_edges();
    ERR_FAIL_COND_V_MSG(magic != "ply", ERR_FILE_UNRECOGNIZED, "Invalid PLY file format.");

    int instance_count = 0;
    bool little_endian = false;

    while (!file->eof_reached()) {
        String line = file->get_line().strip_edges();
        if (line.begins_with("format")) {
            ERR_FAIL_COND_V_MSG(!line.contains("binary_little_endian"), ERR_FILE_UNRECOGNIZED, "PLY file does not contain little endian data.");
            little_endian = true;
        } else if (line.begins_with("element vertex")) {
            PackedStringArray parts = line.split(" ");
            ERR_FAIL_COND_V_MSG(parts.size() < 3, ERR_FILE_UNRECOGNIZED, "PLY file contains no valid vertex count line.");
            instance_count = parts[2].to_int();
        } else if (line == "end_header") {
            break;
        }
    }

    ERR_FAIL_COND_V_MSG(instance_count <= 0 || !little_endian, ERR_FILE_UNRECOGNIZED, "PLY file contains no vertices or failed to parse count or no data format could be found");

	if (const int instance_count_cap = p_options["instance_count_cap"]; instance_count_cap >= 0) {
		instance_count = std::min(instance_count, instance_count_cap);
	}

    Ref<MultiMesh> multiMesh;
    multiMesh.instantiate();
    multiMesh->set_transform_format(MultiMesh::TRANSFORM_3D);
    multiMesh->set_instance_count(0);
    multiMesh->set_use_colors(true);
    multiMesh->set_use_custom_data(true);
    multiMesh->set_instance_count(instance_count);

    constexpr int num_coefficients = 15;
	constexpr int floats_per_pixel = 3;
    constexpr int bytes_per_pixel = floats_per_pixel * sizeof(float);
    Vector<uint8_t> p_data;
    p_data.resize(instance_count * num_coefficients * bytes_per_pixel);
    uint8_t *sh_texture_dest = p_data.ptrw();

    constexpr int num_floats_per_splat = 59;
    Vector<float> splat_buf;
    splat_buf.resize(num_floats_per_splat);

	for (int i = 0; i < instance_count; ++i) {
	    file->get_buffer(reinterpret_cast<uint8_t *>(splat_buf.ptrw()), num_floats_per_splat * sizeof(float));
	    const float* raw = splat_buf.ptr();

	    Transform3D transform;
	    transform.origin = Vector3(raw[0], raw[1], raw[2]);

		const Quaternion rot_quat(raw[8], raw[9], raw[10], raw[7]);
		const Vector3 scale(std::exp(raw[11]), std::exp(raw[12]), std::exp(raw[13]));

		Basis basis(rot_quat);
	    //basis.scale(scale);
	    transform.basis = basis;

	    multiMesh->set_instance_color(i, Color(raw[3], raw[4], raw[5], 1.0f / (1.0f + std::exp(-raw[6]))));

	    multiMesh->set_instance_transform(i, transform);

	    multiMesh->set_instance_custom_data(i, Color(scale.x, scale.y, scale.z, 0.0));

	    const auto current_row_sh = reinterpret_cast<float *>(sh_texture_dest + i * num_coefficients * bytes_per_pixel);
	    memcpy(current_row_sh, &raw[14], 45 * sizeof(float));
	}

	const Ref<Image> image = Image::create_from_data(num_coefficients, instance_count, false, Image::FORMAT_RGBF, p_data);

    Ref<ImageTexture> texture;
    texture.instantiate();
    texture->set_image(image);
    material->set_shader_parameter("sh_tex", texture);

    Ref<QuadMesh> mesh;
    mesh.instantiate();
    mesh->set_size(Vector2(1.0f, 1.0f));
    mesh->set_material(material);

    multiMesh->set_mesh(mesh);

    const Error err = ResourceSaver::save(multiMesh, p_save_path + ".multimesh", 0);
    ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save supersplat multimesh to file \"" + p_save_path + ".res\".");

    return OK;
}

ResourceImporterSupersplatPly::ResourceImporterSupersplatPly() {
    material.instantiate();

    Ref<Shader> shader;
    shader.instantiate();
    shader->set_code(R"(
shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_never, blend_mix;

uniform sampler2D sh_tex; // Must be RGBF, exactly 15 pixels wide per instance row
varying vec4 v_custom;
varying vec3 v_world_view_dir; // Passed from vertex

void vertex() {
	v_custom = INSTANCE_CUSTOM;
    COLOR = COLOR;

    // 1. Get the instance center in View Space (Camera Relative)
    vec4 instance_view_pos = VIEW_MATRIX * MODEL_MATRIX * vec4(0.0, 0.0, 0.0, 1.0);

    // Pass raw world view direction for the SH color module
    vec3 instance_world_pos = (MODEL_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    v_world_view_dir = CAMERA_POSITION_WORLD - instance_world_pos;

    // 2. Extract full 3D Scale and World Rotation from the Model Matrix
    vec3 scale = INSTANCE_CUSTOM.rgb;
    mat3 R = mat3(
        normalize(MODEL_MATRIX[0].xyz),
        normalize(MODEL_MATRIX[1].xyz),
        normalize(MODEL_MATRIX[2].xyz)
    );

    // 3. Compute the 3D Covariance Matrix (Sigma = R * S * S^T * R^T)
    mat3 S = mat3(
        vec3(scale.x * scale.x, 0.0, 0.0),
        vec3(0.0, scale.y * scale.y, 0.0),
        vec3(0.0, 0.0, scale.z * scale.z)
    );
    mat3 sigma3D = R * S * transpose(R);

    // 4. Project the 3D Covariance Matrix into 2D Camera View Space
    mat3 W = mat3(VIEW_MATRIX);
    mat3 sigma2D = W * sigma3D * transpose(W);

    // 5. Extract the 2D Screen-Space Ellipse Bounds
    // Ensure safety filters to prevent infinite thinness or collapsing lines
    float cov_xx = sigma2D[0][0] + 0.05;
    float cov_xy = sigma2D[0][1];
    float cov_yy = sigma2D[1][1] + 0.05;

    // Solve eigenvalues for 2D tilt/stretch factors
    float det = cov_xx * cov_yy - cov_xy * cov_xy;
    float mid = 0.5 * (cov_xx + cov_yy);
    float lambda1 = mid + sqrt(max(0.0, mid * mid - det));
    float lambda2 = mid - sqrt(max(0.0, mid * mid - det));

    // Calculate major/minor radii of the screen ellipse footprint
    // 2.0x multiplier aligns it nicely with our QuadMesh boundaries
    float radius_x = sqrt(max(0.001, lambda1)) * 2.0;
    float radius_y = sqrt(max(0.001, lambda2)) * 2.0;

    // Compute 2D screen rotation angle
    float angle = 0.5 * atan(2.0 * cov_xy, cov_xx - cov_yy);
    mat2 rot2D = mat2(
        vec2(cos(angle), -sin(angle)),
        vec2(sin(angle),  cos(angle))
    );

    // 6. Apply deformation to our local flat quad layout
    vec2 local_deformed = rot2D * (VERTEX.xy * vec2(radius_x, radius_y));
    vec3 billboarded_vertex = vec3(local_deformed, 0.0);

    // 7. CRITICAL FIX: Bypass Godot's vertex double-multiplication trap!
    // We compute the final position in View Space and transform it directly
    // to Clip Space using PROJECTION_MATRIX, skipping local-space alterations.
    vec3 final_view_pos = instance_view_pos.xyz + billboarded_vertex;
    POSITION = PROJECTION_MATRIX * vec4(final_view_pos, 1.0);
}

vec3 eval_sh(vec3 dir, vec3 dc, int id)
{
    float x = dir.x;
    float y = dir.y;
    float z = dir.z;

    // --- Spherical Harmonics Basis Functions ---
    // L0 Basis
    float Y0 = 0.282095;

    // L1 Basis
    float Y1 = 0.488603 * y;
    float Y2 = 0.488603 * z;
    float Y3 = 0.488603 * x;

    // L2 Basis
    float Y4 = 1.092548 * x * y;
    float Y5 = 1.092548 * y * z;
    float Y6 = 0.315392 * (3.0 * z * z - 1.0);
    float Y7 = 1.092548 * x * z;
    float Y8 = 0.546274 * (x * x - y * y);

    // L3 Basis
    float Y9  = 0.590044 * y * (3.0 * x * x - y * y);
    float Y10 = 2.890611 * x * y * z;
    float Y11 = 0.457046 * y * (5.0 * z * z - 1.0);
    float Y12 = 0.373176 * z * (5.0 * z * z - 3.0);
    float Y13 = 0.457046 * x * (5.0 * z * z - 1.0);
    float Y14 = 1.445306 * (x * x - y * y) * z;
    float Y15 = 0.590044 * x * (x * x - 3.0 * y * y);

    // --- Zero-Overhead Texture Fetches ---
    // Read the 3 RGB vectors for L1 (Pixels 0-2)
    vec3 c1 = texelFetch(sh_tex, ivec2(0, id), 0).rgb;
    vec3 c2 = texelFetch(sh_tex, ivec2(1, id), 0).rgb;
    vec3 c3 = texelFetch(sh_tex, ivec2(2, id), 0).rgb;

    // Read the 5 RGB vectors for L2 (Pixels 3-7)
    vec3 c4 = texelFetch(sh_tex, ivec2(3, id), 0).rgb;
    vec3 c5 = texelFetch(sh_tex, ivec2(4, id), 0).rgb;
    vec3 c6 = texelFetch(sh_tex, ivec2(5, id), 0).rgb;
    vec3 c7 = texelFetch(sh_tex, ivec2(6, id), 0).rgb;
    vec3 c8 = texelFetch(sh_tex, ivec2(7, id), 0).rgb;

    // Read the 7 RGB vectors for L3 (Pixels 8-14)
    vec3 c9  = texelFetch(sh_tex, ivec2(8, id), 0).rgb;
    vec3 c10 = texelFetch(sh_tex, ivec2(9, id), 0).rgb;
    vec3 c11 = texelFetch(sh_tex, ivec2(10, id), 0).rgb;
    vec3 c12 = texelFetch(sh_tex, ivec2(11, id), 0).rgb;
    vec3 c13 = texelFetch(sh_tex, ivec2(12, id), 0).rgb;
    vec3 c14 = texelFetch(sh_tex, ivec2(13, id), 0).rgb;
    vec3 c15 = texelFetch(sh_tex, ivec2(14, id), 0).rgb;

    // --- Accumulate Color Contributions ---
    vec3 color =
        dc * Y0 +

        c1 * Y3 +
        c2 * Y1 +
        c3 * Y2 +

        c4 * Y4 +
        c5 * Y5 +
        c6 * Y6 +
        c7 * Y7 +
        c8 * Y8 +

        c9 * Y9 +
        c10 * Y10 +
        c11 * Y11 +
        c12 * Y12 +
        c13 * Y13 +
        c14 * Y14 +
        c15 * Y15;

    return max(color, vec3(0.0));
}

void fragment()
{
    vec3 viewDir = normalize(v_world_view_dir);

    int row_id = int(v_custom.a);
    vec3 color = eval_sh(viewDir, COLOR.rgb, row_id);

    ALBEDO = color;

    vec2 center_offset = (UV - vec2(0.5)) * 2.0;
    float d_squared = dot(center_offset, center_offset);

    float gaussian_weight = exp(-4.0 * d_squared);

    ALPHA = COLOR.a * gaussian_weight;
}
    )");

    material->set_shader(shader);
}