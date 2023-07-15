// @Cleanup: This whole thing is pretty dumb

void MemoryInit(Memory *memory)
{
	memory->framePtr = memory->frameMem;
	memory->stackPtr = memory->stackMem;
	memory->transientPtr = memory->transientMem;

	// Init buddy allocator
	const u32 maxOrder = Ntz(Memory::buddySize / Memory::buddySmallest);
	ASSERT(maxOrder < U8_MAX);
	memory->buddyBookkeep[0] = (u8)maxOrder;

#if DEBUG_BUILD
	memory->buddyMemoryUsage = 0;
#endif
}

// FRAME
void *FrameAllocator::Alloc(u64 size, int alignment)
{
	ASSERT((u8 *)g_memory->framePtr + size < (u8 *)g_memory->frameMem + Memory::frameSize); // Out of memory!

	u8 *result;

	result = (u8 *)g_memory->framePtr;

#if ENABLE_ALIGNMENT
	// Alignment
	ASSERT(IsPowerOf2(alignment));
	int alignmentMask = alignment - 1;

	u64 shift = 0;
	if (result & alignmentMask) shift = alignment;
	result += shift;
	result &= ~alignmentMask;
#endif

	g_memory->framePtr = result + size;

	return result;
}
void *FrameAllocator::Realloc(void *ptr, u64 oldSize, u64 newSize, int alignment)
{
	//Log("WARNING: FRAME REALLOC\n");

	void *newBlock = Alloc(newSize, alignment);
	memcpy(newBlock, ptr, oldSize);
	return newBlock;
}
void FrameAllocator::Free(void *ptr)
{
	(void) ptr;
}
void FrameWipe()
{
	g_memory->lastFrameUsage = (u8 *)g_memory->framePtr - (u8 *)g_memory->frameMem;
	g_memory->framePtr = g_memory->frameMem;
}

// STACK
void *StackAllocator::Alloc(u64 size, int alignment)
{
	ASSERT((u8 *)g_memory->stackPtr + size < (u8 *)g_memory->stackMem + Memory::stackSize); // Out of memory!

	u8 *result;

	result = (u8 *)g_memory->stackPtr;

#if ENABLE_ALIGNMENT
	// Alignment
	ASSERT(IsPowerOf2(alignment));
	int alignmentMask = alignment - 1;

	u64 shift = 0;
	if (result & alignmentMask) shift = alignment;
	result += shift;
	result &= ~alignmentMask;
#endif

	g_memory->stackPtr = result + size;

	return result;
}
void *StackAllocator::Realloc(void *ptr, u64 oldSize, u64 newSize, int alignment)
{
	//ASSERT(false);
	//Log("WARNING: STACK REALLOC\n");

	void *newBlock = Alloc(newSize, alignment);
	memcpy(newBlock, ptr, oldSize);
	return newBlock;
}
void StackAllocator::Free(void *ptr)
{
	g_memory->stackPtr = ptr;
}

// TRANSIENT
void *TransientAllocator::Alloc(u64 size, int alignment)
{
	ASSERT((u8 *)g_memory->transientPtr + size < (u8 *)g_memory->transientMem + Memory::transientSize); // Out of memory!

	u8 *result;

	result = (u8 *)g_memory->transientPtr;

#if ENABLE_ALIGNMENT
	// Alignment
	ASSERT(IsPowerOf2(alignment));
	int alignmentMask = alignment - 1;

	u64 shift = 0;
	if (result & alignmentMask) shift = alignment;
	result += shift;
	result &= ~alignmentMask;
#endif

	g_memory->transientPtr = result + size;

	return result;
}

void *BuddyFindFreeBlockOfOrder(u8 desiredOrder, u8 **bookkeep)
{
	// Look for block of right order
	u8 *end = g_memory->buddyBookkeep + Memory::buddySize / Memory::buddySmallest;
	u8 *bScan = g_memory->buddyBookkeep;
	u8 *mScan = (u8 *)g_memory->buddyMem;
	for (; bScan < end; )
	{
		bool inUse = *bScan & Memory::buddyUsedBit;
		u8 order = *bScan & ~Memory::buddyUsedBit;
		if (order == desiredOrder && !inUse)
		{
			*bookkeep = bScan;
			return mScan;
		}
		else
		{
			// Advance the size of block we are looking for, no need to visit the next smaller blocks
			const u8 higherOrder = Max(order, desiredOrder);
			bScan += (u64)1 << higherOrder;
			mScan += Memory::buddySmallest << higherOrder;
		}
	}
	// If there's no higher order there's no room!
	const u8 maxOrder = Ntz(Memory::buddySize / Memory::buddySmallest);
	if (desiredOrder >= maxOrder)
	{
		return nullptr;
	}
	// Otherwise find block of higher order and split
	void *block = BuddyFindFreeBlockOfOrder(desiredOrder + 1, bookkeep);
	if (block == nullptr)
		return nullptr;

	ASSERT((**bookkeep & Memory::buddyUsedBit) == 0);
	**bookkeep = desiredOrder;

	u8 *newBuddyBookkeep = ((*bookkeep) + ((u64)1 << desiredOrder));
	*newBuddyBookkeep = desiredOrder;
	return block;
}

void *BuddyAllocator::Alloc(u64 size, int alignment)
{
	u64 s = NextPowerOf264(size);
	u8 desiredOrder = Ntz64(s / Memory::buddySmallest); // @Speed: left shift instead of divide
	// Look for block of right order
	u8 *bookkeep;
	void *block = BuddyFindFreeBlockOfOrder(desiredOrder, &bookkeep);
	ASSERT(block); // Out of memory!
	if (block)
	{
		*bookkeep |= Memory::buddyUsedBit;
#if DEBUG_BUILD
		memset(block, 0xCCCC, Memory::buddySmallest << desiredOrder);
#endif
	}

#if DEBUG_BUILD
	g_memory->buddyMemoryUsage += s;
#endif

	return block;
}

u64 BuddyTryMerge(u64 blockIdx)
{
	u8 *bookkeep = &g_memory->buddyBookkeep[blockIdx];

	// Find buddy
	u8 order = *bookkeep;
	const u8 maxOrder = Ntz(Memory::buddySize / Memory::buddySmallest);
	if (order >= maxOrder)
		return blockIdx;

	u32 blockSize = 1 << order;
	u64 buddyIdx = blockIdx ^ blockSize;
	u8 *buddyBookkeep = &g_memory->buddyBookkeep[buddyIdx];
	bool buddyInUse = *buddyBookkeep & Memory::buddyUsedBit;

	// Dumb assert I guess, buy buddy should never be of higher order
	ASSERT((*buddyBookkeep & (~Memory::buddyUsedBit)) <= order);

	if (!buddyInUse && *buddyBookkeep == order)
	{
		// Merge
		u8 *first, *last;
		u64 firstIdx;
		if (blockIdx < buddyIdx)
		{
			firstIdx = blockIdx;
			first = bookkeep;
			last = buddyBookkeep;
		}
		else
		{
			firstIdx = buddyIdx;
			last = bookkeep;
			first = buddyBookkeep;
		}

		++*first;
#if DEBUG_BUILD
		// Visibly mark block as merged for debugging
		*last = 0xFF;
#endif

		return BuddyTryMerge(firstIdx);
	}
	return blockIdx;
}

void BuddyAllocator::Free(void *ptr)
{
	// Stupid ImGui
	if (ptr == nullptr)
		return;

	// Find bookkeep of this block
	u64 offset = (u8 *)ptr - (u8 *)g_memory->buddyMem;
	u64 bookkeepIdx = offset / Memory::buddySmallest;
	ASSERT(offset % Memory::buddySmallest == 0);
	u8 *bookkeep = &g_memory->buddyBookkeep[bookkeepIdx];

#if DEBUG_BUILD
	g_memory->buddyMemoryUsage -= Memory::buddySmallest << g_memory->buddyBookkeep[bookkeepIdx];
#endif

	ASSERT((*bookkeep & Memory::buddyUsedBit) != 0);
	*bookkeep &= ~Memory::buddyUsedBit;

	u64 mergedBlockIdx = BuddyTryMerge(bookkeepIdx);

#if DEBUG_BUILD
	u64 newSize = Memory::buddySmallest << g_memory->buddyBookkeep[mergedBlockIdx];
	void *newPtr = (u8 *)g_memory->buddyMem + mergedBlockIdx * Memory::buddySmallest;
	memset(newPtr, 0xCDCD, newSize);
#endif
}
