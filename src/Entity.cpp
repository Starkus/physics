inline bool IsEntityHandleValid(GameState *gameState, EntityHandle handle)
{
	return handle != ENTITY_HANDLE_INVALID &&
		handle.generation == gameState->entityGenerations[handle.id];
}

Transform *GetEntityTransform(GameState *gameState, EntityHandle handle)
{
	if (!IsEntityHandleValid(gameState, handle))
		return nullptr;

	u32 idx = gameState->entityTransforms[handle.id];
	if (idx == ENTITY_ID_INVALID)
		return nullptr;

	return &gameState->transforms[idx];
}

EntityHandle EntityHandleFromTransformIndex(GameState *gameState, u32 index)
{
	for (u32 entityId = 0; entityId < MAX_ENTITIES; ++entityId)
	{
		u32 currentIdx = gameState->entityTransforms[entityId];
		if (currentIdx == index)
		{
			return { entityId, gameState->entityGenerations[entityId] };
		}
	}
	return ENTITY_HANDLE_INVALID;
}

MeshInstance *GetEntityMesh(GameState *gameState, EntityHandle handle)
{
	if (!IsEntityHandleValid(gameState, handle))
		return nullptr;

	u32 idx = gameState->entityMeshes[handle.id];
	if (idx == ENTITY_ID_INVALID)
		return nullptr;

	return &gameState->meshInstances[idx];
}

Collider *GetEntityCollider(GameState *gameState, EntityHandle handle)
{
	if (!IsEntityHandleValid(gameState, handle))
		return nullptr;

	u32 idx = gameState->entityColliders[handle.id];
	if (idx == ENTITY_ID_INVALID)
		return nullptr;
	return &gameState->colliders[idx];
}

RigidBody *GetEntityRigidBody(GameState *gameState, EntityHandle handle)
{
	if (!IsEntityHandleValid(gameState, handle))
		return nullptr;

	u32 idx = gameState->entityRigidBodies[handle.id];
	if (idx == ENTITY_ID_INVALID)
		return nullptr;
	return &gameState->rigidBodies[idx];
}

void EntityAssignMesh(GameState *gameState, EntityHandle entityHandle,
		MeshInstance *meshInstance)
{
	meshInstance->entityHandle = entityHandle;
	u32 idx = (u32)ArrayPointerToIndex(&gameState->meshInstances, meshInstance);
	gameState->entityMeshes[entityHandle.id] = idx;
}

void EntityAssignCollider(GameState *gameState, EntityHandle entityHandle, Collider *collider)
{
	collider->entityHandle = entityHandle;
	u32 idx = (u32)ArrayPointerToIndex(&gameState->colliders, collider);
	gameState->entityColliders[entityHandle.id] = idx;
}

void EntityAssignRigidBody(GameState *gameState, EntityHandle entityHandle, RigidBody *rigidBody)
{
	rigidBody->entityHandle = entityHandle;
	u32 idx = (u32)ArrayPointerToIndex(&gameState->rigidBodies, rigidBody);
	gameState->entityRigidBodies[entityHandle.id] = idx;
}

void EntityRemoveMesh(GameState *gameState, EntityHandle entityHandle)
{
	MeshInstance *meshInstance = GetEntityMesh(gameState, entityHandle);
	if (meshInstance)
	{
		MeshInstance *last = &gameState->meshInstances[gameState->meshInstances.count - 1];
		// Retarget moved component's entity to the new pointer.
		u32 idx = (u32)ArrayPointerToIndex(&gameState->meshInstances, meshInstance);
		gameState->entityMeshes[last->entityHandle.id] = idx;
		*meshInstance = *last;

		--gameState->meshInstances.count;
		gameState->entityMeshes[entityHandle.id] = ENTITY_ID_INVALID;
	}
}

void EntityRemoveCollider(GameState *gameState, EntityHandle entityHandle)
{
	Collider *collider = GetEntityCollider(gameState, entityHandle);
	if (collider)
	{
		Collider *last = &gameState->colliders[gameState->colliders.count - 1];
		// Retarget moved component's entity to the new pointer.
		u32 idx = (u32)ArrayPointerToIndex(&gameState->colliders, collider);
		gameState->entityColliders[last->entityHandle.id] = idx;
		*collider = *last;

		--gameState->colliders.count;
		gameState->entityColliders[entityHandle.id] = ENTITY_ID_INVALID;
	}
}

void EntityRemoveRigidBody(GameState *gameState, EntityHandle entityHandle)
{
	RigidBody *rigidBody = GetEntityRigidBody(gameState, entityHandle);
	if (rigidBody)
	{
		RigidBody *last = &gameState->rigidBodies[gameState->rigidBodies.count - 1];
		// Retarget moved component's entity to the new pointer.
		u32 idx = (u32)ArrayPointerToIndex(&gameState->rigidBodies, rigidBody);
		gameState->entityRigidBodies[last->entityHandle.id] = idx;
		*rigidBody = *last;

		--gameState->rigidBodies.count;
		gameState->entityRigidBodies[entityHandle.id] = ENTITY_ID_INVALID;
	}
}

EntityHandle AddEntity(GameState *gameState, Transform **outTransform)
{
	u32 newTransformIdx = gameState->transforms.count;
	Transform *newTransform = ArrayAdd(&gameState->transforms);
	*newTransform = {};

	EntityHandle newHandle = ENTITY_HANDLE_INVALID;

	bool slotFound = false;
	for (int entityId = 0; entityId < MAX_ENTITIES; ++entityId)
	{
		u32 indexInId = gameState->entityTransforms[entityId];
		if (indexInId == ENTITY_ID_INVALID)
		{
			// Assing vacant ID to new entity
			gameState->entityTransforms[entityId] = newTransformIdx;

			// Fill out entity handle
			newHandle.id = entityId;
			// Advance generation by one
			newHandle.generation = ++gameState->entityGenerations[entityId];

			slotFound = true;
			break;
		}
	}
	ASSERT(slotFound);

	*outTransform = newTransform;
	return newHandle;
}

// @Improve: delay actual deletion of entities til end of frame?
void RemoveEntity(GameState *gameState, EntityHandle handle)
{
	// Check handle is valid
	ASSERT(IsEntityHandleValid(gameState, handle));
	if (!IsEntityHandleValid(gameState, handle))
		return;

	u32 transformIdx = gameState->entityTransforms[handle.id];
	Transform *transformPtr = &gameState->transforms[transformIdx];
	// If entity is already deleted there's nothing to do
	if (!transformPtr)
		return;

	// Remove 'components'
	{
		u32 last = gameState->transforms.count - 1;
		Transform *lastPtr = &gameState->transforms[last];
		// Retarget moved component's entity to the new pointer.
		EntityHandle handleOfLast = EntityHandleFromTransformIndex(gameState, last); // @Improve
		gameState->entityTransforms[handleOfLast.id] = transformIdx;
		*transformPtr = *lastPtr;

		gameState->entityTransforms[handle.id] = ENTITY_ID_INVALID;
		--gameState->transforms.count;
	}

	MeshInstance *meshInstance = GetEntityMesh(gameState, handle);
	if (meshInstance)
	{
		MeshInstance *last = &gameState->meshInstances[gameState->meshInstances.count - 1];
		// Retarget moved component's entity to the new pointer.
		u32 idx = (u32)ArrayPointerToIndex(&gameState->meshInstances, meshInstance);
		gameState->entityMeshes[last->entityHandle.id] = idx;
		*meshInstance = *last;

		--gameState->meshInstances.count;
		gameState->entityMeshes[handle.id] = ENTITY_ID_INVALID;
	}

	Collider *collider = GetEntityCollider(gameState, handle);
	if (collider)
	{
		Collider *last = &gameState->colliders[gameState->colliders.count - 1];
		// Retarget moved component's entity to the new pointer.
		u32 idx = (u32)ArrayPointerToIndex(&gameState->colliders, collider);
		gameState->entityColliders[last->entityHandle.id] = idx;
		*collider = *last;

		--gameState->colliders.count;
		gameState->entityColliders[handle.id] = ENTITY_ID_INVALID;
	}

	RigidBody *rigidBody = GetEntityRigidBody(gameState, handle);
	if (rigidBody)
	{
		RigidBody *last = &gameState->rigidBodies[gameState->rigidBodies.count - 1];
		// Retarget moved component's entity to the new pointer.
		u32 idx = (u32)ArrayPointerToIndex(&gameState->rigidBodies, rigidBody);
		gameState->entityRigidBodies[last->entityHandle.id] = idx;
		*rigidBody = *last;

		--gameState->rigidBodies.count;
		gameState->entityRigidBodies[handle.id] = ENTITY_ID_INVALID;
	}
}

mat3 CalculateInverseMomentOfInertiaTensor(Collider collider, f32 invMass)
{
	switch (collider.type)
	{
	case COLLIDER_CONVEX_HULL:
	{
		// @Todo
		f32 r2 = 2.0f;
		f32 s = ((1.0f/invMass)/6.0f) * (r2 * r2);
		f32 invs = 1.0f/s;
		return {
			invs,	0,		0,
			0,		invs,	0,
			0,		0,		invs
		};
	}
	case COLLIDER_CUBE:
	{
		f32 r2 = collider.cube.radius * 2;
		f32 s = ((1.0f/invMass)/6.0f) * (r2 * r2);
		f32 invs = 1.0f/s;
		return {
			invs,	0,		0,
			0,		invs,	0,
			0,		0,		invs
		};
	}
	case COLLIDER_SPHERE:
	{
		f32 mass = 1.0f/invMass;
		f32 z = (mass/2.5f) * (collider.sphere.radius * collider.sphere.radius);
		f32 invz = 1.0f/z;
		return {
			invz,	0,		0,
			0,		invz,	0,
			0,		0,		invz
		};
	}
	case COLLIDER_CYLINDER:
	{
		f32 mass = 1.0f/invMass;
		f32 s = (mass/12.0f) * (collider.cylinder.height * collider.cylinder.height) +
				(mass/4.0f)  * (collider.cylinder.radius * collider.cylinder.radius);
		f32 invs = 1.0f/s;
		f32 z = (mass/2.0f)  * (collider.cylinder.radius * collider.cylinder.radius);
		f32 invz = 1.0f/z;
		return {
			invs,	0,		0,
			0,		invs,	0,
			0,		0,		invz
		};
	}
	case COLLIDER_CAPSULE:
	{
		f32 mass = 1.0f/invMass;
		f32 s = (mass/12.0f) * (collider.capsule.height * collider.capsule.height) +
				(mass/2.5f)  * (collider.capsule.radius * collider.capsule.radius) +
				(mass/4.0f)  * (collider.capsule.radius * collider.capsule.radius);
		f32 invs = 1.0f/s;
		f32 z = (mass/2.0f)  * (collider.capsule.radius * collider.capsule.radius);
		f32 invz = 1.0f/z;
		return {
			invs,	0,		0,
			0,		invs,	0,
			0,		0,		invz
		};
	}
	}
}
