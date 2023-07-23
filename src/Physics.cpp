void SimulatePhysics(GameState *gameState, f32 deltaTime)
{
	// Calculate AABBs
	struct AABB
	{
		v3 min;
		v3 max;
	};
	Array<AABB, FrameAllocator> AABBs;
	ArrayInit(&AABBs, gameState->colliders.count);
	for (u32 colliderIdx = 0; colliderIdx < gameState->colliders.count; ++colliderIdx)
	{
		Collider *collider = &gameState->colliders[colliderIdx];
		Transform *transform = GetEntityTransform(gameState, collider->entityHandle);
		AABB *aabb = ArrayAdd(&AABBs);
		GetAABB(transform, collider, &aabb->min, &aabb->max);
	}

	struct Collision
	{
		EntityHandle entityA;
		EntityHandle entityB;
		v3 hitNormal;
		f32 depth;
		int hitCount;
		v3 hitPoints[8];
		f32 hitDepths[8];
		// We save these to calculate friction
		f32 normalImpulseMags[8];
	};

	// Test for collisions
	DynamicArray<Collision, FrameAllocator> collisions;
	DynamicArrayInit(&collisions, 32);
	for (u32 colliderAIdx = 0; colliderAIdx < gameState->colliders.count; ++colliderAIdx)
	{
		Collider *colliderA = &gameState->colliders[colliderAIdx];
		Transform *transformA = GetEntityTransform(gameState, colliderA->entityHandle);
		ASSERT(transformA); // There should never be an orphaned collider.
		AABB aabbA = AABBs[colliderAIdx];

		for (u32 colliderBIdx = colliderAIdx + 1; colliderBIdx < gameState->colliders.count; ++colliderBIdx)
		{
			Collider *colliderB = &gameState->colliders[colliderBIdx];
			Transform *transformB = GetEntityTransform(gameState, colliderB->entityHandle);
			ASSERT(transformB); // There should never be an orphaned collider.
			AABB aabbB = AABBs[colliderAIdx];

			if (!TestAABBs(aabbA.min, aabbA.max, aabbB.min, aabbB.max))
				continue;

			CollisionInfo collisionInfo = TestCollision(gameState, transformA, transformB,
					colliderA, colliderB);
			if (collisionInfo.hitCount)
			{
				Collision *newCollision = DynamicArrayAdd(&collisions);
				*newCollision = {
					.entityA = colliderA->entityHandle,
					.entityB = colliderB->entityHandle,
					.hitNormal = collisionInfo.hitNormal,
					.depth = collisionInfo.depth,
					.hitCount = collisionInfo.hitCount,
				};
				memcpy(newCollision->hitPoints, collisionInfo.hitPoints,
						collisionInfo.hitCount * sizeof(v3));
				memcpy(newCollision->hitDepths, collisionInfo.hitDepths,
						collisionInfo.hitCount * sizeof(f32));
				memset(newCollision->normalImpulseMags, 0,
						collisionInfo.hitCount * sizeof(v3));

#if DEBUG_BUILD
				if (g_debugContext->pausePhysicsOnContact)
					g_debugContext->pausePhysics = true;
#endif
			}
		}
	}

#if DEBUG_BUILD
	if (g_debugContext->pausePhysics)
		return;
#endif

	for (u32 rigidBodyIdx = 0; rigidBodyIdx < gameState->rigidBodies.count; ++rigidBodyIdx)
	{
		RigidBody *rigidBody = &gameState->rigidBodies[rigidBodyIdx];
		Transform *transform = GetEntityTransform(gameState, rigidBody->entityHandle);
		ASSERT(transform); // There should never be an orphaned rigid body.

		// Gravity
		rigidBody->velocity.z -= 9.8f * deltaTime;

		// Calculate world-space inverse moment of inertia tensors for each rigid body
		mat3 R = Mat3FromQuaternion(transform->rotation);
		mat3 noR = Mat3Transpose(R);
		rigidBody->worldInvMomentOfInertiaTensor = Mat3Multiply(
				Mat3Multiply(R, rigidBody->invMomentOfInertiaTensor), noR);
	}

	// Bounce impulse
	for (u32 collisionIdx = 0; collisionIdx < collisions.count; ++collisionIdx)
	{
		Collision* collision = &collisions[collisionIdx];
		if (collision->depth <= 0.0f)
			continue;

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

		for (int hitIdx = 0; hitIdx < collision->hitCount; ++hitIdx)
		{
			v3 hit = collision->hitPoints[hitIdx];
			v3 hitNormal = collision->hitNormal;
			v3 rA = hit - transformA->translation;
			v3 rB = hit - transformB->translation;

			v3 angVelAtHitPointA = V3Cross(rigidBodyA->angularVelocity, rA);
			v3 velAtHitPointA = rigidBodyA->velocity + angVelAtHitPointA;

			v3 angVelAtHitPointB = V3Cross(rigidBodyB->angularVelocity, rB);
			v3 velAtHitPointB = rigidBodyB->velocity + angVelAtHitPointB;

			v3 relVelAtHitPoint = velAtHitPointA - velAtHitPointB;

			f32 speedAlongHitNormal = V3Dot(relVelAtHitPoint, hitNormal);
			if (speedAlongHitNormal > 0)
				continue;

			f32 restitution = Min(rigidBodyA->restitution, rigidBodyB->restitution);

			// Impulse scalar
			f32 impulseScalar = -(1.0f + restitution) * speedAlongHitNormal;
			{
				v3 armCrossNormalA = V3Cross(rA, hitNormal);
				v3 armCrossNormalB = V3Cross(rB, hitNormal);

				impulseScalar /= rigidBodyA->invMass + rigidBodyB->invMass + V3Dot(
					V3Cross(Mat3TransformVector(rigidBodyA->worldInvMomentOfInertiaTensor,
						armCrossNormalA), rA) +
					V3Cross(Mat3TransformVector(rigidBodyB->worldInvMomentOfInertiaTensor,
						armCrossNormalB), rB), hitNormal);
			}
			ASSERT(impulseScalar >= 0);
			v3 impulseVector = hitNormal * impulseScalar;

#if DEBUG_BUILD
			DrawDebugArrow(hit, hit + impulseVector * 10.0f, {1,0,1});
#endif

			// Store normal impulse scalar to calculate friction
			collision->normalImpulseMags[hitIdx] = impulseScalar;

			v3 force = impulseVector / deltaTime;

			rigidBodyA->totalForce += force;
			rigidBodyB->totalForce -= force;

			v3 torque;
			torque = V3Cross(rA, impulseVector) / deltaTime;
			rigidBodyA->totalTorque += torque;
			torque = V3Cross(rB, impulseVector) / deltaTime;
			rigidBodyB->totalTorque -= torque;
		}
	}

	for (u32 rigidBodyIdx = 0; rigidBodyIdx < gameState->rigidBodies.count; ++rigidBodyIdx)
	{
		RigidBody *rigidBody = &gameState->rigidBodies[rigidBodyIdx];

		rigidBody->velocity += rigidBody->totalForce * rigidBody->invMass * deltaTime;

		v3 angularAcceleration = Mat3TransformVector(rigidBody->worldInvMomentOfInertiaTensor,
				rigidBody->totalTorque);
		rigidBody->angularVelocity += angularAcceleration * deltaTime;

		rigidBody->totalForce = { 0, 0, 0 };
		rigidBody->totalTorque = { 0, 0, 0 };
	}

	// Friction impulse
#if DEBUG_BUILD
	if (!g_debugContext->disableFriction)
#endif
	{
		for (u32 collisionIdx = 0; collisionIdx < collisions.count; ++collisionIdx)
		{
			const Collision* collision = &collisions[collisionIdx];

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

			for (int hitIdx = 0; hitIdx < collision->hitCount; ++hitIdx)
			{
				v3 hit = collision->hitPoints[hitIdx];
				v3 rA = hit - transformA->translation;
				v3 rB = hit - transformB->translation;

				v3 angVelAtHitPointA = V3Cross(rigidBodyA->angularVelocity, rA);
				v3 velAtHitPointA = rigidBodyA->velocity + angVelAtHitPointA;

				v3 angVelAtHitPointB = V3Cross(rigidBodyB->angularVelocity, rB);
				v3 velAtHitPointB = rigidBodyB->velocity + angVelAtHitPointB;

				v3 relVelAtHitPoint = velAtHitPointA - velAtHitPointB;

				f32 speedAlongHitNormal = V3Dot(relVelAtHitPoint, hitNormal);

				v3 tangent = relVelAtHitPoint - hitNormal * speedAlongHitNormal;
				f32 tangentSqrLen = V3SqrLen(tangent);
				if (EqualWithEpsilon(tangentSqrLen, 0, 0.00001f))
					continue;
				tangent /= Sqrt(tangentSqrLen);

				f32 staticFriction  = (rigidBodyA->staticFriction  + rigidBodyB->staticFriction)  * 0.5f;
				f32 dynamicFriction = (rigidBodyA->dynamicFriction + rigidBodyB->dynamicFriction) * 0.5f;

				// Impulse scalar
				f32 impulseScalar = -V3Dot(relVelAtHitPoint, tangent);
				{
					v3 armCrossTangentA = V3Cross(rA, tangent);
					v3 armCrossTangentB = V3Cross(rB, tangent);

					v3 crossA = V3Cross(Mat3TransformVector(rigidBodyA->worldInvMomentOfInertiaTensor,
							armCrossTangentA), rA);
					v3 crossB = V3Cross(Mat3TransformVector(rigidBodyB->worldInvMomentOfInertiaTensor,
							armCrossTangentB), rB);

					impulseScalar /= rigidBodyA->invMass + rigidBodyB->invMass +
						V3Dot(crossA + crossB, tangent);
				}

				f32 normalImpulseMag = collision->normalImpulseMags[hitIdx];
				ASSERT(normalImpulseMag >= 0);
				v3 impulseVector;
				if (Abs(impulseScalar) <= normalImpulseMag * staticFriction)
				{
					impulseVector = tangent * impulseScalar;
					DEBUG_ONLY(DrawDebugArrow(hit, hit + impulseVector * 10.0f, {1,0,0}));
				}
				else
				{
					impulseVector = tangent * -normalImpulseMag * dynamicFriction;
					DEBUG_ONLY(DrawDebugArrow(hit, hit + impulseVector * 10.0f, {0,1,0}));
				}

				v3 force = impulseVector / deltaTime;

				rigidBodyA->totalForce += force;
				rigidBodyB->totalForce -= force;

				v3 torque;
				torque = V3Cross(rA, impulseVector) / deltaTime;
				rigidBodyA->totalTorque += torque;
				torque = V3Cross(rB, impulseVector) / deltaTime;
				rigidBodyB->totalTorque -= torque;
			}
		}
	}

	// Springs
	for (u32 springIdx = 0; springIdx < gameState->springs.count; ++springIdx)
	{
		Spring *spring = &gameState->springs[springIdx];
		Transform *transformA = GetEntityTransform(gameState, spring->entityA);
		if (!transformA) continue;
		Transform *transformB = GetEntityTransform(gameState, spring->entityB);
		if (!transformB) continue;

		RigidBody *rigidBodyA = GetEntityRigidBody(gameState, spring->entityA);
		RigidBody *rigidBodyB = GetEntityRigidBody(gameState, spring->entityB);
		bool hasRigidBodyA = rigidBodyA != nullptr;
		bool hasRigidBodyB = rigidBodyB != nullptr;

		if (!hasRigidBodyA && !hasRigidBodyB)
			continue;

		if (!rigidBodyA) rigidBodyA = &RIGID_BODY_STATIC;
		if (!rigidBodyB) rigidBodyB = &RIGID_BODY_STATIC;

		// @Todo: this doesn't factor in scale
		v3 rA = TransformDirection(*transformA, spring->offsetA);
		v3 rB = TransformDirection(*transformB, spring->offsetB);

#if DEBUG_BUILD
		DrawDebugLine(transformA->translation + rA, transformB->translation + rB, { 0.9f, 0.9f, 0.9f });
#endif

		v3 ab = (transformB->translation + rB) - (transformA->translation + rA);
		f32 currentDistance = V3Length(ab);
		v3 abDir = ab / currentDistance;

		v3 angVelA = V3Cross(rigidBodyA->angularVelocity, rA);
		v3 velA = rigidBodyA->velocity + angVelA;

		v3 angVelB = V3Cross(rigidBodyB->angularVelocity, rB);
		v3 velB = rigidBodyB->velocity + angVelB;

		v3 relVel = velB - velA;
		f32 relVelAlongSpring = V3Dot(relVel, abDir);

		// Impulse scalar
		f32 impulseScalar = (currentDistance - spring->distance) * spring->stiffness;
		impulseScalar += relVelAlongSpring * spring->damping;
		{
			v3 armCrossNormalA = V3Cross(rA, abDir);
			v3 armCrossNormalB = V3Cross(rB, abDir);

			impulseScalar /= rigidBodyA->invMass + rigidBodyB->invMass + V3Dot(
				V3Cross(Mat3TransformVector(rigidBodyA->worldInvMomentOfInertiaTensor,
					armCrossNormalA), rA) +
				V3Cross(Mat3TransformVector(rigidBodyB->worldInvMomentOfInertiaTensor,
					armCrossNormalB), rB), abDir);
		}

		// Prevent explosions
		impulseScalar = Min(impulseScalar, 20.0f);

		v3 impulseVector = abDir * impulseScalar;

		v3 force = impulseVector / deltaTime;

		rigidBodyA->totalForce += force;
		rigidBodyB->totalForce -= force;

		v3 torque;
		torque = V3Cross(rA, impulseVector) / deltaTime;
		rigidBodyA->totalTorque += torque;
		torque = V3Cross(rB, impulseVector) / deltaTime;
		rigidBodyB->totalTorque -= torque;
	}

	// Depenetrate
#if DEBUG_BUILD
	if (!g_debugContext->disableDepenetration)
#endif
	{
		for (u32 collisionIdx = 0; collisionIdx < collisions.count; ++collisionIdx)
		{
			const Collision* collision = &collisions[collisionIdx];

			RigidBody *rigidBodyA = GetEntityRigidBody(gameState, collision->entityA);
			RigidBody *rigidBodyB = GetEntityRigidBody(gameState, collision->entityB);
			if (!rigidBodyA) rigidBodyA = &RIGID_BODY_STATIC;
			if (!rigidBodyB) rigidBodyB = &RIGID_BODY_STATIC;

			if (rigidBodyA->invMass + rigidBodyB->invMass == 0)
				continue;

			Transform *transformA = GetEntityTransform(gameState, collision->entityA);
			Transform *transformB = GetEntityTransform(gameState, collision->entityB);

			const float factor = 0.8f;
			const float slop = 0.001f;

			v3 correction = collision->hitNormal * factor * Max(0.0f, collision->depth - slop);
			correction /= rigidBodyA->invMass + rigidBodyB->invMass;

			transformA->translation += correction * rigidBodyA->invMass;
			transformB->translation -= correction * rigidBodyB->invMass;
		}
	}

	for (u32 rigidBodyIdx = 0; rigidBodyIdx < gameState->rigidBodies.count; ++rigidBodyIdx)
	{
		RigidBody *rigidBody = &gameState->rigidBodies[rigidBodyIdx];
		Transform *transform = GetEntityTransform(gameState, rigidBody->entityHandle);

		// Integrate velocities
		rigidBody->velocity += rigidBody->totalForce * rigidBody->invMass * deltaTime;

		v3 angularAcceleration = Mat3TransformVector(rigidBody->worldInvMomentOfInertiaTensor,
				rigidBody->totalTorque);
		rigidBody->angularVelocity += angularAcceleration * deltaTime;

		// Integrate position and orientation
		transform->translation += rigidBody->velocity * deltaTime;

		v3 deltaRot = rigidBody->angularVelocity * deltaTime;
		if (deltaRot.x || deltaRot.y || deltaRot.z)
		{
			f32 angle = V3Length(deltaRot);
			transform->rotation = QuaternionMultiply(
					QuaternionFromAxisAngle(deltaRot / angle, angle),
					transform->rotation);
			transform->rotation = V4Normalize(transform->rotation);
		}

		// Reset forces
		rigidBody->totalForce = { 0, 0, 0 };
		rigidBody->totalTorque = { 0, 0, 0 };

		f32 drag = 0.01f;
		f32 angularDrag = 0.06f;
		rigidBody->velocity -= rigidBody->velocity * deltaTime * drag;
		rigidBody->angularVelocity -= rigidBody->angularVelocity * deltaTime * angularDrag;
	}

#if DEBUG_BUILD
	if (g_debugContext->resetMomentum)
	{
		for (u32 rigidBodyIdx = 0; rigidBodyIdx < gameState->rigidBodies.count; ++rigidBodyIdx)
		{
			RigidBody *rigidBody = &gameState->rigidBodies[rigidBodyIdx];

			rigidBody->velocity = {};
			rigidBody->angularVelocity = {};
		}
		g_debugContext->resetMomentum = false;
	}
#endif
}
