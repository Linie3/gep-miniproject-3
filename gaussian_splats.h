#ifndef GAUSSIAN_SPLATS_H
#define GAUSSIAN_SPLATS_H

#include "core/io/resource.h"
#include "core/string/ustring.h"

class GaussianSplats : public Resource {
	GDCLASS(GaussianSplats, Resource);

    String path;

protected:
	static void _bind_methods();

public:
	GaussianSplats();
};

#endif // GAUSSIAN_SPLATS_H