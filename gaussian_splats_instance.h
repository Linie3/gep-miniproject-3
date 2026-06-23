#ifndef GAUSSIAN_SPLATS_INSTANCE_H
#define GAUSSIAN_SPLATS_INSTANCE_H

#include "scene/3d/node_3d.h"

class GaussianSplatsInstance : public Node3D {
	GDCLASS(GaussianSplatsInstance, Node3D);

protected:
	static void _bind_methods();

public:
	GaussianSplatsInstance();
};

#endif // GAUSSIAN_SPLATS_INSTANCE_H