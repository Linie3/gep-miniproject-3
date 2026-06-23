#include "resource_importer_supersplat_ply.h"

#include "core/io/resource_saver.h"
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
    multiMesh->set_instance_count(1);
    multiMesh->set_instance_color(0, Color(1.0, 0.5, 0.0, 1.0));

    Ref<SphereMesh> mesh;
    mesh.instantiate();
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
            render_mode unshaded, blend_mix;

            void fragment() {
                ALBEDO = COLOR.rgb;
                ALPHA = COLOR.a;
            }
    )");

    material->set_shader(shader);
}