struct EntityHandle
{
	// @Speed: use some bits of ID as generation number.
	u32 id;
	u8 generation;
};
const u32 ENTITY_ID_INVALID = U32_MAX;
const EntityHandle ENTITY_HANDLE_INVALID = { ENTITY_ID_INVALID, 0 };

inline bool operator==(const EntityHandle &a, const EntityHandle &b)
{
	return a.id == b.id && a.generation == b.generation;
}

inline bool operator!=(const EntityHandle &a, const EntityHandle &b)
{
	return a.id != b.id && a.generation != b.generation;
}

enum ColliderType
{
	COLLIDER_CONVEX_HULL,
	COLLIDER_CUBE,
	COLLIDER_SPHERE,
	COLLIDER_CYLINDER,
	COLLIDER_CAPSULE
};

struct Collider
{
	ColliderType type;
	union
	{
		struct
		{
			const Resource *meshRes;
			f32 scale;
		} convexHull;
		struct
		{
			f32 radius;
			v3 offset;
		} cube;
		struct
		{
			f32 radius;
			v3 offset;
		} sphere;
		struct
		{
			f32 radius;
			f32 height;
			v3 offset;
		} cylinder, capsule;
	};
	EntityHandle entityHandle;
};

struct RigidBody
{
	EntityHandle entityHandle;
	v3 velocity;
	v3 angularVelocity;
	v3 totalForce;
	v3 totalTorque;
};

struct MeshInstance
{
	EntityHandle entityHandle;
	const Resource *meshRes;
};
