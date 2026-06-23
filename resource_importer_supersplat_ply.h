#ifndef RESOURCE_IMPORTER_SUPERSPLAT_PLY_H
#define RESOURCE_IMPORTER_SUPERSPLAT_PLY_H

#include "core/io/resource_importer.h"
#include "scene/resources/material.h"

class ResourceImporterSupersplatPly : public ResourceImporter {
	GDCLASS(ResourceImporterSupersplatPly, ResourceImporter);

	Ref<ShaderMaterial> material;

public:
	String get_importer_name() const override;

	String get_visible_name() const override;
	void get_recognized_extensions(List<String> *p_extensions) const override;
	String get_save_extension() const override;
	String get_resource_type() const override;

	void get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset = 0) const override;
	bool get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const override;

	Error import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files = nullptr, Variant *r_metadata = nullptr) override;

	bool can_import_threaded() const override { return true; }

	ResourceImporterSupersplatPly();
};

#endif //RESOURCE_IMPORTER_SUPERSPLAT_PLY_H
