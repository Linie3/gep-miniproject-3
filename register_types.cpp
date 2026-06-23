#include "register_types.h"

#include "resource_importer_supersplat_ply.h"
#include "core/io/resource_importer.h"

void initialize_gaussian_splats_module(const ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	Ref<ResourceImporterSupersplatPly> supersplatPlyImporter;
	supersplatPlyImporter.instantiate();
	ResourceFormatImporter::get_singleton()->add_importer(supersplatPlyImporter);
}

void uninitialize_gaussian_splats_module(const ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
   // Nothing to do here in this example.
}