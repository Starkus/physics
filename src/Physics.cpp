void SimulatePhysics(GameState *gameState, f32 deltaTime)
{
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

		for (u32 colliderBIdx = colliderAIdx + 1; colliderBIdx < gameState->colliders.count; ++colliderBIdx)
		{
			Collider *colliderB = &gameState->colliders[colliderBIdx];
			Transform *transformB = GetEntityTransform(gameState, colliderB->entityHandle);
			ASSERT(transformB); // There should never be an orphaned collider.

			// @Improve: better collision against multiple colliders?
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

	if (g_debugContext->pausePhysics)
		return;

	for (u32 rigidBodyIdx = 0; rigidBodyIdx < gameState->rigidBodies.count; ++rigidBodyIdx)
	{
		RigidBody *rigidBody = &gameState->rigidBodies[rigidBodyIdx];
		Transform *transform = GetEntityTransform(gameState, rigidBody->entityHandle);
		ASSERT(transform); // There should never be an orphaned rigid body.

#if 1
		// Gravity
		rigidBody->velocity.z -= 9.8f * deltaTime;
#endif
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
			v3 localHitPointA = hit - transformA->translation;
			v3 localHitPointB = hit - transformB->translation;

			v3 angVelAtHitPointA = V3Cross(rigidBodyA->angularVelocity, localHitPointA);
			v3 velAtHitPointA = rigidBodyA->velocity + angVelAtHitPointA;

			v3 angVelAtHitPointB = V3Cross(rigidBodyB->angularVelocity, localHitPointB);
			v3 velAtHitPointB = rigidBodyB->velocity + angVelAtHitPointB;

			v3 relVelAtHitPoint = velAtHitPointA - velAtHitPointB;

			f32 speedAlongHitNormal = V3Dot(relVelAtHitPoint, hitNormal);
			if (speedAlongHitNormal > 0)
				continue;

			f32 restitution = Min(rigidBodyA->restitution, rigidBodyB->restitution);

			// Impulse scalar
			f32 impulseScalar = -(1.0f + restitution) * speedAlongHitNormal;
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

				impulseScalar /= rigidBodyA->invMass + rigidBodyB->invMass + V3Dot(
					V3Cross(Mat4TransformDirection(worldInvMomentOfInertiaA,
						armCrossNormalA), localHitPointA) +
					V3Cross(Mat4TransformDirection(worldInvMomentOfInertiaB,
						armCrossNormalB), localHitPointB), hitNormal);
			}
			ASSERT(impulseScalar >= 0);
			v3 impulseVector = hitNormal * impulseScalar;

			DrawDebugArrow(hit, hit + impulseVector * 10.0f, {1,0,1});

			// Store normal impulse scalar to calculate friction
			collision->normalImpulseMags[hitIdx] = impulseScalar;

			v3 force = impulseVector / deltaTime;

			rigidBodyA->totalForce += force;
			rigidBodyB->totalForce -= force;

			v3 torque;
			torque = V3Cross(localHitPointA, impulseVector) / deltaTime;
			rigidBodyA->totalTorque += torque;
			torque = V3Cross(localHitPointB, impulseVector) / deltaTime;
			rigidBodyB->totalTorque -= torque;
		}
	}

	for (u32 rigidBodyIdx = 0; rigidBodyIdx < gameState->rigidBodies.count; ++rigidBodyIdx)
	{
		RigidBody *rigidBody = &gameState->rigidBodies[rigidBodyIdx];
		Transform *transform = GetEntityTransform(gameState, rigidBody->entityHandle);

		rigidBody->velocity += rigidBody->totalForce * rigidBody->invMass * deltaTime;

		mat4 R = Mat4FromQuaternion(transform->rotation);
		mat4 noR = Mat4Transpose(R);
		mat4 worldInvMomentOfInertia = Mat4Multiply(Mat4Multiply(R, rigidBody->invMomentOfInertiaTensor), noR);

		v3 angularAcceleration = Mat4TransformDirection(worldInvMomentOfInertia,
				rigidBody->totalTorque);
		rigidBody->angularVelocity += angularAcceleration * deltaTime;

		rigidBody->totalForce = { 0, 0, 0 };
		rigidBody->totalTorque = { 0, 0, 0 };
	}

	// Friction impulse
	if (!g_debugContext->disableFriction)
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
				v3 localHitPointA = hit - transformA->translation;
				v3 localHitPointB = hit - transformB->translation;

				v3 angVelAtHitPointA = V3Cross(rigidBodyA->angularVelocity, localHitPointA);
				v3 velAtHitPointA = rigidBodyA->velocity + angVelAtHitPointA;

				v3 angVelAtHitPointB = V3Cross(rigidBodyB->angularVelocity, localHitPointB);
				v3 velAtHitPointB = rigidBodyB->velocity + angVelAtHitPointB;

				v3 relVelAtHitPoint = velAtHitPointA - velAtHitPointB;

				f32 speedAlongHitNormal = V3Dot(relVelAtHitPoint, hitNormal);

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

					v3 crossA = V3Cross(Mat4TransformDirection(worldInvMomentOfInertiaA,
							armCrossTangentA), localHitPointA);
					v3 crossB = V3Cross(Mat4TransformDirection(worldInvMomentOfInertiaB,
							armCrossTangentB), localHitPointB);

					impulseScalar /= rigidBodyA->invMass + rigidBodyB->invMass +
						V3Dot(crossA + crossB, tangent);
				}

				f32 normalImpulseMag = collision->normalImpulseMags[hitIdx];
				ASSERT(normalImpulseMag >= 0);
				v3 impulseVector;
				if (Abs(impulseScalar) <= normalImpulseMag * staticFriction)
				{
					impulseVector = tangent * impulseScalar;
					DrawDebugArrow(hit, hit + impulseVector * 10.0f, {1,0,0});
				}
				else
				{
					impulseVector = tangent * -normalImpulseMag * dynamicFriction;
					DrawDebugArrow(hit, hit + impulseVector * 10.0f, {0,1,0});
				}

				v3 force = impulseVector / deltaTime;

				rigidBodyA->totalForce += force;
				rigidBodyB->totalForce -= force;

				v3 torque;
				torque = V3Cross(localHitPointA, impulseVector) / deltaTime;
				rigidBodyA->totalTorque += torque;
				torque = V3Cross(localHitPointB, impulseVector) / deltaTime;
				rigidBodyB->totalTorque -= torque;
			}
		}
	}

	// Depenetrate
	if (!g_debugContext->disableDepenetration)
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

	// Rigid bodies
	for (u32 rigidBodyIdx = 0; rigidBodyIdx < gameState->rigidBodies.count; ++rigidBodyIdx)
	{
		RigidBody *rigidBody = &gameState->rigidBodies[rigidBodyIdx];
		Transform *transform = GetEntityTransform(gameState, rigidBody->entityHandle);

		// Integrate velocities
		rigidBody->velocity += rigidBody->totalForce * rigidBody->invMass * deltaTime;

		mat4 R = Mat4FromQuaternion(transform->rotation);
		mat4 noR = Mat4Transpose(R);
		mat4 worldInvMomentOfInertia = Mat4Multiply(Mat4Multiply(R, rigidBody->invMomentOfInertiaTensor), noR);

		v3 angularAcceleration = Mat4TransformDirection(worldInvMomentOfInertia,
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
}
