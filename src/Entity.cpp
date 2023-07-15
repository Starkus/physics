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
	if (collider)
	{
		RigidBody *last = &gameState->rigidBodies[gameState->colliders.count - 1];
		// Retarget moved component's entity to the new pointer.
		u32 idx = (u32)ArrayPointerToIndex(&gameState->rigidBodies, collider);
		gameState->entityRigidBodies[last->entityHandle.id] = idx;
		*rigidBody = *last;

		--gameState->rigidBodies.count;
		gameState->entityRigidBodies[handle.id] = ENTITY_ID_INVALID;
	}
}
