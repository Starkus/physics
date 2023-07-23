struct LevelGeometry
{
	const Resource *renderMesh;
	const Resource *geometryGrid;
};

#if DEBUG_BUILD
struct DebugCube
{
	v3 pos;
	v3 color;
	v3 fw;
	v3 up;
	f32 scale;
};

struct DebugVertex
{
	v3 pos;
	v3 color;
};

struct DebugGeometryBuffer
{
	DeviceMesh deviceMesh;
	DeviceMesh cubeMesh;
	DeviceMesh cubePositionsBuffer;

	DebugVertex *triangleData;
	u32 triangleVertexCount;
	DebugVertex *lineData;
	u32 lineVertexCount;

	DebugCube debugCubes[2048];
	u32 debugCubeCount;
};

struct DebugContext
{
	DeviceProgram debugDrawProgram, debugCubesProgram;
	DebugGeometryBuffer debugGeometryBuffer;

	bool wireframeDebugDraws;
	bool drawAABBs;
	bool drawSupports;
	bool verboseCollisionLogging;

	bool disableDepenetration;
	bool disableFriction;

	// GJK EPA
	bool drawGJKPolytope;
	bool freezeGJKGeom;
	int gjkDrawStep;
	int gjkStepCount;
	DebugVertex *GJKSteps[64];
	int GJKStepsVertexCounts[64];
	v3 GJKNewPoint[64];

	static const int epaMaxSteps = 32;
	bool drawEPAPolytope;
	bool drawEPAClosestFeature;
	bool freezePolytopeGeom;
	int polytopeDrawStep;
	int epaStepCount;
	DebugVertex *polytopeSteps[epaMaxSteps];
	int polytopeStepsVertexCounts[epaMaxSteps];
	v3 epaNewPoint[epaMaxSteps];

	bool pauseUpdates;
	bool pausePhysics;
	bool pausePhysicsOnContact;
	bool resetMomentum;
};
#endif

#if EDITOR_PRESENT
// @Improve: this is garbage
enum PickingResult
{
	PICKING_NOTHING = 0,
	PICKING_ENTITY = 1,
	PICKING_GIZMO_ROT_X = 2,
	PICKING_GIZMO_ROT_Y = 3,
	PICKING_GIZMO_ROT_Z = 4,
	PICKING_GIZMO_X = 5,
	PICKING_GIZMO_Y = 6,
	PICKING_GIZMO_Z = 7,
};

enum EditMode
{
	EDIT_MOVE,
	EDIT_ROTATE
};

struct EditorContext
{
	EntityHandle selectedEntity = ENTITY_HANDLE_INVALID;
	EntityHandle hoveredEntity;
	EditMode currentEditMode;
	bool editRelative;

	DeviceProgram editorSelectedProgram;
	DeviceProgram editorGizmoProgram;
};
#endif

struct CollisionPair;
struct CachedHitPoint;

#define MAX_ENTITIES 4096
struct GameState
{
	f32 timeMultiplier;

	v3 camPos;
	f32 camYaw;
	f32 camPitch;

	u8 entityGenerations[MAX_ENTITIES];
	u32 entityTransforms[MAX_ENTITIES];
	u32 entityMeshes[MAX_ENTITIES];
	u32 entitySkinnedMeshes[MAX_ENTITIES];
	u32 entityParticleSystems[MAX_ENTITIES];
	u32 entityColliders[MAX_ENTITIES];
	u32 entityRigidBodies[MAX_ENTITIES];

	LevelGeometry levelGeometry;

	Array<Transform, TransientAllocator> transforms;
	Array<MeshInstance, TransientAllocator> meshInstances;
	Array<Collider, TransientAllocator> colliders;
	Array<RigidBody, TransientAllocator> rigidBodies;

	Array<Spring, TransientAllocator> springs;

	v3 lightPosition;
	v3 lightDirection;

	// @Cleanup: move to some Render Device Context or something?
	mat4 invViewMatrix, viewMatrix, projMatrix, lightSpaceMatrix;
	DeviceProgram program;

	HashMap<CollisionPair, FixedArray<CachedHitPoint, 8>, BuddyAllocator> hitPointCache;
};
