#include "register_types.h"

#include "core/object/class_db.h"
#include "gaussian_splats.h"

void initialize_gaussian_splats_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_class<GaussianSplats>();
}

void uninitialize_gaussian_splats_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
   // Nothing to do here in this example.
}