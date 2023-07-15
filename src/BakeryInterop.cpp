#include "BakeryInterop.h"

template <typename T, typename Allocator>
inline T* AllocAndCopy(u64 count, const u8 *fileBuffer, u64 offset)
{
	const u64 blobSize = sizeof(T) * count;
	void *mem = Allocator::Alloc(blobSize, alignof(T));
	memcpy(mem, fileBuffer + offset, blobSize);
	return (T*)mem;
}

void ReadMesh(const u8 *fileBuffer, Vertex **vertexData, u16 **indexData, u32 *vertexCount,
		u32 *indexCount, const char **materialFilename)
{
	BakeryMeshHeader *header = (BakeryMeshHeader *)fileBuffer;

	*vertexCount = header->vertexCount;
	*indexCount = header->indexCount;

	*vertexData = (Vertex *)(fileBuffer + header->vertexBlobOffset);
	*indexData = (u16 *)(fileBuffer + header->indexBlobOffset);

	*materialFilename = (const char *)(fileBuffer + header->materialNameOffset);
}

void ReadBakeryShader(const u8 *fileBuffer, const char **vertexShader, const char **fragmentShader)
{
	BakeryShaderHeader *header = (BakeryShaderHeader *)fileBuffer;
	*vertexShader = (const char *)(fileBuffer + header->vertexShaderBlobOffset);
	*fragmentShader = (const char *)(fileBuffer + header->fragmentShaderBlobOffset);
}

void ReadSkinnedMesh(const u8 *fileBuffer, ResourceSkinnedMesh *skinnedMesh, SkinnedVertex **vertexData,
		u16 **indexData, u32 *vertexCount, u32 *indexCount, const char **materialFilename)
{
	BakerySkinnedMeshHeader *header = (BakerySkinnedMeshHeader *)fileBuffer;

	*vertexCount = header->vertexCount;
	*indexCount = header->indexCount;

	*vertexData = (SkinnedVertex *)(fileBuffer + header->vertexBlobOffset);
	*indexData = (u16 *)(fileBuffer + header->indexBlobOffset);

	u32 jointCount = header->jointCount;

	// @Broken: Memory leak with resource reload!!!
	Transform* bindPoses = AllocAndCopy<Transform, TransientAllocator>(jointCount, fileBuffer, header->bindPosesBlobOffset);

	u8* jointParents = AllocAndCopy<u8, TransientAllocator>(jointCount, fileBuffer, header->jointParentsBlobOffset);

	Transform* restPoses = AllocAndCopy<Transform, TransientAllocator>(jointCount, fileBuffer, header->restPosesBlobOffset);

	ASSERT(jointCount < U8_MAX);
	skinnedMesh->jointCount = (u8)jointCount;
	skinnedMesh->bindPoses = bindPoses;
	skinnedMesh->jointParents = jointParents;
	skinnedMesh->restPoses = restPoses;

	const u32 animationCount = header->animationCount;
	skinnedMesh->animationCount = animationCount;

	const u64 animationsBlobSize = sizeof(Animation) * animationCount;
	skinnedMesh->animations = (Animation *)TransientAllocator::Alloc(animationsBlobSize, alignof(u64));

	BakerySkinnedMeshAnimationHeader *animationHeaders = (BakerySkinnedMeshAnimationHeader *)
		(fileBuffer + header->animationBlobOffset);
	for (u32 animIdx = 0; animIdx < animationCount; ++animIdx)
	{
		Animation *animation = &skinnedMesh->animations[animIdx];

		BakerySkinnedMeshAnimationHeader *animationHeader = &animationHeaders[animIdx];

		u32 frameCount = animationHeader->frameCount;
		u32 channelCount = animationHeader->channelCount;

		f32 *timestamps = AllocAndCopy<f32, TransientAllocator>(frameCount, fileBuffer, animationHeader->timestampsBlobOffset);

		animation->frameCount = frameCount;
		animation->timestamps = timestamps;
		animation->channelCount = channelCount;
		animation->loop = animationHeader->loop;
		animation->channels = ALLOC_N(TransientAllocator, AnimationChannel, channelCount);

		BakerySkinnedMeshAnimationChannelHeader *channelHeaders =
			(BakerySkinnedMeshAnimationChannelHeader *)(fileBuffer +
					animationHeader->channelsBlobOffset);
		for (u32 channelIdx = 0; channelIdx < channelCount; ++channelIdx)
		{
			AnimationChannel *channel = &animation->channels[channelIdx];

			BakerySkinnedMeshAnimationChannelHeader *channelHeader = &channelHeaders[channelIdx];

			u32 jointIndex = channelHeader->jointIndex;

			Transform *transforms = AllocAndCopy<Transform, TransientAllocator>(frameCount, fileBuffer, channelHeader->transformsBlobOffset);

			channel->jointIndex = jointIndex;
			channel->transforms = transforms;
		}
	}

	*materialFilename = (const char *)(fileBuffer + header->materialNameOffset);
}

void ReadTriangleGeometry(const u8 *fileBuffer, ResourceGeometryGrid *geometryGrid)
{
	BakeryTriangleDataHeader *header = (BakeryTriangleDataHeader *)fileBuffer;
	geometryGrid->lowCorner = header->lowCorner;
	geometryGrid->highCorner = header->highCorner;
	geometryGrid->cellsSide = header->cellsSide;
	geometryGrid->positionCount = header->positionCount;

	u32 offsetCount = header->cellsSide * header->cellsSide + 1;
	geometryGrid->offsets = AllocAndCopy<u32, TransientAllocator>(offsetCount, fileBuffer, header->offsetsBlobOffset);

	u32 positionCount = header->positionCount;
	geometryGrid->positions = AllocAndCopy<v3, TransientAllocator>(positionCount, fileBuffer, header->positionsBlobOffset);

	u32 triangleCount = geometryGrid->offsets[offsetCount - 1];
	geometryGrid->triangles = AllocAndCopy<IndexTriangle, TransientAllocator>(triangleCount, fileBuffer, header->trianglesBlobOffset);
}

void ReadCollisionMesh(const u8 *fileBuffer, ResourceCollisionMesh *collisionMesh)
{
	BakeryCollisionMeshHeader *header = (BakeryCollisionMeshHeader *)fileBuffer;

	u32 positionCount = header->positionCount;
	collisionMesh->positionCount = positionCount;
	collisionMesh->positionData = AllocAndCopy<v3, TransientAllocator>(positionCount, fileBuffer, header->positionsBlobOffset);

	u32 triangleCount = header->triangleCount;
	collisionMesh->triangleCount = triangleCount;
	collisionMesh->triangleData = AllocAndCopy<IndexTriangle, TransientAllocator>(triangleCount, fileBuffer, header->trianglesBlobOffset);
}

void ReadImage(const u8 *fileBuffer, const u8 **imageData, u32 *width, u32 *height, u32 *components)
{
	BakeryImageHeader *header = (BakeryImageHeader *)fileBuffer;

	*width = header->width;
	*height = header->height;
	*components = header->components;
	*imageData = fileBuffer + header->dataBlobOffset;
}

void ReadMaterial(const u8 *fileBuffer, RawBakeryMaterial *rawMaterial)
{
	BakeryMaterialHeader *header = (BakeryMaterialHeader *)fileBuffer;
	rawMaterial->shaderFilename = (const char *)fileBuffer + header->shaderNameOffset;
	rawMaterial->textureCount = (u8)header->textureCount;

	const char *currentTexName = (const char *)fileBuffer + header->textureNamesOffset;
	for (u8 texIdx = 0; texIdx < header->textureCount; ++texIdx)
	{
		rawMaterial->textureFilenames[texIdx] = currentTexName;
		currentTexName += strlen(currentTexName) + 1;
	}
}
