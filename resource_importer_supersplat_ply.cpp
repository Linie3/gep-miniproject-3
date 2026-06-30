#include "resource_importer_supersplat_ply.h"

#include "core/io/file_access.h"
#include "core/io/resource_saver.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/multimesh.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

#include <memory>
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
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "upscale"), 1.0f));
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING_NAME, "data_format", PROPERTY_HINT_ENUM, "Standard,SuperSplat Compressed"), "Standard"));
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

    std::unique_ptr<happly::PLYData> plyIn = std::make_unique<happly::PLYData>(file->get_path_absolute().utf8().get_data());

	const String format = p_options["data_format"];

	if (format == "Standard") {

		ERR_FAIL_COND_V_MSG(plyIn->inputDataFormat != happly::DataFormat::Binary, ERR_FILE_UNRECOGNIZED, "PLY file does not contain binary little endian data.");
		ERR_FAIL_COND_V_MSG(!plyIn->hasElement("vertex"), ERR_FILE_UNRECOGNIZED, "PLY file does not contain vertices.");
	    happly::Element& vertices = plyIn->getElement("vertex");
		ERR_FAIL_COND_V_MSG(!vertices.hasProperty("x"), ERR_FILE_UNRECOGNIZED, "Vertices do not contain position information.");
		ERR_FAIL_COND_V_MSG(!vertices.hasProperty("f_dc_0"), ERR_FILE_UNRECOGNIZED, "Vertices do not contain primary color information.");
		ERR_FAIL_COND_V_MSG(!vertices.hasProperty("scale_0"), ERR_FILE_UNRECOGNIZED, "Vertices do not contain scale information.");
		ERR_FAIL_COND_V_MSG(!vertices.hasProperty("rot_0"), ERR_FILE_UNRECOGNIZED, "Vertices do not contain rotation information.");
		ERR_FAIL_COND_V_MSG(!vertices.hasProperty("f_rest_0"), ERR_FILE_UNRECOGNIZED, "Vertices do not contain spherical harmonics information.");
		ERR_FAIL_COND_V_MSG(!vertices.hasProperty("opacity"), ERR_FILE_UNRECOGNIZED, "Vertices do not contain opacity information.");

	    int instance_count = vertices.count;
		char position_begin_idx = vertices.getPropertyPtr("x")->position;
		char rotation_begin_idx = vertices.getPropertyPtr("rot_0")->position;
		char scale_begin_idx = vertices.getPropertyPtr("scale_0")->position;
		char color_begin_idx = vertices.getPropertyPtr("f_dc_0")->position;
		char opacity_idx = vertices.getPropertyPtr("opacity")->position;
		char spherical_harmonics_begin_idx = vertices.getPropertyPtr("f_rest_0")->position;

		if (const int instance_count_cap = p_options["instance_count_cap"]; instance_count_cap >= 0) {
			instance_count = std::min(instance_count, instance_count_cap);
		}

		const float upscale = p_options["upscale"];

		constexpr int floats_per_coefficient = 3;
		constexpr int floats_per_pixel = 3;
	    constexpr int pixel_per_coefficient = floats_per_coefficient / floats_per_pixel;
	    constexpr int num_coefficients = 3 + 5 + 7;
	    constexpr int pixels_per_splat_sh = num_coefficients * pixel_per_coefficient;
	    constexpr int bytes_per_pixel = floats_per_pixel * sizeof(float);

		RenderingDevice *rendering_device = RenderingServer::get_singleton()->get_rendering_device();

		const int max_pixels_per_dimension = rendering_device->limit_get(RenderingDevice::LIMIT_MAX_TEXTURE_SIZE_2D);
		const int max_coefficients_per_row = max_pixels_per_dimension / pixel_per_coefficient;
		const int max_splat_sh_per_row = max_coefficients_per_row / num_coefficients;
		const int max_splat_sh = max_pixels_per_dimension * max_splat_sh_per_row;

		if (max_splat_sh < instance_count) {
			WARN_PRINT(("Too many splats in model, discarded last " + std::to_string(instance_count - max_splat_sh) + " splats").c_str());
			instance_count = instance_count - max_splat_sh;
		}


	    Ref<MultiMesh> multi_mesh;
	    multi_mesh.instantiate();
	    multi_mesh->set_transform_format(MultiMesh::TRANSFORM_3D);
	    multi_mesh->set_instance_count(0);
	    multi_mesh->set_use_colors(true);
	    multi_mesh->set_use_custom_data(true);
	    multi_mesh->set_instance_count(instance_count);

		const int splat_sh_size_pixels = instance_count * pixels_per_splat_sh;
		const int min_pixels_per_row = std::ceil(std::sqrt(static_cast<double>(instance_count * pixels_per_splat_sh)));
		const int pixels_per_row = ((min_pixels_per_row + pixels_per_splat_sh - 1) / pixels_per_splat_sh) * pixels_per_splat_sh;
		const int pixels_per_column = std::ceil(static_cast<double>(splat_sh_size_pixels) / pixels_per_row);

	    Vector<uint8_t> p_data;
	    p_data.resize(pixels_per_row * pixels_per_column * bytes_per_pixel); // Reserve whole cube size, including empty buffer after last sh
	    uint8_t *sh_texture_dest = p_data.ptrw();

	    constexpr int num_floats_per_splat = num_coefficients * floats_per_coefficient + 14;
	    Vector<float> splat_buf;
	    splat_buf.resize(num_floats_per_splat);

		AABB total_aabb;
		bool first_aabb = true;

		String line;
		while (line != "end_header") {
			line = file->get_line();
		}

		for (int i = 0; i < instance_count; ++i) {
		    file->get_buffer(reinterpret_cast<uint8_t *>(splat_buf.ptrw()), num_floats_per_splat * sizeof(float));
		    const float* raw = splat_buf.ptr();

		    Transform3D transform;
		    transform.origin = Vector3(raw[position_begin_idx], raw[position_begin_idx + 1], raw[position_begin_idx + 2]) * upscale;

			const Quaternion rot_quat(raw[rotation_begin_idx + 1], raw[rotation_begin_idx + 2], raw[rotation_begin_idx + 3], raw[rotation_begin_idx]);

			Basis basis(rot_quat);
		    transform.basis = basis;

			Vector3 scale = Vector3(std::exp(raw[scale_begin_idx]), std::exp(raw[scale_begin_idx + 1]), std::exp(raw[scale_begin_idx + 2])) * upscale;

			if (first_aabb) {
				total_aabb = AABB(transform.origin, Vector3(0.01, 0.01, 0.01));
				first_aabb = false;
			} else {
				total_aabb.expand_to(transform.origin);
			}

		    multi_mesh->set_instance_color(i, Color(raw[color_begin_idx], raw[color_begin_idx + 1], raw[color_begin_idx + 2], 1.0f / (1.0f + std::exp(-raw[opacity_idx]))));
		    multi_mesh->set_instance_transform(i, transform);
		    multi_mesh->set_instance_custom_data(i, Color(scale.x, scale.y, scale.z, i));

		    const auto current_row_sh = reinterpret_cast<float *>(sh_texture_dest + i * num_coefficients * bytes_per_pixel);
		    memcpy(current_row_sh, &raw[spherical_harmonics_begin_idx], 45 * sizeof(float));
		}

		multi_mesh->set_custom_aabb(total_aabb);

		const Ref<Image> image = Image::create_from_data(pixels_per_row, pixels_per_column, false, Image::FORMAT_RGBF, p_data);

	    Ref<ImageTexture> texture;
	    texture.instantiate();
	    texture->set_image(image);
	    material->set_shader_parameter("sh_tex", texture);

	    Ref<QuadMesh> mesh;
	    mesh.instantiate();
	    mesh->set_size(Vector2(2.0f, 2.0f));
	    mesh->set_material(material);

	    multi_mesh->set_mesh(mesh);

	    const Error err = ResourceSaver::save(multi_mesh, p_save_path + ".multimesh", 0);
	    ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save supersplat multimesh to file \"" + p_save_path + ".res\".");

	    return OK;

	}

	return FAILED;
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

    vec4 instance_view_pos = VIEW_MATRIX * MODEL_MATRIX * vec4(0.0, 0.0, 0.0, 1.0);

    vec3 instance_world_pos = (MODEL_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    v_world_view_dir = CAMERA_POSITION_WORLD - instance_world_pos;

	// Extract Scale and Rotation
    vec3 scale = INSTANCE_CUSTOM.rgb;
    mat3 R = mat3(
        normalize(MODEL_MATRIX[0].xyz),
        normalize(MODEL_MATRIX[1].xyz),
        normalize(MODEL_MATRIX[2].xyz)
    );

    mat3 S = mat3(
        vec3(scale.x * scale.x, 0.0, 0.0),
        vec3(0.0, scale.y * scale.y, 0.0),
        vec3(0.0, 0.0, scale.z * scale.z)
    );
    mat3 sigma3D = R * S * transpose(R);

    mat3 W = mat3(VIEW_MATRIX);
    mat3 sigma2D = W * sigma3D * transpose(W);

	// Covariance
    float cov_xx = sigma2D[0][0] + 1e-6;
    float cov_xy = sigma2D[0][1];
    float cov_yy = sigma2D[1][1] + 1e-6;

    float det = cov_xx * cov_yy - cov_xy * cov_xy;
    float mid = 0.5 * (cov_xx + cov_yy);
    float lambda1 = mid + sqrt(max(0.0, mid * mid - det));
    float lambda2 = mid - sqrt(max(0.0, mid * mid - det));

    float radius_x = sqrt(max(0.001, lambda1)) * 2.0;
    float radius_y = sqrt(max(0.001, lambda2)) * 2.0;

    float angle = 0.5 * atan(2.0 * cov_xy, cov_xx - cov_yy);
    mat2 rot2D = mat2(
        vec2(cos(angle), -sin(angle)),
        vec2(sin(angle),  cos(angle))
    );

	// Billboarding
    vec2 local_deformed = rot2D * (VERTEX.xy * vec2(radius_x, radius_y));
    vec3 billboarded_vertex = vec3(local_deformed, 0.0);

    vec3 final_view_pos = instance_view_pos.xyz + billboarded_vertex;
    POSITION = PROJECTION_MATRIX * vec4(final_view_pos, 1.0);
}

vec3 eval_sh(vec3 dir, vec3 dc, int id)
{
    float x = dir.x;
    float y = dir.y;
    float z = dir.z;

    // Spherical Harmonics Basis Functions
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

    // Read sh values
	// L1
    vec3 c1 = texelFetch(sh_tex, ivec2(0, id), 0).rgb;
    vec3 c2 = texelFetch(sh_tex, ivec2(1, id), 0).rgb;
    vec3 c3 = texelFetch(sh_tex, ivec2(2, id), 0).rgb;

	// L2
    vec3 c4 = texelFetch(sh_tex, ivec2(3, id), 0).rgb;
    vec3 c5 = texelFetch(sh_tex, ivec2(4, id), 0).rgb;
    vec3 c6 = texelFetch(sh_tex, ivec2(5, id), 0).rgb;
    vec3 c7 = texelFetch(sh_tex, ivec2(6, id), 0).rgb;
    vec3 c8 = texelFetch(sh_tex, ivec2(7, id), 0).rgb;

	// L3
    vec3 c9  = texelFetch(sh_tex, ivec2(8, id), 0).rgb;
    vec3 c10 = texelFetch(sh_tex, ivec2(9, id), 0).rgb;
    vec3 c11 = texelFetch(sh_tex, ivec2(10, id), 0).rgb;
    vec3 c12 = texelFetch(sh_tex, ivec2(11, id), 0).rgb;
    vec3 c13 = texelFetch(sh_tex, ivec2(12, id), 0).rgb;
    vec3 c14 = texelFetch(sh_tex, ivec2(13, id), 0).rgb;
    vec3 c15 = texelFetch(sh_tex, ivec2(14, id), 0).rgb;

    // sh Color Contributions
    vec3 color =
        dc +

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

	if (ALPHA < 0.05) {
        discard;
    }
}
    )");

    material->set_shader(shader);
}
