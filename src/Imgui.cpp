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
		if (ImGui::Button("Reset momenta")) g_debugContext->resetMomenta = true;
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
