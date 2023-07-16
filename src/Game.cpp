#include <stdlib.h>
#include <stddef.h>
#include <memory.h>
#include <string.h>
#include <stdarg.h>

#if TARGET_WINDOWS && DEBUG_BUILD
#define USING_IMGUI 1
#endif

#if USING_IMGUI && DEBUG_BUILD
#define EDITOR_PRESENT 1
#else
#define EDITOR_PRESENT 0
#endif

#define IMGUI_SHOW_DEMO

#if USING_IMGUI
#include <imgui/imgui.h>
#ifdef IMGUI_SHOW_DEMO
#include <imgui/imgui_demo.cpp> // @Todo: remove
#endif
#endif

#include "RandomTable.h"
#include "StringStream.h"
#include "Entity.h"
#include "Game.h"

#if DEBUG_BUILD
DebugContext *g_debugContext;
#endif

#include "DebugDraw.cpp"
#include "Collision.cpp"
#include "BakeryInterop.cpp"
#include "Resource.cpp"
#include "Entity.cpp"
#include "Parsing.cpp"

#if TARGET_WINDOWS
#define GAMEDLL NOMANGLE __declspec(dllexport)
#else
#define GAMEDLL NOMANGLE __attribute__((visibility("default")))
#endif

#ifdef USING_IMGUI
#include "Imgui.cpp"
#endif

bool GameResourcePostLoad(Resource *resource, u8 *fileBuffer, bool initialize)
{
	switch(resource->type)
	{
	case RESOURCETYPE_MESH:
	{
		ResourceLoadMesh(resource, fileBuffer, initialize);
		return true;
	} break;
	case RESOURCETYPE_SKINNEDMESH:
	{
		ResourceLoadSkinnedMesh(resource, fileBuffer, initialize);
		return true;
	} break;
	case RESOURCETYPE_LEVELGEOMETRYGRID:
	{
		ResourceLoadLevelGeometryGrid(resource, fileBuffer, initialize);
		return true;
	} break;
	case RESOURCETYPE_COLLISIONMESH:
	{
		ResourceLoadCollisionMesh(resource, fileBuffer, initialize);
		return true;
	} break;
	case RESOURCETYPE_TEXTURE:
	{
		ResourceLoadTexture(resource, fileBuffer, initialize);
		return true;
	} break;
	case RESOURCETYPE_SHADER:
	{
		ResourceLoadShader(resource, fileBuffer, initialize);
		return true;
	} break;
	case RESOURCETYPE_MATERIAL:
	{
		ResourceLoadMaterial(resource, fileBuffer, initialize);
		return true;
	} break;
	}
	return false;
}

void UpdateViewProjMatrices(GameState *gameState)
{
	// Projection matrix
	mat4 *proj = &gameState->projMatrix;
	{
		const f32 fov = HALFPI;
		const f32 near = 0.01f;
		const f32 far = 2000.0f;
		const f32 aspectRatio = (16.0f / 9.0f);

		const f32 top = Tan(HALFPI - fov / 2.0f);
		const f32 right = top / aspectRatio;

		*proj =
		{
			right, 0.0f, 0.0f, 0.0f,
			0.0f, top, 0.0f, 0.0f,
			0.0f, 0.0f, -(far + near) / (far - near), -1.0f,
			0.0f, 0.0f, -(2.0f * far * near) / (far - near), 0.0f
		};
	}

	// View matrix
	mat4 *view = &gameState->viewMatrix;
	{
		f32 yawSin = Sin(gameState->camYaw);
		f32 yawCos = Cos(gameState->camYaw);
		f32 pitchSin = Sin(gameState->camPitch);
		f32 pitchCos = Cos(gameState->camPitch);
		// NOTE: 0 yaw means looking towards +Y!
		v3 camFw = { yawSin * pitchSin, yawCos * pitchSin, pitchCos };

		const v3 right = V3Normalize(V3Cross(camFw, v3{0,0,1}));
		const v3 up = V3Cross(right, camFw);
		const v3 pos = gameState->camPos;
		gameState->invViewMatrix =
		{
			right.x,	right.y,	right.z,	0.0f,
			up.x,		up.y,		up.z,		0.0f,
			-camFw.x,	-camFw.y,	-camFw.z,	0.0f,
			pos.x,		pos.y,		pos.z,		1.0f
		};

		*view = Mat4Inverse(gameState->invViewMatrix);
	}
}

void StartGame()
{
	ASSERT(g_memory->transientMem == g_memory->transientPtr);
	GameState *gameState = ALLOC(TransientAllocator, GameState);
#if DEBUG_BUILD
	g_debugContext = ALLOC(TransientAllocator, DebugContext);
	*g_debugContext = {};
#endif

	// Init game state
	memset(gameState, 0, sizeof(GameState));
	gameState->timeMultiplier = 1.0f;
	ArrayInit(&gameState->transforms, 4096);
	ArrayInit(&gameState->meshInstances, 4096);
	ArrayInit(&gameState->colliders, 4096);
	ArrayInit(&gameState->rigidBodies, 4096);

	// @Hack: Hmmm
	memset(gameState->entityTransforms, 0xFF, sizeof(gameState->entityTransforms));
	memset(gameState->entityMeshes, 0xFF, sizeof(gameState->entityMeshes));
	memset(gameState->entityColliders, 0xFF, sizeof(gameState->entityColliders));
	memset(gameState->entityRigidBodies, 0xFF, sizeof(gameState->entityRigidBodies));

	// Initialize
	{
		SetUpDevice();

		LoadResource(RESOURCETYPE_MESH, "anvil.b");
		LoadResource(RESOURCETYPE_MESH, "teapot.b");
		LoadResource(RESOURCETYPE_MESH, "cube.b");
		LoadResource(RESOURCETYPE_MESH, "sphere.b");
		LoadResource(RESOURCETYPE_MESH, "cylinder.b");
		LoadResource(RESOURCETYPE_MESH, "capsule.b");
		LoadResource(RESOURCETYPE_MESH, "level_graphics.b");
		LoadResource(RESOURCETYPE_LEVELGEOMETRYGRID, "level.b");
		LoadResource(RESOURCETYPE_COLLISIONMESH, "anvil_collision.b");
		LoadResource(RESOURCETYPE_COLLISIONMESH, "teapot_collision.b");

		LoadResource(RESOURCETYPE_MATERIAL, "material_default.b");

#if EDITOR_PRESENT
		LoadResource(RESOURCETYPE_MESH, "editor_arrow.b");
		LoadResource(RESOURCETYPE_COLLISIONMESH, "editor_arrow_collision.b");
		LoadResource(RESOURCETYPE_MESH, "editor_circle.b");
		LoadResource(RESOURCETYPE_COLLISIONMESH, "editor_circle_collision.b");
#endif

#if DEBUG_BUILD
		// Debug geometry buffer
		{
			int attribs = RENDERATTRIB_POSITION | RENDERATTRIB_COLOR3;
			DebugGeometryBuffer *dgb = &g_debugContext->debugGeometryBuffer;
			dgb->triangleData = ALLOC_N(TransientAllocator, DebugVertex, 2048);
			dgb->lineData = ALLOC_N(TransientAllocator, DebugVertex, 2048);
			dgb->debugCubeCount = 0;
			dgb->triangleVertexCount = 0;
			dgb->lineVertexCount = 0;
			dgb->deviceMesh = CreateDeviceMesh(attribs);
			dgb->cubePositionsBuffer = CreateDeviceMesh(attribs);
		}

		// Send the cube mesh that gets instanced
		{
			u8 *fileBuffer;
			u64 fileSize;
			bool success = PlatformReadEntireFile("data/cube.b", &fileBuffer,
					&fileSize, FrameAllocator::Alloc);
			ASSERT(success);

			Vertex *vertexData;
			u16 *indexData;
			u32 vertexCount;
			u32 indexCount;
			const char *materialName;
			ReadMesh(fileBuffer, &vertexData, &indexData, &vertexCount, &indexCount, &materialName);

			// Keep only positions
			v3 *positionBuffer = ALLOC_N(FrameAllocator, v3, vertexCount);
			for (u32 i = 0; i < vertexCount; ++i)
			{
				positionBuffer[i] = vertexData[i].pos;
			}

			int attribs = RENDERATTRIB_POSITION;
			g_debugContext->debugGeometryBuffer.cubeMesh = CreateDeviceIndexedMesh(attribs);
			SendIndexedMesh(&g_debugContext->debugGeometryBuffer.cubeMesh, positionBuffer,
					vertexCount, sizeof(v3), indexData, indexCount, false);
		}
#endif

		// Shaders
		const Resource *shaderRes = LoadResource(RESOURCETYPE_SHADER, "shaders/shader_general.b");
		gameState->program = shaderRes->shader.programHandle;

#if DEBUG_BUILD
		const Resource *shaderDebugRes = LoadResource(RESOURCETYPE_SHADER, "shaders/shader_debug.b");
		g_debugContext->debugDrawProgram = shaderDebugRes->shader.programHandle;

		const Resource *shaderDebugCubesRes = LoadResource(RESOURCETYPE_SHADER, "shaders/shader_debug_cubes.b");
		g_debugContext->debugCubesProgram = shaderDebugCubesRes->shader.programHandle;
#endif

#if EDITOR_PRESENT
		const Resource *shaderEditorSelectedRes = LoadResource(RESOURCETYPE_SHADER, "shaders/shader_editor_selected.b");
		g_debugContext->editorSelectedProgram = shaderEditorSelectedRes->shader.programHandle;

		const Resource *shaderEditorGizmoRes = LoadResource(RESOURCETYPE_SHADER, "shaders/shader_editor_gizmo.b");
		g_debugContext->editorGizmoProgram = shaderEditorGizmoRes->shader.programHandle;
#endif
	}

	// Init level
	{
		const Resource *levelGraphicsRes = GetResource("level_graphics.b");
		gameState->levelGeometry.renderMesh = levelGraphicsRes;

		const Resource *levelCollisionRes = GetResource("level.b");
		gameState->levelGeometry.geometryGrid = levelCollisionRes;
	}

	// Init camera
	{
		gameState->camPos = { 0, -3, 3};
		gameState->camPitch = PI * 0.6f;

		UpdateViewProjMatrices(gameState);
	}

	// Test entities
	{
		const Resource *anvilRes = GetResource("anvil.b");
		MeshInstance anvilMesh;
		anvilMesh.meshRes = anvilRes;

		Collider anvilCollider;
		anvilCollider.type = COLLIDER_CONVEX_HULL;
		const Resource *anvilCollRes = GetResource("anvil_collision.b");
		anvilCollider.convexHull.meshRes = anvilCollRes;
		anvilCollider.convexHull.scale = 1.0f;

		const Resource *teapotRes = GetResource("teapot.b");
		MeshInstance teapotMesh;
		teapotMesh.meshRes = teapotRes;

		Collider teapotCollider;
		teapotCollider.type = COLLIDER_CONVEX_HULL;
		const Resource *teapotCollRes = GetResource("teapot_collision.b");
		teapotCollider.convexHull.meshRes = teapotCollRes;
		teapotCollider.convexHull.scale = 1.0f;

		Transform *transform;
		EntityHandle testEntityHandle = AddEntity(gameState, &transform);
		transform->translation = { -6.0f, 3.0f, 1.0f };
		transform->rotation = QUATERNION_IDENTITY;
		MeshInstance *meshInstance = ArrayAdd(&gameState->meshInstances);
		*meshInstance = anvilMesh;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		Collider *collider = ArrayAdd(&gameState->colliders);
		*collider = anvilCollider;
		EntityAssignCollider(gameState, testEntityHandle, collider);

		testEntityHandle = AddEntity(gameState, &transform);
		transform->translation = { 5.0f, 4.0f, 1.0f };
		transform->rotation = QUATERNION_IDENTITY;
		meshInstance = ArrayAdd(&gameState->meshInstances);
		*meshInstance = anvilMesh;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		collider = ArrayAdd(&gameState->colliders);
		*collider = anvilCollider;
		EntityAssignCollider(gameState, testEntityHandle, collider);

		testEntityHandle = AddEntity(gameState, &transform);
		transform->translation = { 3.0f, -4.0f, 1.0f };
		transform->rotation = QuaternionFromEulerZYX(v3{ 0, 0, HALFPI * -0.5f });
		meshInstance = ArrayAdd(&gameState->meshInstances);
		*meshInstance = anvilMesh;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		collider = ArrayAdd(&gameState->colliders);
		*collider = anvilCollider;
		EntityAssignCollider(gameState, testEntityHandle, collider);

		testEntityHandle = AddEntity(gameState, &transform);
		transform->translation = { -8.0f, -4.0f, 1.0f };
		transform->rotation = QuaternionFromEulerZYX(v3{ HALFPI * 0.5f, 0, 0 });
		meshInstance = ArrayAdd(&gameState->meshInstances);
		*meshInstance = teapotMesh;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		collider = ArrayAdd(&gameState->colliders);
		*collider = teapotCollider;
		EntityAssignCollider(gameState, testEntityHandle, collider);

		const Resource *cubeRes = GetResource("cube.b");
		testEntityHandle = AddEntity(gameState, &transform);
		transform->translation = { -3.0f, 5.0f, 1.0f };
		transform->rotation = QUATERNION_IDENTITY;
		meshInstance = ArrayAdd(&gameState->meshInstances);
		meshInstance->meshRes = cubeRes;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		collider = ArrayAdd(&gameState->colliders);
		collider->type = COLLIDER_CUBE;
		collider->cube.radius = 1;
		collider->cube.offset = {};
		EntityAssignCollider(gameState, testEntityHandle, collider);

		testEntityHandle = AddEntity(gameState, &transform);
		transform->translation = { 0.0f, 5.0f, 1.0f };
		transform->rotation = QuaternionFromEulerZYX(v3{ 0, HALFPI, 0 });
		meshInstance = ArrayAdd(&gameState->meshInstances);
		meshInstance->meshRes = cubeRes;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		collider = ArrayAdd(&gameState->colliders);
		collider->type = COLLIDER_CUBE;
		collider->cube.radius = 1;
		collider->cube.offset = {};
		EntityAssignCollider(gameState, testEntityHandle, collider);

		testEntityHandle = AddEntity(gameState, &transform);
		transform->translation = { 3.0f, 5.0f, 1.0f };
		transform->rotation = QuaternionFromEulerZYX(v3{ HALFPI, 0, 0 });
		meshInstance = ArrayAdd(&gameState->meshInstances);
		meshInstance->meshRes = cubeRes;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		collider = ArrayAdd(&gameState->colliders);
		collider->type = COLLIDER_CUBE;
		collider->cube.radius = 1;
		collider->cube.offset = {};
		EntityAssignCollider(gameState, testEntityHandle, collider);

		testEntityHandle = AddEntity(gameState, &transform);
		transform->translation = { 0.0f, 0.0f, -20.0f };
		transform->rotation = QuaternionFromEulerZYX(v3{ HALFPI, 0, 0 });
		meshInstance = ArrayAdd(&gameState->meshInstances);
		meshInstance->meshRes = cubeRes;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		collider = ArrayAdd(&gameState->colliders);
		collider->type = COLLIDER_CUBE;
		collider->cube.radius = 20;
		collider->cube.offset = {};
		EntityAssignCollider(gameState, testEntityHandle, collider);

		testEntityHandle = AddEntity(gameState, &transform);
		transform->translation = { 3.0f, 5.0f, 4.0f };
		transform->rotation = QuaternionFromEulerZYX(v3{ HALFPI, 0, 0 });
		meshInstance = ArrayAdd(&gameState->meshInstances);
		meshInstance->meshRes = cubeRes;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		collider = ArrayAdd(&gameState->colliders);
		collider->type = COLLIDER_CUBE;
		collider->cube.radius = 1;
		collider->cube.offset = {};
		EntityAssignCollider(gameState, testEntityHandle, collider);
		RigidBody *rigidBody = ArrayAdd(&gameState->rigidBodies);
		*rigidBody = {};
		rigidBody->invMass = 1.0f;
		rigidBody->restitution = 0.3f;
		rigidBody->staticFriction = 0.4f;
		rigidBody->dynamicFriction = 0.2f;
		{
			// Moment of inertia of 2m^3, 1kg cube
			f32 s = ((1.0f/rigidBody->invMass)/6.0f) * (4 * collider->cube.radius * collider->cube.radius);
			f32 invs = 1.0f/s;
			rigidBody->invMomentOfInertiaTensor = {
				invs,	0,		0,		0,
				0,		invs,	0,		0,
				0,		0,		invs,	0,
				0,		0,		0,		0
			};
		}
		EntityAssignRigidBody(gameState, testEntityHandle, rigidBody);

		const Resource *sphereRes = GetResource("sphere.b");
		testEntityHandle = AddEntity(gameState, &transform);
		transform->translation = { -6.0f, 7.0f, 1.0f };
		transform->rotation = QUATERNION_IDENTITY;
		meshInstance = ArrayAdd(&gameState->meshInstances);
		meshInstance->meshRes = sphereRes;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		collider = ArrayAdd(&gameState->colliders);
		collider->type = COLLIDER_SPHERE;
		collider->sphere.radius = 1;
		collider->sphere.offset = {};
		EntityAssignCollider(gameState, testEntityHandle, collider);

		const Resource *cylinderRes = GetResource("cylinder.b");
		testEntityHandle = AddEntity(gameState, &transform);
		transform->translation = { -3.0f, 7.0f, 1.0f };
		transform->rotation = QUATERNION_IDENTITY;
		meshInstance = ArrayAdd(&gameState->meshInstances);
		meshInstance->meshRes = cylinderRes;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		collider = ArrayAdd(&gameState->colliders);
		collider->type = COLLIDER_CYLINDER;
		collider->cylinder.radius = 1;
		collider->cylinder.height = 2;
		collider->cylinder.offset = {};
		EntityAssignCollider(gameState, testEntityHandle, collider);

		testEntityHandle = AddEntity(gameState, &transform);
		transform->translation = { -3.0f, 5.0f, 4.0f };
		transform->rotation = QUATERNION_IDENTITY;
		meshInstance = ArrayAdd(&gameState->meshInstances);
		meshInstance->meshRes = cylinderRes;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		collider = ArrayAdd(&gameState->colliders);
		collider->type = COLLIDER_CYLINDER;
		collider->cylinder.radius = 1;
		collider->cylinder.height = 2;
		collider->cylinder.offset = {};
		EntityAssignCollider(gameState, testEntityHandle, collider);
		rigidBody = ArrayAdd(&gameState->rigidBodies);
		*rigidBody = {};
		rigidBody->invMass = 1.0f;
		rigidBody->restitution = 0.3f;
		rigidBody->staticFriction = 0.4f;
		rigidBody->dynamicFriction = 0.2f;
		{
			f32 mass = 1.0f/rigidBody->invMass;
			f32 s = (mass/12.0f) * (collider->cylinder.height * collider->cylinder.height) +
					(mass/4.0f)  * (collider->cylinder.radius * collider->cylinder.radius);
			f32 invs = 1.0f/s;
			f32 z = (mass/2.0f)  * (collider->cylinder.radius * collider->cylinder.radius);
			f32 invz = 1.0f/z;
			rigidBody->invMomentOfInertiaTensor = {
				invs,	0,		0,		0,
				0,		invs,	0,		0,
				0,		0,		invz,	0,
				0,		0,		0,		0
			};
		}
		EntityAssignRigidBody(gameState, testEntityHandle, rigidBody);

		const Resource *capsuleRes = GetResource("capsule.b");
		testEntityHandle = AddEntity(gameState, &transform);
		transform->translation = { 0.0f, 7.0f, 2.0f };
		transform->rotation = QUATERNION_IDENTITY;
		meshInstance = ArrayAdd(&gameState->meshInstances);
		meshInstance->meshRes = capsuleRes;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		collider = ArrayAdd(&gameState->colliders);
		collider->type = COLLIDER_CAPSULE;
		collider->capsule.radius = 1;
		collider->capsule.height = 2;
		collider->capsule.offset = {};
		EntityAssignCollider(gameState, testEntityHandle, collider);

		const Resource *arrowRes = GetResource("editor_arrow.b");
		for (int i = 0; i < ArrayCount(g_debugContext->debugArrows); ++i)
		{
			g_debugContext->debugArrows[i] = AddEntity(gameState, &transform);
			transform->translation = {};
			transform->rotation = QUATERNION_IDENTITY;
			meshInstance = ArrayAdd(&gameState->meshInstances);
			meshInstance->meshRes = arrowRes;
			EntityAssignMesh(gameState, g_debugContext->debugArrows[i], meshInstance);
		}
	}

	// Init light
	{
		gameState->lightPosition = { 2, 2, 3 };
		gameState->lightDirection = V3Normalize({ 0.2f, 0.4f, -0.7f });
	}
}

DeviceProgram BindMaterial(GameState *gameState, const Resource *materialRes, const mat4 *modelMatrix)
{
	const Resource *shaderRes = materialRes->material.shaderRes;

	DeviceProgram programHandle = shaderRes->shader.programHandle;
	UseProgram(programHandle);
	DeviceUniform viewUniform = GetUniform(programHandle, "view");
	DeviceUniform projUniform = GetUniform(programHandle, "projection");
	DeviceUniform modelUniform = GetUniform(programHandle, "model");

	DeviceUniform lightSpaceUniform = GetUniform(programHandle, "lightSpaceMatrix");
	DeviceUniform lightDirectionUniform = GetUniform(programHandle, "lightDirection");
	DeviceUniform shadowMapUniform = GetUniform(programHandle, "shadowMap");

	DeviceUniform albedoUniform = GetUniform(programHandle, "texAlbedo");
	DeviceUniform normalUniform = GetUniform(programHandle, "texNormal");

	UniformMat4Array(viewUniform, 1, gameState->viewMatrix.m);
	UniformMat4Array(projUniform, 1, gameState->projMatrix.m);
	UniformMat4Array(modelUniform, 1, modelMatrix->m);

	UniformMat4Array(lightSpaceUniform, 1, gameState->lightSpaceMatrix.m);
	UniformV3(lightDirectionUniform, gameState->lightDirection);
	const s32 shadowMapSlot = materialRes->material.textureCount;
	UniformInt(shadowMapUniform, shadowMapSlot);

	UniformInt(albedoUniform, 0);
	UniformInt(normalUniform, 1);

	for (int i = 0; i < materialRes->material.textureCount; ++i)
	{
		const Resource *tex = materialRes->material.textures[i];
		BindTexture(tex->texture.deviceTexture, i);
	}

	return programHandle;
}

// @Cleanup: we shouldn't need delta time here probably?
void Render(GameState *gameState, f32 deltaTime)
{
	// Meshes
	for (u32 meshInstanceIdx = 0; meshInstanceIdx < gameState->meshInstances.count;
			++meshInstanceIdx)
	{
		MeshInstance *meshInstance = &gameState->meshInstances[meshInstanceIdx];
		const Resource *meshRes = meshInstance->meshRes;
#if EDITOR_PRESENT
		// While editing this resource can be null. Don't check otherwise, to avoid an
		// unnecessary branch.
		if (meshRes)
#endif
		{
			Transform *transform = GetEntityTransform(gameState, meshInstance->entityHandle);

			const mat4 model = Mat4Compose(transform->translation, transform->rotation);

			// @Improve: don't rebind program, textures and all for every mesh! Maybe sort by
			// material.
			const Resource *materialRes = meshRes->mesh.materialRes;
#if DEBUG_BUILD
			if (!materialRes)
			{
				materialRes = GetResource("material_default.b");
			}
#endif

			BindMaterial(gameState, materialRes, &model);

			RenderIndexedMesh(meshRes->mesh.deviceMesh);
		}
	}

	// Level
	if (0)
	{
		LevelGeometry *level = &gameState->levelGeometry;

		const Resource *materialRes = level->renderMesh->mesh.materialRes;
#if DEBUG_BUILD
		if (!materialRes)
		{
			materialRes = GetResource("material_default.b");
		}
#endif
		BindMaterial(gameState, materialRes, &MAT4_IDENTITY);

		RenderIndexedMesh(level->renderMesh->mesh.deviceMesh);
	}

#if DEBUG_BUILD
	// Debug meshes
	{
		DebugGeometryBuffer *dgb = &g_debugContext->debugGeometryBuffer;
		if (g_debugContext->wireframeDebugDraws)
			SetFillMode(RENDER_LINE);

		SetBackfaceCullingEnabled(false);

		//DisableDepthTest();
		ClearDepthBuffer();

		UseProgram(g_debugContext->debugDrawProgram);
		DeviceUniform viewUniform = GetUniform(g_debugContext->debugDrawProgram, "view");
		DeviceUniform projUniform = GetUniform(g_debugContext->debugDrawProgram, "projection");
		UniformMat4Array(projUniform, 1, gameState->projMatrix.m);
		UniformMat4Array(viewUniform, 1, gameState->viewMatrix.m);

		SendMesh(&dgb->deviceMesh,
				dgb->triangleData,
				dgb->triangleVertexCount, sizeof(DebugVertex), true);

		RenderMesh(dgb->deviceMesh);

		SendMesh(&dgb->deviceMesh,
				dgb->lineData,
				dgb->lineVertexCount, sizeof(DebugVertex), true);
		RenderLines(dgb->deviceMesh);

		if (dgb->debugCubeCount)
		{
			UseProgram(g_debugContext->debugCubesProgram);
			viewUniform = GetUniform(g_debugContext->debugDrawProgram, "view");
			projUniform = GetUniform(g_debugContext->debugDrawProgram, "projection");
			UniformMat4Array(projUniform, 1, gameState->projMatrix.m);
			UniformMat4Array(viewUniform, 1, gameState->viewMatrix.m);

			SendMesh(&dgb->cubePositionsBuffer,
					dgb->debugCubes,
					dgb->debugCubeCount, sizeof(DebugCube), true);

			u32 meshAttribs = RENDERATTRIB_POSITION;
			u32 instAttribs = RENDERATTRIB_POSITION | RENDERATTRIB_COLOR3 |
				RENDERATTRIB_1CUSTOMV3 | RENDERATTRIB_2CUSTOMV3 | RENDERATTRIB_1CUSTOMF32;
			RenderIndexedMeshInstanced(dgb->cubeMesh, dgb->cubePositionsBuffer,
					meshAttribs, instAttribs);
		}

		dgb->debugCubeCount = 0;
		dgb->triangleVertexCount = 0;
		dgb->lineVertexCount = 0;

		//EnableDepthTest();
		SetBackfaceCullingEnabled(true);
		SetFillMode(RENDER_FILL);
	}
#endif

#if EDITOR_PRESENT
	// Selected entity
	if (Transform *selectedEntity = GetEntityTransform(gameState, g_debugContext->selectedEntity))
	{
		SetFillMode(RENDER_LINE);

		UseProgram(g_debugContext->editorSelectedProgram);
		DeviceUniform viewUniform = GetUniform(g_debugContext->editorSelectedProgram, "view");
		UniformMat4Array(viewUniform, 1, gameState->viewMatrix.m);
		DeviceUniform projUniform = GetUniform(g_debugContext->editorSelectedProgram, "projection");
		UniformMat4Array(projUniform, 1, gameState->projMatrix.m);
		DeviceUniform modelUniform = GetUniform(g_debugContext->editorSelectedProgram, "model");

		static f32 t = 0;
		t += deltaTime;
		DeviceUniform timeUniform = GetUniform(g_debugContext->editorSelectedProgram, "time");
		UniformFloat(timeUniform, t);

		const mat4 model = Mat4Compose(selectedEntity->translation, selectedEntity->rotation);
		UniformMat4Array(modelUniform, 1, model.m);

		MeshInstance *meshInstance = GetEntityMesh(gameState, g_debugContext->selectedEntity);
		if (meshInstance)
		{
			const Resource *meshRes = meshInstance->meshRes;
			if (meshRes)
				RenderIndexedMesh(meshInstance->meshRes->mesh.deviceMesh);
		}

		SetFillMode(RENDER_FILL);
	}
#endif
}

void UpdateAndRenderGame(Controller *controller, f32 deltaTime)
{
	GameState *gameState = (GameState *)g_memory->transientMem;

	deltaTime *= gameState->timeMultiplier;

#ifdef USING_IMGUI

#ifdef IMGUI_SHOW_DEMO
	ImGui::ShowDemoWindow();
#endif

	ImguiShowDebugWindow(gameState);
#endif

	if (deltaTime < 0 || deltaTime > 1)
	{
		Log("WARNING: Delta time out of range! %f\n", deltaTime);
		deltaTime = 1 / 60.0f;
	}

#if 0
	f32 mass = 1.0f;
	f32 restitution = 0.3f;
	// @Hardcoded: moment of inertia of 1m^3, 1kg cube
	f32 s = (mass/12.0f) * (4.0f*4.0f);
	f32 invs = 1.0f/s;
	mat4 invMomentOfInertia =
	{
		invs,	0,		0,		0,
		0,		invs,	0,		0,
		0,		0,		invs,	0,
		0,		0,		0,		0
	};
	mat4 momentOfInertia =
	{
		s,	0,	0,	0,
		0,	s,	0,	0,
		0,	0,	s,	0,
		0,	0,	0,	0
	};
#endif

	// Update
	{
		// Collision
		struct Collision
		{
			v3 hitPoint;
			v3 hitNormal;
			f32 penetrationLength;
			f32 normalImpulseMag;
			EntityHandle entityA;
			EntityHandle entityB;
		};
		DynamicArray<Collision, FrameAllocator> collisions;
		DynamicArrayInit(&collisions, 32);
		for (u32 colliderAIdx = 0; colliderAIdx < gameState->colliders.count; ++colliderAIdx)
		{
			Collider *colliderA = &gameState->colliders[colliderAIdx];
			Transform *transformA = GetEntityTransform(gameState, colliderA->entityHandle);
			ASSERT(transformA); // There should never be an orphaned collider.

			for (u32 colliderBIdx = colliderAIdx + 1; colliderBIdx < gameState->colliders.count; ++colliderBIdx)
			{
				Collider *colliderB = &gameState->colliders[colliderBIdx];
				Transform *transformB = GetEntityTransform(gameState, colliderB->entityHandle);
				ASSERT(transformB); // There should never be an orphaned collider.

				// @Improve: better collision against multiple colliders?
				CollisionInfo collisionInfo = TestCollision(transformA, transformB, colliderA,
						colliderB);
				if (collisionInfo.hit)
				{
					*DynamicArrayAdd(&collisions) = {
						.hitPoint = collisionInfo.hitPoint,
						.hitNormal = collisionInfo.collisionNormal,
						.penetrationLength = collisionInfo.penetrationLength,
						.entityA = colliderA->entityHandle,
						.entityB = colliderB->entityHandle
					};

#if DEBUG_BUILD
					if (g_debugContext->pausePhysicsOnContact)
						g_debugContext->pausePhysics = true;
#endif
				}
			}
		}
		// Bounce impulse
		for (u32 collisionIdx = 0; collisionIdx < collisions.count; ++collisionIdx)
		{
			Collision* collision = &collisions[collisionIdx];
			//if (collision->penetrationLength <= 0.0f)
				//continue;

			RigidBody *rigidBodyA = GetEntityRigidBody(gameState, collision->entityA);
			RigidBody *rigidBodyB = GetEntityRigidBody(gameState, collision->entityB);
			bool hasRigidBodyA = rigidBodyA != nullptr;
			bool hasRigidBodyB = rigidBodyB != nullptr;

			if (!hasRigidBodyA && !hasRigidBodyB)
				continue;

			if (!rigidBodyA) rigidBodyA = &RIGID_BODY_STATIC;
			if (!rigidBodyB) rigidBodyB = &RIGID_BODY_STATIC;

			Transform *transformA = GetEntityTransform(gameState, collision->entityA);
			Transform *transformB = GetEntityTransform(gameState, collision->entityB);

			v3 hitNormal = collision->hitNormal;
			v3 localHitPointA = collision->hitPoint - transformA->translation;
			v3 localHitPointB = collision->hitPoint - transformB->translation;

			v3 angVelAtHitPointA = V3Cross(rigidBodyA->angularVelocity, localHitPointA);
			v3 velAtHitPointA = rigidBodyA->velocity + angVelAtHitPointA;

			v3 angVelAtHitPointB = V3Cross(rigidBodyB->angularVelocity, localHitPointB);
			v3 velAtHitPointB = rigidBodyB->velocity + angVelAtHitPointB;

			v3 relVelAtHitPoint = velAtHitPointA - velAtHitPointB;

			f32 speedAlongHitNormal = V3Dot(relVelAtHitPoint, hitNormal);
			if (speedAlongHitNormal > 0)
				continue;
			v3 impulseVector = hitNormal * speedAlongHitNormal;

			f32 restitution = Min(rigidBodyA->restitution, rigidBodyB->restitution);

			// Impulse scalar
			f32 impulseScalar = -(1.0f + restitution);
			{
				mat4 RA = Mat4FromQuaternion(transformA->rotation);
				mat4 noRA = Mat4Transpose(RA);
				mat4 worldInvMomentOfInertiaA = Mat4Multiply(Mat4Multiply(RA,
							rigidBodyA->invMomentOfInertiaTensor), noRA);

				mat4 RB = Mat4FromQuaternion(transformB->rotation);
				mat4 noRB = Mat4Transpose(RB);
				mat4 worldInvMomentOfInertiaB = Mat4Multiply(Mat4Multiply(RB,
							rigidBodyB->invMomentOfInertiaTensor), noRB);

				v3 armCrossNormalA = V3Cross(localHitPointA, hitNormal);
				v3 armCrossNormalB = V3Cross(localHitPointB, hitNormal);

				impulseScalar /= rigidBodyA->invMass + rigidBodyB->invMass +
					V3Dot(V3Cross(Mat4TransformDirection(worldInvMomentOfInertiaA,
						armCrossNormalA), localHitPointA), hitNormal) +
					V3Dot(V3Cross(Mat4TransformDirection(worldInvMomentOfInertiaB,
						armCrossNormalB), localHitPointB), hitNormal);
			}
			collision->normalImpulseMag = impulseScalar;
			impulseVector *= impulseScalar;

			v3 force = impulseVector / deltaTime;

			rigidBodyA->totalForce += force;
			rigidBodyB->totalForce -= force;

			v3 torque;
			torque = V3Cross(localHitPointA, impulseVector) / deltaTime;
			rigidBodyA->totalTorque += torque;
			torque = V3Cross(localHitPointB, impulseVector) / deltaTime;
			rigidBodyB->totalTorque -= torque;
		}

		// Friction impulse
		for (u32 collisionIdx = 0; collisionIdx < collisions.count; ++collisionIdx)
		{
			const Collision* collision = &collisions[collisionIdx];
			//if (collision->penetrationLength <= 0.0f)
				//continue;

			RigidBody *rigidBodyA = GetEntityRigidBody(gameState, collision->entityA);
			RigidBody *rigidBodyB = GetEntityRigidBody(gameState, collision->entityB);
			bool hasRigidBodyA = rigidBodyA != nullptr;
			bool hasRigidBodyB = rigidBodyB != nullptr;

			if (!hasRigidBodyA && !hasRigidBodyB)
				continue;

			if (!rigidBodyA) rigidBodyA = &RIGID_BODY_STATIC;
			if (!rigidBodyB) rigidBodyB = &RIGID_BODY_STATIC;

			Transform *transformA = GetEntityTransform(gameState, collision->entityA);
			Transform *transformB = GetEntityTransform(gameState, collision->entityB);

			v3 hitNormal = collision->hitNormal;
			v3 localHitPointA = collision->hitPoint - transformA->translation;
			v3 localHitPointB = collision->hitPoint - transformB->translation;

			v3 angVelAtHitPointA = V3Cross(rigidBodyA->angularVelocity, localHitPointA);
			v3 velAtHitPointA = rigidBodyA->velocity + angVelAtHitPointA;

			v3 angVelAtHitPointB = V3Cross(rigidBodyB->angularVelocity, localHitPointB);
			v3 velAtHitPointB = rigidBodyB->velocity + angVelAtHitPointB;

			v3 relVelAtHitPoint = velAtHitPointA - velAtHitPointB;

			f32 speedAlongHitNormal = V3Dot(relVelAtHitPoint, hitNormal);
			if (speedAlongHitNormal > 0)
				continue;

			v3 tangent = relVelAtHitPoint - hitNormal * speedAlongHitNormal;
			if (V3EqualWithEpsilon(tangent, {}, 0.00001f))
				continue;
			tangent = V3Normalize(tangent);

			f32 staticFriction  = (rigidBodyA->staticFriction  + rigidBodyB->staticFriction)  * 0.5f;
			f32 dynamicFriction = (rigidBodyA->dynamicFriction + rigidBodyB->dynamicFriction) * 0.5f;

			// Impulse scalar
			f32 impulseScalar = -V3Dot(relVelAtHitPoint, tangent);
			{
				mat4 RA = Mat4FromQuaternion(transformA->rotation);
				mat4 noRA = Mat4Transpose(RA);
				mat4 worldInvMomentOfInertiaA = Mat4Multiply(Mat4Multiply(RA,
							rigidBodyA->invMomentOfInertiaTensor), noRA);

				mat4 RB = Mat4FromQuaternion(transformB->rotation);
				mat4 noRB = Mat4Transpose(RB);
				mat4 worldInvMomentOfInertiaB = Mat4Multiply(Mat4Multiply(RB,
							rigidBodyB->invMomentOfInertiaTensor), noRB);

				v3 armCrossTangentA = V3Cross(localHitPointA, tangent);
				v3 armCrossTangentB = V3Cross(localHitPointB, tangent);

				impulseScalar /= rigidBodyA->invMass + rigidBodyB->invMass +
					V3Dot(V3Cross(Mat4TransformDirection(worldInvMomentOfInertiaA,
						armCrossTangentA), localHitPointA), tangent) +
					V3Dot(V3Cross(Mat4TransformDirection(worldInvMomentOfInertiaB,
						armCrossTangentB), localHitPointB), tangent);
			}

			if (Abs(impulseScalar) > collision->normalImpulseMag * staticFriction)
				impulseScalar = collision->normalImpulseMag * dynamicFriction;

			v3 impulseVector = tangent * impulseScalar;

			v3 force = impulseVector / deltaTime;

			rigidBodyA->totalForce += force;
			rigidBodyB->totalForce -= force;

			v3 torque;
			torque = V3Cross(localHitPointA, impulseVector) / deltaTime;
			rigidBodyA->totalTorque += torque;
			torque = V3Cross(localHitPointB, impulseVector) / deltaTime;
			rigidBodyB->totalTorque -= torque;
		}

		// Depenetrate
		if (!g_debugContext->disableDepenetration)
		{
			for (u32 collisionIdx = 0; collisionIdx < collisions.count; ++collisionIdx)
			{
				const Collision* collision = &collisions[collisionIdx];
				if (collision->penetrationLength <= 0.0f)
					continue;

				RigidBody *rigidBodyA = GetEntityRigidBody(gameState, collision->entityA);
				RigidBody *rigidBodyB = GetEntityRigidBody(gameState, collision->entityB);

				Transform *transformA = GetEntityTransform(gameState, collision->entityA);
				Transform *transformB = GetEntityTransform(gameState, collision->entityB);

				if (rigidBodyA)
					transformA->translation += collision->hitNormal * collision->penetrationLength;
				if (rigidBodyB)
					transformB->translation -= collision->hitNormal * collision->penetrationLength;
			}
		}

		// Rigid bodies
		if (!g_debugContext->pausePhysics)
		{
			for (u32 rigidBodyIdx = 0; rigidBodyIdx < gameState->rigidBodies.count; ++rigidBodyIdx)
			{
				RigidBody *rigidBody = &gameState->rigidBodies[rigidBodyIdx];
				Transform *transform = GetEntityTransform(gameState, rigidBody->entityHandle);
				ASSERT(transform); // There should never be an orphaned rigid body.

				mat4 R = Mat4FromQuaternion(transform->rotation);
				mat4 noR = Mat4Transpose(R);
				mat4 worldInvMomentOfInertia = Mat4Multiply(Mat4Multiply(R, rigidBody->invMomentOfInertiaTensor), noR);

				rigidBody->velocity += rigidBody->totalForce * rigidBody->invMass * deltaTime;
				transform->translation += rigidBody->velocity * deltaTime;

				v3 angularAcceleration = Mat4TransformDirection(worldInvMomentOfInertia,
						rigidBody->totalTorque);
				rigidBody->angularVelocity += angularAcceleration * deltaTime;
				v3 deltaRot = rigidBody->angularVelocity * deltaTime;
				if (deltaRot.x || deltaRot.y || deltaRot.z)
				{
					f32 angle = V3Length(deltaRot);
					transform->rotation = QuaternionMultiply(
							QuaternionFromAxisAngle(deltaRot / angle, angle),
							transform->rotation);
					transform->rotation = V4Normalize(transform->rotation);
				}

				rigidBody->totalForce = { 0, 0, -9.8f };
				rigidBody->totalTorque = { 0, 0, 0 };

				f32 drag = 0.1f;
				f32 angularDrag = 0.1f;
				rigidBody->velocity -= rigidBody->velocity * deltaTime * drag;
				rigidBody->angularVelocity -= rigidBody->angularVelocity * deltaTime * angularDrag;
			}
		}
		if (g_debugContext->resetMomenta)
		{
			for (u32 rigidBodyIdx = 0; rigidBodyIdx < gameState->rigidBodies.count; ++rigidBodyIdx)
			{
				RigidBody *rigidBody = &gameState->rigidBodies[rigidBodyIdx];

				rigidBody->velocity = {};
				rigidBody->angularVelocity = {};
			}
			g_debugContext->resetMomenta = false;
		}

		// Move camera
		if (controller->mouseRight.endedDown)
		{
			static v2 oldMousePos = controller->mousePos;

			if (!controller->mouseRight.changed)
			{
				v2 delta = controller->mousePos - oldMousePos;
				gameState->camYaw += delta.x;
				gameState->camPitch -= delta.y;
			}

			oldMousePos = controller->mousePos;

			v3 camFw = -Mat4ColumnV3(gameState->viewMatrix, 2);
			if (controller->up.endedDown)
			{
				gameState->camPos += camFw * 8.0f * deltaTime;
			}
			else if (controller->down.endedDown)
			{
				gameState->camPos -= camFw * 8.0f * deltaTime;
			}

			v3 camRight = Mat4ColumnV3(gameState->viewMatrix, 0);
			if (controller->right.endedDown)
			{
				gameState->camPos += camRight * 8.0f * deltaTime;
			}
			else if (controller->left.endedDown)
			{
				gameState->camPos -= camRight * 8.0f * deltaTime;
			}

			if (controller->jump.endedDown)
			{
				gameState->camPos += { 0, 0, 8.0f * deltaTime };
			}
			else if (controller->crouch.endedDown)
			{
				gameState->camPos -= { 0, 0, 8.0f * deltaTime };
			}
		}

		// Mouse picking
		PickingResult pickingResult = PICKING_NOTHING;
		{
			v4 worldCursorPos = { controller->mousePos.x, controller->mousePos.y, -1, 1 };
			mat4 invViewMatrix = gameState->invViewMatrix;
			mat4 invProjMatrix = Mat4Inverse(gameState->projMatrix);
			worldCursorPos = Mat4TransformV4(invProjMatrix, worldCursorPos);
			worldCursorPos /= worldCursorPos.w;
			worldCursorPos = Mat4TransformV4(invViewMatrix, worldCursorPos);
			v3 cursorXYZ = { worldCursorPos.x, worldCursorPos.y, worldCursorPos.z };

			v3 camPos = gameState->camPos;
			v3 origin = camPos;
			v3 dir = cursorXYZ - origin;
			g_debugContext->hoveredEntity = ENTITY_HANDLE_INVALID;
			f32 closestDistance = INFINITY;
			for (u32 colliderIdx = 0; colliderIdx < gameState->colliders.count; ++colliderIdx)
			{
				Collider *collider = &gameState->colliders[colliderIdx];
				Transform *transform = GetEntityTransform(gameState, collider->entityHandle);
				ASSERT(transform);
				v3 hit;
				v3 hitNor;
				if (RayColliderIntersection(origin, dir, true, transform,
							collider, &hit, &hitNor))
				{
					f32 dist = V3SqrLen(hit - camPos);
					if (dist < closestDistance)
					{
						closestDistance = dist;
						g_debugContext->hoveredEntity = collider->entityHandle;
						pickingResult = PICKING_ENTITY;
					}
				}
			}

			// @Hack: hard coded gizmo colliders!
			if (Transform *selectedEntity = GetEntityTransform(gameState, g_debugContext->selectedEntity))
			{
				Collider gizmoCollider = {};
				gizmoCollider.type = COLLIDER_CONVEX_HULL;

				Transform gizmoZT;
				if (g_debugContext->editRelative)
					gizmoZT = *selectedEntity;
				else
				{
					gizmoZT = TRANSFORM_IDENTITY;
					gizmoZT.translation = selectedEntity->translation;
				}

				Transform gizmoXT = gizmoZT;
				Transform gizmoYT = gizmoZT;

				gizmoXT.rotation = QuaternionMultiply(gizmoXT.rotation,
						QuaternionFromEulerZYX({ 0, HALFPI, 0 }));
				gizmoYT.rotation = QuaternionMultiply(gizmoYT.rotation,
						QuaternionFromEulerZYX({ -HALFPI, 0, 0 }));

				f32 dist = V3Length(gameState->camPos - selectedEntity->translation);
				gizmoCollider.convexHull.scale = 0.2f * dist;

				if (g_debugContext->currentEditMode == EDIT_MOVE)
				{
					gizmoCollider.convexHull.meshRes = GetResource("editor_arrow_collision.b");

					v3 hit;
					v3 hitNor;
					f32 hitSqrDistToCam = INFINITY;
					if (RayColliderIntersection(origin, dir, true, &gizmoXT, &gizmoCollider, &hit, &hitNor))
					{
						pickingResult = PICKING_GIZMO_X;
						hitSqrDistToCam = V3SqrLen(hit - camPos);
					}
					if (RayColliderIntersection(origin, dir, true, &gizmoYT, &gizmoCollider, &hit, &hitNor))
					{
						f32 newSqrDist = V3SqrLen(hit - camPos);
						if (newSqrDist < hitSqrDistToCam)
						{
							pickingResult = PICKING_GIZMO_Y;
							hitSqrDistToCam = newSqrDist;
						}
					}
					if (RayColliderIntersection(origin, dir, true, &gizmoZT, &gizmoCollider, &hit, &hitNor))
					{
						f32 newSqrDist = V3SqrLen(hit - camPos);
						if (newSqrDist < hitSqrDistToCam)
						{
							pickingResult = PICKING_GIZMO_Z;
							hitSqrDistToCam = newSqrDist;
						}
					}
				}
				else if (g_debugContext->currentEditMode == EDIT_ROTATE)
				{
					gizmoCollider.convexHull.meshRes = GetResource("editor_circle_collision.b");

					v3 hit;
					v3 hitNor;
					f32 hitSqrDistToCam = INFINITY;
					if (RayColliderIntersection(origin, dir, true, &gizmoXT, &gizmoCollider, &hit, &hitNor))
					{
						f32 newSqrDist = V3SqrLen(hit - camPos);
						if (newSqrDist < hitSqrDistToCam)
						{
							pickingResult = PICKING_GIZMO_ROT_X;
							hitSqrDistToCam = newSqrDist;
						}
					}
					if (RayColliderIntersection(origin, dir, true, &gizmoYT, &gizmoCollider, &hit, &hitNor))
					{
						f32 newSqrDist = V3SqrLen(hit - camPos);
						if (newSqrDist < hitSqrDistToCam)
						{
							pickingResult = PICKING_GIZMO_ROT_Y;
							hitSqrDistToCam = newSqrDist;
						}
					}
					if (RayColliderIntersection(origin, dir, true, &gizmoZT, &gizmoCollider, &hit, &hitNor))
					{
						f32 newSqrDist = V3SqrLen(hit - camPos);
						if (newSqrDist < hitSqrDistToCam)
						{
							pickingResult = PICKING_GIZMO_ROT_Z;
							hitSqrDistToCam = newSqrDist;
						}
					}
				}
			}
		}

		//Editor stuff
		{
			static bool grabbingX = false;
			static bool grabbingY = false;
			static bool grabbingZ = false;
			static bool rotatingX = false;
			static bool rotatingY = false;
			static bool rotatingZ = false;
			static v2 oldMousePos = controller->mousePos;

			if (controller->editMove.endedDown && controller->editMove.changed)
				g_debugContext->currentEditMode = EDIT_MOVE;
			else if (controller->editRotate.endedDown && controller->editRotate.changed)
				g_debugContext->currentEditMode = EDIT_ROTATE;

			if (controller->mouseLeft.endedDown && controller->mouseLeft.changed)
			{
				if (pickingResult == PICKING_GIZMO_X)
					grabbingX = true;
				else if (pickingResult == PICKING_GIZMO_Y)
					grabbingY = true;
				else if (pickingResult == PICKING_GIZMO_Z)
					grabbingZ = true;
				else if (pickingResult == PICKING_GIZMO_ROT_X)
					rotatingX = true;
				else if (pickingResult == PICKING_GIZMO_ROT_Y)
					rotatingY = true;
				else if (pickingResult == PICKING_GIZMO_ROT_Z)
					rotatingZ = true;
				else if (pickingResult == PICKING_NOTHING)
					g_debugContext->selectedEntity = ENTITY_HANDLE_INVALID;
				else
					g_debugContext->selectedEntity = g_debugContext->hoveredEntity;
			}
			else if (!controller->mouseLeft.endedDown)
			{
				grabbingX = false;
				grabbingY = false;
				grabbingZ = false;
				rotatingX = false;
				rotatingY = false;
				rotatingZ = false;
			}

			if (Transform *selectedEntity = GetEntityTransform(gameState, g_debugContext->selectedEntity))
			{
				v4 entityScreenPos = V4Point(selectedEntity->translation);
				entityScreenPos = Mat4TransformV4(gameState->viewMatrix, entityScreenPos);
				entityScreenPos = Mat4TransformV4(gameState->projMatrix, entityScreenPos);
				entityScreenPos /= entityScreenPos.w;

				if (grabbingX || grabbingY || grabbingZ)
				{
					v3 worldDir = { (f32)grabbingX, (f32)grabbingY, (f32)grabbingZ };
					if (g_debugContext->editRelative)
						worldDir = QuaternionRotateVector(selectedEntity->rotation, worldDir);

					v4 offsetScreenPos = V4Point(selectedEntity->translation + worldDir);
					offsetScreenPos = Mat4TransformV4(gameState->viewMatrix, offsetScreenPos);
					offsetScreenPos = Mat4TransformV4(gameState->projMatrix, offsetScreenPos);
					offsetScreenPos /= offsetScreenPos.w;

					v2 screenSpaceDragDir = offsetScreenPos.xy - entityScreenPos.xy;

					v2 mouseDelta = controller->mousePos - oldMousePos;
					f32 delta = V2Dot(screenSpaceDragDir, mouseDelta) / V2SqrLen(screenSpaceDragDir);
					selectedEntity->translation += worldDir * delta;
				}
				else if (rotatingX || rotatingY || rotatingZ)
				{
					// Draw out two vectors on the screen, one from entity to the old cursor
					// position, another from entity to new cursor position. Then calculate angle
					// between them and rotate entity by that angle delta.
					v2 oldDelta = oldMousePos - entityScreenPos.xy;
					v2 newDelta = controller->mousePos - entityScreenPos.xy;
					f32 theta = Atan2(newDelta.y, newDelta.x) - Atan2(oldDelta.y, oldDelta.x);

					// Invert angle if we are grabbing the gizmo from behind it's normal.
					{
						v3 worldNormal = { (f32)rotatingX, (f32)rotatingY, (f32)rotatingZ };
						if (g_debugContext->editRelative)
							worldNormal = QuaternionRotateVector(selectedEntity->rotation, worldNormal);
						v3 camToObject = selectedEntity->translation - gameState->camPos;
						f32 dot = V3Dot(worldNormal, camToObject);
						if (dot > 0)
							theta = -theta;
					}

					v3 euler = { rotatingX * theta, rotatingY * theta, rotatingZ * theta };

					if (g_debugContext->editRelative)
						selectedEntity->rotation = QuaternionMultiply(selectedEntity->rotation,
							QuaternionFromEulerZYX(euler));
					else
						selectedEntity->rotation = QuaternionMultiply(QuaternionFromEulerZYX(euler),
							selectedEntity->rotation);
				}
			}

			oldMousePos = controller->mousePos;
		}

#if DEBUG_BUILD
		if (g_debugContext->drawGJKPolytope)
		{
			DebugVertex *gjkVertices;
			u32 gjkVertexCount;
			GetGJKStepGeometry(g_debugContext->gjkDrawStep, &gjkVertices, &gjkVertexCount);
			DrawDebugTriangles(gjkVertices, gjkVertexCount);

			DrawDebugCubeAA(g_debugContext->GJKNewPoint[g_debugContext->gjkDrawStep], 0.03f);

			DrawDebugCubeAA({}, 0.04f, {1,0,1});
		}

		if (g_debugContext->drawEPAPolytope)
		{
			int stepToDraw = Clamp(g_debugContext->polytopeDrawStep, 0,
					g_debugContext->epaStepCount - 1);

			DebugVertex *epaVertices;
			u32 epaVertexCount;
			GetEPAStepGeometry(stepToDraw, &epaVertices, &epaVertexCount);
			DrawDebugTriangles(epaVertices, epaVertexCount);

			DrawDebugCubeAA(g_debugContext->epaNewPoint[stepToDraw], 0.03f);

			DrawDebugCubeAA({}, 0.04f, {1,0,1});
		}
#endif
	}

	// Draw
	{
		UpdateViewProjMatrices(gameState);

		const v4 clearColor = { 0.95f, 0.88f, 0.05f, 1.0f };
		ClearColorBuffer(clearColor);
		ClearDepthBuffer();

		Render(gameState, deltaTime);

		UnbindFrameBuffer();
		{
			u32 w, h;
			GetWindowSize(&w, &h);
			SetViewport(0, 0, w, h);
		}

		// Gizmos
#if EDITOR_PRESENT
		{
			if (Transform *selectedEntity = GetEntityTransform(gameState, g_debugContext->selectedEntity))
			{
				ClearDepthBuffer();

				UseProgram(g_debugContext->editorGizmoProgram);
				DeviceUniform viewUniform = GetUniform(g_debugContext->editorGizmoProgram, "view");
				UniformMat4Array(viewUniform, 1, gameState->viewMatrix.m);
				DeviceUniform projUniform = GetUniform(g_debugContext->editorGizmoProgram, "projection");
				UniformMat4Array(projUniform, 1, gameState->projMatrix.m);
				DeviceUniform modelUniform = GetUniform(g_debugContext->editorGizmoProgram, "model");
				DeviceUniform colorUniform = GetUniform(g_debugContext->editorGizmoProgram, "color");

				const Resource *arrowRes = GetResource("editor_arrow.b");
				const Resource *circleRes = GetResource("editor_circle.b");

				v3 pos = selectedEntity->translation;
				v4 rot = QUATERNION_IDENTITY;
				if (g_debugContext->editRelative)
					rot = selectedEntity->rotation;

				f32 gizmoSize = 0.2f;
				f32 dist = V3Length(gameState->camPos - pos);
				gizmoSize *= dist;

				mat4 gizmoModel = Mat4Compose(pos, rot, gizmoSize);

				UniformMat4Array(modelUniform, 1, gizmoModel.m);
				UniformV4(colorUniform, { 0, 0, 1, 1 });
				if (g_debugContext->currentEditMode == EDIT_MOVE)
					RenderIndexedMesh(arrowRes->mesh.deviceMesh);
				else if (g_debugContext->currentEditMode == EDIT_ROTATE)
					RenderIndexedMesh(circleRes->mesh.deviceMesh);

				v4 xRot = QuaternionMultiply(rot, QuaternionFromEulerZYX({ 0, HALFPI, 0 }));
				gizmoModel = Mat4Compose(pos, xRot, gizmoSize);
				UniformMat4Array(modelUniform, 1, gizmoModel.m);
				UniformV4(colorUniform, { 1, 0, 0, 1 });
				if (g_debugContext->currentEditMode == EDIT_MOVE)
					RenderIndexedMesh(arrowRes->mesh.deviceMesh);
				else if (g_debugContext->currentEditMode == EDIT_ROTATE)
					RenderIndexedMesh(circleRes->mesh.deviceMesh);

				v4 yRot = QuaternionMultiply(rot, QuaternionFromEulerZYX({ -HALFPI, 0, 0 }));
				gizmoModel = Mat4Compose(pos, yRot, gizmoSize);
				UniformMat4Array(modelUniform, 1, gizmoModel.m);
				UniformV4(colorUniform, { 0, 1, 0, 1 });
				if (g_debugContext->currentEditMode == EDIT_MOVE)
					RenderIndexedMesh(arrowRes->mesh.deviceMesh);
				else if (g_debugContext->currentEditMode == EDIT_ROTATE)
					RenderIndexedMesh(circleRes->mesh.deviceMesh);
			}
		}
#endif
	}
}

void CleanupGame(GameState *gameState)
{
	(void) gameState;
	return;
}
