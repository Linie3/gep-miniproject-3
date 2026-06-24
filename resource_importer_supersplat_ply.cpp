#include "resource_importer_supersplat_ply.h"

#include "core/io/resource_saver.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/multimesh.h"

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
}

bool ResourceImporterSupersplatPly::get_option_visibility(const String &p_path, const String &p_option,
    const HashMap<StringName, Variant> &p_options) const {
    return true;
}

Error ResourceImporterSupersplatPly::import(ResourceUID::ID p_source_id, const String &p_source_file,
    const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants,
    List<String> *r_gen_files, Variant *r_metadata) {

    Ref<MultiMesh> multiMesh;
    multiMesh.instantiate();
    multiMesh->set_transform_format(MultiMesh::TRANSFORM_3D);
    multiMesh->set_use_colors(true);
    multiMesh->set_use_custom_data(true);
    multiMesh->set_instance_count(1);

    Basis basis = Basis(Quaternion(Vector3(1, 0, 0), Math::PI * 0.25f));
    basis.scale(Vector3(2, 3, 2));

    Transform3D transform;
    transform.basis = basis;
    transform.origin = Vector3(0, 0, 0);

    multiMesh->set_instance_transform(0, transform);
    multiMesh->set_instance_color(0, Color(0.0, 0.5, 0.5, 1.0));

    multiMesh->set_instance_custom_data(0, Color(0.0, 0.0, 0.0, 0.0));

    int num_coefficients = 15;
    int bytes_per_pixel = 3 * sizeof(float);
    Vector<uint8_t> p_data;
    p_data.resize(num_coefficients * bytes_per_pixel);

    uint8_t *dst = p_data.ptrw();
    //float dummy_sh_value = 0.5f;
    // 15 Pixels of RGB data (45 floats total)

    float sh_payload[45] = {
        // L1 Coefficients (Pixels 0 to 2)
        0.2f,  0.5f, -0.1f,   // Pixel 0 (c1)
        0.2f,  0.5f, -0.1f,   // Pixel 1 (c2)
        0.2f,  0.5f, -0.1f,   // Pixel 2 (c3)

        // L2 Coefficients (Pixels 3 to 7)
       -0.1f,  0.3f,  0.4f,   // Pixel 3 (c4)
       -0.1f,  0.3f,  0.4f,   // Pixel 4 (c5)
       -0.1f,  0.3f,  0.4f,   // Pixel 5 (c6)
       -0.1f,  0.3f,  0.4f,   // Pixel 6 (c7)
       -0.1f,  0.3f,  0.4f,   // Pixel 7 (c8)

        // L3 Coefficients (Pixels 8 to 14)
        0.0f, -0.2f,  0.1f,   // Pixel 8 (c9)
        0.0f, -0.2f,  0.1f,   // Pixel 9 (c10)
        0.0f, -0.2f,  0.1f,   // Pixel 10 (c11)
        0.0f, -0.2f,  0.1f,   // Pixel 11 (c12)
        0.0f, -0.2f,  0.1f,   // Pixel 12 (c13)
        0.0f, -0.2f,  0.1f,   // Pixel 13 (c14)
        0.0f, -0.2f,  0.1f    // Pixel 14 (c15)
    };
    memcpy(dst, sh_payload, num_coefficients * bytes_per_pixel);
    /*for (int i = 0; i < num_coefficients * 3; ++i) {
        memcpy(dst + (i * sizeof(float)), &dummy_sh_value, sizeof(float));
    }*/

    Ref<Image> image = Image::create_from_data(num_coefficients, 1, false, Image::FORMAT_RGBF, p_data);

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

    vec3 world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    v_world_view_dir = CAMERA_POSITION_WORLD - world_pos;
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