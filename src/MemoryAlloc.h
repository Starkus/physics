#define ALLOC(ALLOCATOR, TYPE) (TYPE *)ALLOCATOR::Alloc(sizeof(TYPE), alignof(TYPE))
#define ALLOC_N(ALLOCATOR, TYPE, N) (TYPE *)ALLOCATOR::Alloc(sizeof(TYPE) * N, alignof(TYPE))

struct Memory
{
	void *frameMem, *stackMem, *transientMem, *buddyMem;
	void *framePtr, *stackPtr, *transientPtr;
	u64 lastFrameUsage;
	u8 *buddyBookkeep;

#if DEBUG_BUILD
	// Memory usage tracking info
	u64 buddyMemoryUsage;
#endif

	static const u64 frameSize = 64 * 1024 * 1024;
	static const u64 stackSize = 64 * 1024 * 1024;
	static const u64 transientSize = 64 * 1024 * 1024;
	static const u64 buddySize = 32 * 1024 * 1024;
	static const u64 buddySmallest = 64;
	static const u32 buddyUsedBit = 0x80;
};

void MemoryInit(Memory *memory);
void FrameWipe();
void *BuddyFindFreeBlockOfOrder(u8 desiredOrder, u8 **bookkeep);
u64 BuddyTryMerge(u64 blockIdx);

class FrameAllocator {
public:
	static void *Alloc(u64 size, int alignment);
	static void *Realloc(void *ptr, u64 oldSize, u64 newSize, int alignment);
	static void Free(void *ptr);
};

class StackAllocator {
public:
	static void *Alloc(u64 size, int alignment);
	static void *Realloc(void *ptr, u64 oldSize, u64 newSize, int alignment);
	static void Free(void *ptr);
};

class TransientAllocator {
public:
	static void *Alloc(u64 size, int alignment);
};

class BuddyAllocator {
public:
	static void *Alloc(u64 size, int alignment);
	static void *Realloc(void *ptr, u64 oldSize, u64 newSize, int alignment);
	static void Free(void *ptr);
};

inline void *BuddyAllocHook(u64 size) { return BuddyAllocator::Alloc(size, 1); }
