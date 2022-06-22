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
void *FrameAlloc(u64 size);
void *FrameRealloc(void *ptr, u64 newSize);
void FrameFree(void *ptr);
void FrameWipe();
void *StackAlloc(u64 size);
void *StackRealloc(void *ptr, u64 newSize);
void StackFree(void *ptr);
void *TransientAlloc(u64 size);
void *BuddyFindFreeBlockOfOrder(u8 desiredOrder, u8 **bookkeep);
void *BuddyAlloc(u64 size);
u64 BuddyTryMerge(u64 blockIdx);
void BuddyFree(void *ptr);
