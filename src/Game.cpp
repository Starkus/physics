#include <stdlib.h>
#include <stddef.h>
#include <memory.h>
#include <string.h>
#include <stdarg.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "stb/stb_sprintf.h"

#define IMGUI_SHOW_DEMO 1

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

#if EDITOR_PRESENT
EditorContext *g_editorContext;
#endif

String TPrintF(const char *format, ...)
{
	char *buffer = (char *)g_memory->framePtr;

	va_list args;
	va_start(args, format);
	u64 size = stbsp_vsprintf(buffer, format, args);
	va_end(args);

	g_memory->framePtr = (u8 *)g_memory->framePtr + size + 1;

	return { size, buffer };
}

#include "DebugDraw.cpp"
#include "Collision.cpp"
#include "BakeryInterop.cpp"
#include "Resource.cpp"
#include "Entity.cpp"
#include "Parsing.cpp"
#include "Physics.cpp"

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
#if EDITOR_PRESENT
	g_editorContext = ALLOC(TransientAllocator, EditorContext);
	*g_editorContext = {};
#endif

	// Init game state
	memset(gameState, 0, sizeof(GameState));
	gameState->timeMultiplier = 1.0f;
	ArrayInit(&gameState->transforms, 4096);
	ArrayInit(&gameState->meshInstances, 4096);
	ArrayInit(&gameState->colliders, 4096);
	ArrayInit(&gameState->rigidBodies, 4096);
	ArrayInit(&gameState->springs, 1024);
	HashMapInit(&gameState->hitPointCache, 256);

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
		g_editorContext->editorSelectedProgram = shaderEditorSelectedRes->shader.programHandle;

		const Resource *shaderEditorGizmoRes = LoadResource(RESOURCETYPE_SHADER, "shaders/shader_editor_gizmo.b");
		g_editorContext->editorGizmoProgram = shaderEditorGizmoRes->shader.programHandle;
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

		Spring *spring = ArrayAdd(&gameState->springs);
		*spring = {};
		spring->distance = 4.0f;
		spring->stiffness = 5.0f;
		spring->damping = 0.4f;

		const Resource *cubeRes = GetResource("cube.b");
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
		rigidBody->invMomentOfInertiaTensor = CalculateInverseMomentOfInertiaTensor(*collider,
				rigidBody->invMass);
		EntityAssignRigidBody(gameState, testEntityHandle, rigidBody);
		spring->entityA = testEntityHandle;

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
		transform->scale = { 20.0f, 20.0f, 20.0f };
		meshInstance = ArrayAdd(&gameState->meshInstances);
		meshInstance->meshRes = cubeRes;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		collider = ArrayAdd(&gameState->colliders);
		collider->type = COLLIDER_CUBE;
		collider->cube.radius = 20;
		collider->cube.offset = {};
		EntityAssignCollider(gameState, testEntityHandle, collider);

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
		transform->translation = { -3.0f, 5.0f, 5.0f };
		transform->rotation = QUATERNION_IDENTITY;
		meshInstance = ArrayAdd(&gameState->meshInstances);
		meshInstance->meshRes = sphereRes;
		EntityAssignMesh(gameState, testEntityHandle, meshInstance);
		collider = ArrayAdd(&gameState->colliders);
		collider->type = COLLIDER_SPHERE;
		collider->sphere.radius = 1;
		collider->sphere.offset = {};
		EntityAssignCollider(gameState, testEntityHandle, collider);
		rigidBody = ArrayAdd(&gameState->rigidBodies);
		*rigidBody = {};
		rigidBody->invMass = 1.0f;
		rigidBody->restitution = 0.3f;
		rigidBody->staticFriction = 0.4f;
		rigidBody->dynamicFriction = 0.2f;
		rigidBody->invMomentOfInertiaTensor = CalculateInverseMomentOfInertiaTensor(*collider,
				rigidBody->invMass);
		EntityAssignRigidBody(gameState, testEntityHandle, rigidBody);
		spring->entityB = testEntityHandle;

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

			const mat4 model = Mat4Compose(*transform);

			// @Improve: don't rebind program, textures and all for every mesh! Maybe sort by
			// material.
			const Resource *materialRes = meshRes->mesh.materialRes;
			if (!materialRes)
				materialRes = GetResource("material_default.b");

			BindMaterial(gameState, materialRes, &model);

			RenderIndexedMesh(meshRes->mesh.deviceMesh);
		}
	}

	// Level
	if (0)
	{
		LevelGeometry *level = &gameState->levelGeometry;

		const Resource *materialRes = level->renderMesh->mesh.materialRes;
		if (!materialRes)
			materialRes = GetResource("material_default.b");

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

		//EnableDepthTest();
		SetBackfaceCullingEnabled(true);
		SetFillMode(RENDER_FILL);
	}
#endif

#if EDITOR_PRESENT
	// Selected entity
	if (Transform *selectedEntity = GetEntityTransform(gameState, g_editorContext->selectedEntity))
	{
		SetFillMode(RENDER_LINE);

		UseProgram(g_editorContext->editorSelectedProgram);
		DeviceUniform viewUniform = GetUniform(g_editorContext->editorSelectedProgram, "view");
		UniformMat4Array(viewUniform, 1, gameState->viewMatrix.m);
		DeviceUniform projUniform = GetUniform(g_editorContext->editorSelectedProgram, "projection");
		UniformMat4Array(projUniform, 1, gameState->projMatrix.m);
		DeviceUniform modelUniform = GetUniform(g_editorContext->editorSelectedProgram, "model");

		static f32 t = 0;
		t += deltaTime;
		DeviceUniform timeUniform = GetUniform(g_editorContext->editorSelectedProgram, "time");
		UniformFloat(timeUniform, t);

		const mat4 model = Mat4Compose(selectedEntity->translation, selectedEntity->rotation);
		UniformMat4Array(modelUniform, 1, model.m);

		MeshInstance *meshInstance = GetEntityMesh(gameState, g_editorContext->selectedEntity);
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

void UpdateAndRenderGame(Controller *controller, f32 deltaTime, f32 lastUpdateTook)
{
	GameState *gameState = (GameState *)g_memory->transientMem;

	deltaTime *= gameState->timeMultiplier;

#if USING_IMGUI

#if IMGUI_SHOW_DEMO
	ImGui::ShowDemoWindow();
#endif

	ImguiShowDebugWindow(gameState, lastUpdateTook);
	ImguiShowPropertiesWindow(gameState);
#endif

	if (deltaTime < 0 || deltaTime > 1)
	{
		Log("WARNING: Delta time out of range! %f\n", deltaTime);
		deltaTime = 1 / 60.0f;
	}

	// Update
#if DEBUG_BUILD
	if (!g_debugContext->pauseUpdates)
#endif
	{
#if DEBUG_BUILD
		{
			DebugGeometryBuffer *dgb = &g_debugContext->debugGeometryBuffer;
			dgb->debugCubeCount = 0;
			dgb->triangleVertexCount = 0;
			dgb->lineVertexCount = 0;
		}
#endif

		int iterations = 10;
		f32 physicsStep = deltaTime / (f32)iterations;
		for (int i = 0; i < iterations; ++i)
			SimulatePhysics(gameState, physicsStep);
	}

#if EDITOR_PRESENT
	// Update editor
	{
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
		if (controller->mousePos.x > -INFINITY && controller->mousePos.x < INFINITY &&
			controller->mousePos.y > -INFINITY && controller->mousePos.y < INFINITY)
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
			g_editorContext->hoveredEntity = ENTITY_HANDLE_INVALID;
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
						g_editorContext->hoveredEntity = collider->entityHandle;
						pickingResult = PICKING_ENTITY;
					}
				}
			}

			// @Hack: hard coded gizmo colliders!
			if (Transform *selectedEntity = GetEntityTransform(gameState, g_editorContext->selectedEntity))
			{
				Collider gizmoCollider = {};
				gizmoCollider.type = COLLIDER_CONVEX_HULL;

				Transform gizmoZT;
				if (g_editorContext->editRelative)
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

				if (g_editorContext->currentEditMode == EDIT_MOVE)
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
				else if (g_editorContext->currentEditMode == EDIT_ROTATE)
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
				g_editorContext->currentEditMode = EDIT_MOVE;
			else if (controller->editRotate.endedDown && controller->editRotate.changed)
				g_editorContext->currentEditMode = EDIT_ROTATE;

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
					g_editorContext->selectedEntity = ENTITY_HANDLE_INVALID;
				else
					g_editorContext->selectedEntity = g_editorContext->hoveredEntity;
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

			if (Transform *selectedEntity = GetEntityTransform(gameState, g_editorContext->selectedEntity))
			{
				v4 entityScreenPos = V4Point(selectedEntity->translation);
				entityScreenPos = Mat4TransformV4(gameState->viewMatrix, entityScreenPos);
				entityScreenPos = Mat4TransformV4(gameState->projMatrix, entityScreenPos);
				entityScreenPos /= entityScreenPos.w;

				if (grabbingX || grabbingY || grabbingZ)
				{
					v3 worldDir = { (f32)grabbingX, (f32)grabbingY, (f32)grabbingZ };
					if (g_editorContext->editRelative)
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
						if (g_editorContext->editRelative)
							worldNormal = QuaternionRotateVector(selectedEntity->rotation, worldNormal);
						v3 camToObject = selectedEntity->translation - gameState->camPos;
						f32 dot = V3Dot(worldNormal, camToObject);
						if (dot > 0)
							theta = -theta;
					}

					v3 euler = { rotatingX * theta, rotatingY * theta, rotatingZ * theta };

					if (g_editorContext->editRelative)
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
		// @Todo: draw without using debug tools
		if (Collider *collider = GetEntityCollider(gameState, g_editorContext->selectedEntity))
		{
			Transform *transform = GetEntityTransform(gameState, g_editorContext->selectedEntity);
			switch (collider->type)
			{
			case COLLIDER_CUBE:
			{
				v3 vertices[24];
				for (int i = 0; i < 8; ++i)
					vertices[i] = { -0.5f+(i%2), -0.5f+((i/2)%2), -0.5f+((i/4)%2) };
				for (int i = 0; i < 8; ++i)
					vertices[i+8] = { -0.5f+((i/2)%2), -0.5f+(i%2), -0.5f+((i/4)%2) };
				for (int i = 0; i < 8; ++i)
					vertices[i+16] = { -0.5f+((i/4)%2), -0.5f+((i/2)%2), -0.5f+(i%2) };

				for (int i = 0; i < 24; ++i)
				{
					v3 v = vertices[i];
					v = QuaternionRotateVector(transform->rotation, v);
					v *= 2.0f * collider->cube.radius;
					v += transform->translation;
					vertices[i] = v;
				}

				DrawDebugLines(vertices, 24, { 0.0f, 0.7f, 1.0f });
			} break;
			}
		}
#endif
	}
#endif

#if DEBUG_BUILD
	{
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
	}
#endif

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
			if (Transform *selectedEntity = GetEntityTransform(gameState, g_editorContext->selectedEntity))
			{
				ClearDepthBuffer();

				UseProgram(g_editorContext->editorGizmoProgram);
				DeviceUniform viewUniform = GetUniform(g_editorContext->editorGizmoProgram, "view");
				UniformMat4Array(viewUniform, 1, gameState->viewMatrix.m);
				DeviceUniform projUniform = GetUniform(g_editorContext->editorGizmoProgram, "projection");
				UniformMat4Array(projUniform, 1, gameState->projMatrix.m);
				DeviceUniform modelUniform = GetUniform(g_editorContext->editorGizmoProgram, "model");
				DeviceUniform colorUniform = GetUniform(g_editorContext->editorGizmoProgram, "color");

				const Resource *arrowRes = GetResource("editor_arrow.b");
				const Resource *circleRes = GetResource("editor_circle.b");

				v3 pos = selectedEntity->translation;
				v4 rot = QUATERNION_IDENTITY;
				if (g_editorContext->editRelative)
					rot = selectedEntity->rotation;

				f32 gizmoSize = 0.2f;
				f32 dist = V3Length(gameState->camPos - pos);
				gizmoSize *= dist;

				mat4 gizmoModel = Mat4Compose(pos, rot, gizmoSize);

				UniformMat4Array(modelUniform, 1, gizmoModel.m);
				UniformV4(colorUniform, { 0, 0, 1, 1 });
				if (g_editorContext->currentEditMode == EDIT_MOVE)
					RenderIndexedMesh(arrowRes->mesh.deviceMesh);
				else if (g_editorContext->currentEditMode == EDIT_ROTATE)
					RenderIndexedMesh(circleRes->mesh.deviceMesh);

				v4 xRot = QuaternionMultiply(rot, QuaternionFromEulerZYX({ 0, HALFPI, 0 }));
				gizmoModel = Mat4Compose(pos, xRot, gizmoSize);
				UniformMat4Array(modelUniform, 1, gizmoModel.m);
				UniformV4(colorUniform, { 1, 0, 0, 1 });
				if (g_editorContext->currentEditMode == EDIT_MOVE)
					RenderIndexedMesh(arrowRes->mesh.deviceMesh);
				else if (g_editorContext->currentEditMode == EDIT_ROTATE)
					RenderIndexedMesh(circleRes->mesh.deviceMesh);

				v4 yRot = QuaternionMultiply(rot, QuaternionFromEulerZYX({ -HALFPI, 0, 0 }));
				gizmoModel = Mat4Compose(pos, yRot, gizmoSize);
				UniformMat4Array(modelUniform, 1, gizmoModel.m);
				UniformV4(colorUniform, { 0, 1, 0, 1 });
				if (g_editorContext->currentEditMode == EDIT_MOVE)
					RenderIndexedMesh(arrowRes->mesh.deviceMesh);
				else if (g_editorContext->currentEditMode == EDIT_ROTATE)
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
