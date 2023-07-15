struct Button
{
	bool endedDown;
	bool changed;
};

struct Controller
{
	v2 leftStick;
	v2 rightStick;
	union
	{
		struct
		{
			Button up;
			Button down;
			Button left;
			Button right;
			Button jump;
			Button crouch;
			Button camUp;
			Button camDown;
			Button camLeft;
			Button camRight;

			Button editMove;
			Button editRotate;

			Button mouseLeft;
			Button mouseMiddle;
			Button mouseRight;

			Button f1;
			Button f2;
			Button f3;
			Button f4;
		};
		Button b[16];
	};
	v2 mousePos;
};

struct PlatformCode;
struct ImGuiContext;
struct PlatformContext
{
	Memory *memory;
#if USING_IMGUI
	ImGuiContext *imguiContext;
#else
	void *imguiContext;
#endif
};

bool GameResourcePostLoad(Resource *resource, u8 *fileBuffer, bool initialize);
