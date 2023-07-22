void ImguiShowDebugWindow(GameState *gameState)
{
#if DEBUG_BUILD && USING_IMGUI
	ImGui::SetNextWindowPos(ImVec2(8, 340), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(315, 425), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Debug things", nullptr, 0))
	{
		ImGui::End();
		return;
	}

	ImGui::Checkbox("Local", &g_debugContext->editRelative);

	ImGui::Checkbox("Pause updates", &g_debugContext->pauseUpdates);

	if (ImGui::CollapsingHeader("Memory usage"))
	{
		static f32 frameMemSamples[32];
		static f32 transMemSamples[32];
		static f32 buddyMemSamples[32];
		static int memSamplesIdx = 0;
		frameMemSamples[memSamplesIdx] = (f32)(g_memory->lastFrameUsage);
		transMemSamples[memSamplesIdx] = (f32)((u8 *)g_memory->transientPtr -
				(u8 *)g_memory->transientMem);
		buddyMemSamples[memSamplesIdx] = (f32)(g_memory->buddyMemoryUsage);
		++memSamplesIdx;
		if (memSamplesIdx >= ArrayCount(frameMemSamples))
			memSamplesIdx = 0;

		ImGui::PlotHistogram("Frame alloc", frameMemSamples, ArrayCount(frameMemSamples), memSamplesIdx, "", 0,
				Memory::frameSize, ImVec2(0, 80.0f));

		ImGui::PlotHistogram("Transient alloc", transMemSamples, ArrayCount(transMemSamples), memSamplesIdx, "", 0,
				Memory::transientSize, ImVec2(0, 80.0f));

		ImGui::PlotHistogram("Buddy alloc", buddyMemSamples, ArrayCount(buddyMemSamples), memSamplesIdx, "", 0,
				Memory::buddySize, ImVec2(0, 80.0f));
	}

	ImGui::Checkbox("Debug draws in wireframe", &g_debugContext->wireframeDebugDraws);
	ImGui::SliderFloat("Time speed", &gameState->timeMultiplier, 0.001f, 10.0f, "factor = %.3f", ImGuiSliderFlags_Logarithmic);

	if (ImGui::CollapsingHeader("Physics debug"))
	{
		ImGui::Checkbox("Pause physics", &g_debugContext->pausePhysics);
		ImGui::Checkbox("Pause upon contact", &g_debugContext->pausePhysicsOnContact);
		ImGui::Checkbox("Disable friction", &g_debugContext->disableFriction);
		if (ImGui::Button("Reset momentum")) g_debugContext->resetMomentum = true;
	}

	if (ImGui::CollapsingHeader("Collision debug"))
	{
		ImGui::Checkbox("Disable depenetration", &g_debugContext->disableDepenetration);

		ImGui::Checkbox("Draw AABBs", &g_debugContext->drawAABBs);
		ImGui::Checkbox("Draw GJK support function results", &g_debugContext->drawSupports);
		ImGui::Checkbox("Enable verbose collision logging", &g_debugContext->verboseCollisionLogging);

		ImGui::Separator();

		ImGui::Text("GJK:");
		ImGui::Checkbox("Draw polytope", &g_debugContext->drawGJKPolytope);
		ImGui::SameLine();
		ImGui::Checkbox("Freeze", &g_debugContext->freezeGJKGeom);

		static bool gjkLastStep = true;
		ImGui::Checkbox("Last step", &gjkLastStep);
		ImGui::SameLine();
		if (ImGui::InputInt("Step", &g_debugContext->gjkDrawStep))
			gjkLastStep = false;

		if (gjkLastStep)
			g_debugContext->gjkDrawStep = g_debugContext->gjkStepCount - 1;
		else
			g_debugContext->gjkDrawStep = Clamp(g_debugContext->gjkDrawStep, 0,
					g_debugContext->gjkStepCount - 1);

		ImGui::Separator();

		ImGui::Text("EPA:");
		ImGui::Checkbox("Draw polytope##", &g_debugContext->drawEPAPolytope);
		ImGui::SameLine();
		ImGui::Checkbox("Freeze##", &g_debugContext->freezePolytopeGeom);
		ImGui::Checkbox("Draw closest feature", &g_debugContext->drawEPAClosestFeature);

		static bool epaLastStep = true;
		ImGui::Checkbox("Last step##", &epaLastStep);
		ImGui::SameLine();
		if (ImGui::InputInt("Step##", &g_debugContext->polytopeDrawStep))
			epaLastStep = false;

		if (epaLastStep)
			g_debugContext->polytopeDrawStep = g_debugContext->epaStepCount - 1;
		else
			g_debugContext->polytopeDrawStep = Clamp(g_debugContext->polytopeDrawStep, 0,
					g_debugContext->epaStepCount - 1);
	}

	ImGui::End();
#else
	(void)gameState;
#endif
}

void ImguiShowEntityTab(GameState *gameState)
{
	Transform *selectedEntity = GetEntityTransform(gameState, g_debugContext->selectedEntity);
	if (!selectedEntity)
		return;

	ImGui::Text("Entity #%u:%hhu", g_debugContext->selectedEntity.id,
			g_debugContext->selectedEntity.generation);

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Position", selectedEntity->translation.v, 0.01f);
		ImGui::DragFloat4("Rotation", selectedEntity->rotation.v, 0.01f);
		ImGui::DragFloat3("Scale", selectedEntity->scale.v, 0.01f);
	}

	bool recalculateInersiaTensor = false;

	Collider *collider = GetEntityCollider(gameState, g_debugContext->selectedEntity);
	if (collider)
	{
		bool keep = true;
		if (ImGui::CollapsingHeader("Collider", &keep, ImGuiTreeNodeFlags_DefaultOpen))
		{
			static const char *typeNames[] = {
				"Convex hull",
				"Cube",
				"Sphere",
				"Cylinder",
				"Capsule"
			};

			if (ImGui::BeginCombo("Type", typeNames[collider->type]))
			{
				for (int i = 0; i < ArrayCount(typeNames); i++)
				{
					const bool isSelected = i == collider->type;
					if (ImGui::Selectable(typeNames[i], isSelected))
					{
						collider->type = (ColliderType)i;
						recalculateInersiaTensor = true;
					}

					// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			switch (collider->type)
			{
			case COLLIDER_CONVEX_HULL:
				break;
			case COLLIDER_CUBE:
				if (ImGui::DragFloat("Radius", &collider->cube.radius, 0.01f))
					recalculateInersiaTensor = true;
				break;
			case COLLIDER_SPHERE:
				if (ImGui::DragFloat("Radius", &collider->sphere.radius, 0.01f))
					recalculateInersiaTensor = true;
				break;
			case COLLIDER_CYLINDER:
				if (ImGui::DragFloat("Radius", &collider->cylinder.radius, 0.01f))
					recalculateInersiaTensor = true;
				if (ImGui::DragFloat("Height", &collider->cylinder.height, 0.01f))
					recalculateInersiaTensor = true;
				break;
			case COLLIDER_CAPSULE:
				if (ImGui::DragFloat("Radius", &collider->capsule.radius, 0.01f))
					recalculateInersiaTensor = true;
				if (ImGui::DragFloat("Height", &collider->capsule.height, 0.01f))
					recalculateInersiaTensor = true;
				break;
			}
		}
		if (!keep)
		{
			EntityRemoveCollider(gameState, g_debugContext->selectedEntity);
		}
	}
	else
	{
		if (ImGui::Button("Add collider"))
		{
			collider = ArrayAdd(&gameState->colliders);
			*collider = {};
			collider->type = COLLIDER_CUBE;
			collider->cube.radius = 1;
			collider->cube.offset = {};
			EntityAssignCollider(gameState, g_debugContext->selectedEntity, collider);
		}
	}

	RigidBody *rigidBody = GetEntityRigidBody(gameState, g_debugContext->selectedEntity);
	if (rigidBody)
	{
		bool keep = true;
		if (ImGui::CollapsingHeader("Rigid body", &keep, ImGuiTreeNodeFlags_DefaultOpen))
		{
			f32 mass = 1.0f / rigidBody->invMass;
			if (ImGui::DragFloat("Mass", &mass, 0.01f) && mass > 0)
			{
				rigidBody->invMass = 1.0f / mass;
				recalculateInersiaTensor = true;
			}

			ImGui::DragFloat("Restitution", &rigidBody->restitution, 0.01f);
			ImGui::DragFloat("Static friction", &rigidBody->staticFriction, 0.01f);
			ImGui::DragFloat("Dynamic friction", &rigidBody->dynamicFriction, 0.01f);
		}
		if (recalculateInersiaTensor)
		{
			rigidBody->invMomentOfInertiaTensor = CalculateInverseMomentOfInertiaTensor(*collider,
					rigidBody->invMass);
		}
		if (!keep)
		{
			EntityRemoveRigidBody(gameState, g_debugContext->selectedEntity);
		}
	}
	else
	{
		if (ImGui::Button("Add rigid body"))
		{
			rigidBody = ArrayAdd(&gameState->rigidBodies);
			*rigidBody = {};
			rigidBody->invMass = 1.0f;
			rigidBody->restitution = 0.3f;
			rigidBody->staticFriction = 0.4f;
			rigidBody->dynamicFriction = 0.2f;
			rigidBody->invMomentOfInertiaTensor = CalculateInverseMomentOfInertiaTensor(*collider,
					rigidBody->invMass);
			EntityAssignRigidBody(gameState, g_debugContext->selectedEntity, rigidBody);
		}
	}
}

bool ImguiGetSpringName(void *, int index, const char **name)
{
	*name = TPrintF("Spring #%d\0", index).data;
	return true;
}

void ImguiShowSpringsTab(GameState *gameState)
{
	static int currentSpringIdx = 0;
	if (ImGui::Button("Add"))
		*ArrayAdd(&gameState->springs) = {};
	ImGui::ListBox("Springs", &currentSpringIdx, ImguiGetSpringName, nullptr,
			gameState->springs.count);

	if (currentSpringIdx < 0 || currentSpringIdx >= (int)gameState->springs.count)
		return;

	Spring* currentSpring = &gameState->springs[currentSpringIdx];

	int step = 1;
	if (ImGui::InputScalar("Entity A", ImGuiDataType_U32, &currentSpring->entityA.id, &step))
	{
		currentSpring->entityA.generation = gameState->entityGenerations[currentSpring->entityA.id];
	}
	ImGui::SameLine();
	if (ImGui::Button("Sel##A"))
		currentSpring->entityA = g_debugContext->selectedEntity;

	if (ImGui::InputScalar("Entity B", ImGuiDataType_U32, &currentSpring->entityB.id, &step))
	{
		currentSpring->entityB.generation = gameState->entityGenerations[currentSpring->entityB.id];
	}
	ImGui::SameLine();
	if (ImGui::Button("Sel##B"))
		currentSpring->entityB = g_debugContext->selectedEntity;

	ImGui::DragFloat3("Offset A", currentSpring->offsetA.v, 0.01f);
	ImGui::DragFloat3("Offset B", currentSpring->offsetB.v, 0.01f);

	ImGui::DragFloat("Distance", &currentSpring->distance, 0.01f);
	ImGui::DragFloat("Stiffness", &currentSpring->stiffness, 0.01f);
	ImGui::DragFloat("Damping", &currentSpring->damping, 0.01f);
}

void ImguiShowPropertiesWindow(GameState *gameState)
{
#if DEBUG_BUILD && USING_IMGUI
	ImGui::SetNextWindowPos(ImVec2(600, 340), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(315, 425), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Properties", nullptr, 0))
	{
		ImGui::End();
		return;
	}

	if (ImGui::BeginTabBar("TabsBar"))
	{
		if (ImGui::BeginTabItem("Entity"))
		{
			ImguiShowEntityTab(gameState);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Springs"))
		{
			ImguiShowSpringsTab(gameState);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
#else
	(void)gameState;
#endif
}
