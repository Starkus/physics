#define EPA_LOGGING 0
#define EPA_ERROR_LOGGING 1

#if DEBUG_BUILD && defined(USING_IMGUI)
#define VERBOSE_LOG(...) if (g_debugContext->verboseCollisionLogging) Log(__VA_ARGS__)
#else
#define VERBOSE_LOG(...)
#endif

struct GJKPoint
{
	v3 dif; // we store a-b instead of b since it's more useful.
	v3 a;
};

struct GJKResult
{
	bool hit;
	GJKPoint points[4];
};

struct EPAFace
{
	GJKPoint a;
	GJKPoint b;
	GJKPoint c;
};

struct EPAEdge
{
	GJKPoint a;
	GJKPoint b;
};

struct CollisionInfo
{
	bool hit;
	v3 hitPoint;
	v3 depenetrationVector;
};

#if DEBUG_BUILD
void GenPolytopeMesh(EPAFace *polytopeData, int faceCount, DebugVertex *outputBuffer, int *vertexCount)
{
	*vertexCount = 0;
	for (int faceIdx = 0; faceIdx < faceCount; ++faceIdx)
	{
		EPAFace *face = &polytopeData[faceIdx];
		v3 normal = V3Normalize(V3Cross(face->c.dif - face->a.dif, face->b.dif - face->a.dif));
		normal = normal * 0.5f + v3{ 0.5f, 0.5f, 0.5f };
		outputBuffer[(*vertexCount)++] = { face->a.a, normal };
		outputBuffer[(*vertexCount)++] = { face->b.a, normal };
		outputBuffer[(*vertexCount)++] = { face->c.a, normal };

		outputBuffer[(*vertexCount)++] = { face->a.dif, normal };
		outputBuffer[(*vertexCount)++] = { face->b.dif, normal };
		outputBuffer[(*vertexCount)++] = { face->c.dif, normal };
	}
}
#endif

bool RayTriangleIntersection(v3 rayOrigin, v3 rayDir, bool infinite, const Triangle *triangle, v3 *hit)
{
	const v3 &a = triangle->a;
	const v3 &b = triangle->b;
	const v3 &c = triangle->c;
	const v3 &nor = triangle->normal;

	const v3 ab = b - a;
	const v3 bc = c - b;
	const v3 ca = a - c;

	f32 rayDistAlongNormal = V3Dot(rayDir, -nor);
	const f32 epsilon = 0.000001f;
	if (rayDistAlongNormal > -epsilon && rayDistAlongNormal < epsilon)
		// Perpendicular
		return false;

	f32 aDistAlongNormal = V3Dot(a - rayOrigin, -nor);
	f32 factor = aDistAlongNormal / rayDistAlongNormal;

	if (factor < 0 || (!infinite && factor > 1))
		return false;

	v3 rayPlaneInt = rayOrigin + rayDir * factor;

	// Barycentric coordinates
	{
		const v3 projABOntoBC = bc * (V3Dot(ab, bc) / V3Dot(bc, bc));
		const v3 v = ab - projABOntoBC;
		if (V3Dot(v, ab) < V3Dot(v, rayPlaneInt - a))
			return false;
	}

	{
		const v3 projBCOntoCA = ca * (V3Dot(bc, ca) / V3Dot(ca, ca));
		const v3 v = bc - projBCOntoCA;
		if (V3Dot(v, bc) < V3Dot(v, rayPlaneInt - b))
			return false;
	}

	{
		const v3 projCAOntoAB = ab * (V3Dot(ca, ab) / V3Dot(ab, ab));
		const v3 v = ca - projCAOntoAB;
		if (V3Dot(v, ca) < V3Dot(v, rayPlaneInt - c))
			return false;
	}

	*hit = rayPlaneInt;
	return true;
}

v3 BarycentricCoordinates(Triangle *triangle, v3 p)
{
	// Fast barycentric function stolen from Real-Time Collision Detection by Christer Ericson
	v3 result;
	v3 AB = triangle->b - triangle->a;
	v3 AC = triangle->c - triangle->a;
	v3 AP = p - triangle->a;
	float dot00 = V3Dot(AB, AB);
	float dot01 = V3Dot(AB, AC);
	float dot11 = V3Dot(AC, AC);
	float dot20 = V3Dot(AP, AB);
	float dot21 = V3Dot(AP, AC);
	float denom = dot00 * dot11 - dot01 * dot01;
	result.y = (dot11 * dot20 - dot01 * dot21) / denom;
	result.z = (dot00 * dot21 - dot01 * dot20) / denom;
	result.x = 1.0f - result.y - result.z;
	return result;
}

bool HitTest_CheckCell(GameState *gameState, int cellX, int cellY, bool swapXY, v3 rayOrigin,
		v3 rayDir, bool infinite, v3 *hit, Triangle *triangle)
{
	if (swapXY)
	{
		int tmp = cellX;
		cellX = cellY;
		cellY = tmp;
	}

	const ResourceGeometryGrid *geometryGrid = &gameState->levelGeometry.geometryGrid->geometryGrid;
	v2 cellSize = (geometryGrid->highCorner - geometryGrid->lowCorner) / (f32)(geometryGrid->cellsSide);

	if (cellX < 0 || cellX >= geometryGrid->cellsSide ||
		cellY < 0 || cellY >= geometryGrid->cellsSide)
		return false;

	bool result = false;
	v3 closestHit = {};
	Triangle closestTriangle = {};
	f32 closestSqrLen = INFINITY;

	int offsetIdx = cellX + cellY * geometryGrid->cellsSide;
	u32 triBegin = geometryGrid->offsets[offsetIdx];
	u32 triEnd = geometryGrid->offsets[offsetIdx + 1];
	for (u32 triIdx = triBegin; triIdx < triEnd; ++triIdx)
	{
		IndexTriangle *curTriangle = &geometryGrid->triangles[triIdx];
		Triangle tri =
		{
			geometryGrid->positions[curTriangle->a],
			geometryGrid->positions[curTriangle->b],
			geometryGrid->positions[curTriangle->c],
			curTriangle->normal
		};

#if HITTEST_VISUAL_DEBUG
		Vertex tri[] =
		{
			{tri.a, {}, {}},
			{tri.b, {}, {}},
			{tri.c, {}, {}}
		};
		DrawDebugTriangles(gameState, tri, 3);
#endif

		v3 thisHit;
		if (RayTriangleIntersection(rayOrigin, rayDir, infinite, &tri, &thisHit))
		{
			f32 sqrLen = V3SqrLen(thisHit - rayOrigin);
			if (sqrLen < closestSqrLen)
			{
				result = true;
				closestHit = thisHit;
				closestTriangle = tri;
				closestSqrLen = sqrLen;
			}
		}
	}

#if HITTEST_VISUAL_DEBUG
	f32 cellMinX = geometryGrid->lowCorner.x + (cellX) * cellSize.x;
	f32 cellMaxX = cellMinX + cellSize.x;
	f32 cellMinY = geometryGrid->lowCorner.y + (cellY) * cellSize.y;
	f32 cellMaxY = cellMinY + cellSize.y;
	f32 top = 1;
	Vertex cellDebugTris[] =
	{
		{ { cellMinX, cellMinY, -1 }, {}, {} },
		{ { cellMinX, cellMinY, top }, {}, {} },
		{ { cellMaxX, cellMinY, -1 }, {}, {} },

		{ { cellMaxX, cellMinY, -1 }, {}, {} },
		{ { cellMinX, cellMinY, top }, {}, {} },
		{ { cellMaxX, cellMinY, top }, {}, {} },

		{ { cellMinX, cellMinY, -1 }, {}, {} },
		{ { cellMinX, cellMinY, top }, {}, {} },
		{ { cellMinX, cellMaxY, -1 }, {}, {} },

		{ { cellMinX, cellMaxY, -1 }, {}, {} },
		{ { cellMinX, cellMinY, top }, {}, {} },
		{ { cellMinX, cellMaxY, top }, {}, {} },

		{ { cellMinX, cellMaxY, -1 }, {}, {} },
		{ { cellMinX, cellMaxY, top }, {}, {} },
		{ { cellMaxX, cellMaxY, -1 }, {}, {} },

		{ { cellMaxX, cellMaxY, -1 }, {}, {} },
		{ { cellMinX, cellMaxY, top }, {}, {} },
		{ { cellMaxX, cellMaxY, top }, {}, {} },

		{ { cellMaxX, cellMinY, -1 }, {}, {} },
		{ { cellMaxX, cellMinY, top }, {}, {} },
		{ { cellMaxX, cellMaxY, -1 }, {}, {} },

		{ { cellMaxX, cellMaxY, -1 }, {}, {} },
		{ { cellMaxX, cellMinY, top }, {}, {} },
		{ { cellMaxX, cellMaxY, top }, {}, {} },
	};
	DrawDebugTriangles(gameState, cellDebugTris, 24);
#endif

	if (result)
	{
		*hit = closestHit;
		*triangle = closestTriangle;
	}
	return result;
}

bool HitTest(GameState *gameState, v3 rayOrigin, v3 rayDir, bool infinite, v3 *hit, Triangle *triangle)
{
	const ResourceGeometryGrid *geometryGrid = &gameState->levelGeometry.geometryGrid->geometryGrid;
	v2 cellSize = (geometryGrid->highCorner - geometryGrid->lowCorner) / (f32)(geometryGrid->cellsSide);

#if HITTEST_VISUAL_DEBUG
	auto ddraw = [&gameState, &cellSize, &geometryGrid](f32 relX, f32 relY)
	{
		DrawDebugCubeAA(gameState, v3{
				relX * cellSize.x + geometryGrid->lowCorner.x,
				relY * cellSize.y + geometryGrid->lowCorner.y,
				1}, 0.1f);
	};
#else
#define ddraw(...)
#endif

	v2 p1 = { rayOrigin.x, rayOrigin.y };
	v2 p2 = { rayOrigin.x + rayDir.x, rayOrigin.y + rayDir.y };
	p1 = p1 - geometryGrid->lowCorner;
	p2 = p2 - geometryGrid->lowCorner;
	p1.x /= cellSize.x;
	p1.y /= cellSize.y;
	p2.x /= cellSize.x;
	p2.y /= cellSize.y;

	f32 slope = (p2.y - p1.y) / (p2.x - p1.x);
	bool swapXY = slope > 1 || slope < -1;
	if (swapXY)
	{
		f32 tmp = p1.x;
		p1.x = p1.y;
		p1.y = tmp;
		tmp = p2.x;
		p2.x = p2.y;
		p2.y = tmp;
		slope = 1.0f / slope;
	}
	bool flipX = p1.x > p2.x;
	if (flipX)
	{
		slope = -slope;
	}
	f32 xSign = 1.0f - 2.0f * flipX;

	f32 curX = p1.x;
	f32 curY = p1.y;

	if (HitTest_CheckCell(gameState, (int)Floor(curX), (int)Floor(curY), swapXY, rayOrigin, rayDir,
				infinite, hit, triangle))
		return true;

	int oldY = (int)Floor(curY);
	for (bool done = false; !done; )
	{
		curX += xSign;
		curY += slope;
		int newY = (int)Floor(curY);

		if ((!flipX && curX > p2.x) || (flipX && curX < p2.x))
		{
			curX = p2.x;
			curY = p2.y;
			done = true;
		}

		if (!swapXY)
			ddraw(curX, curY);
		else
			ddraw(curY, curX);

		if (newY != oldY)
		{
			// When we go to a new row, check whether we come from the side or the top by projecting
			// the current point to the left border of the current cell. If the row of the projected
			// point is different than the current one, then we come from below.
			f32 cellRelX = Fmod(curX, 1.0f) - flipX;
			f32 projY = curY - cellRelX * slope * xSign;

			if (!swapXY)
				ddraw(curX - cellRelX, projY);
			else
				ddraw(projY, curX - cellRelX);

			if ((int)Floor(projY) != newY)
			{
				// Paint cell below
				if (HitTest_CheckCell(gameState, (int)Floor(curX), (int)Floor(projY), swapXY,
							rayOrigin, rayDir, infinite, hit, triangle))
					return true;
			}
			else
			{
				// Paint cell behind
				if (HitTest_CheckCell(gameState, (int)Floor(curX - xSign), (int)Floor(curY), swapXY,
							rayOrigin, rayDir, infinite, hit, triangle))
					return true;
			}
		}

		if (HitTest_CheckCell(gameState, (int)Floor(curX), (int)Floor(curY), swapXY, rayOrigin,
					rayDir, infinite, hit, triangle))
			return true;

		oldY = newY;
	}

	return false;
}

// @Speed: is 'infinite' bool slow here? Branch should be predictable across many similar ray tests
bool RayColliderIntersection(v3 rayOrigin, v3 rayDir, bool infinite, const Transform *transform,
		const Collider *c, v3 *hit, v3 *hitNor)
{
	bool result = false;

	ColliderType type = c->type;

		rayOrigin = rayOrigin - transform->translation;
	if (type != COLLIDER_SPHERE)
	{
		// Un-rotate direction
		v4 invQ = transform->rotation;
		invQ.w = -invQ.w;
		rayDir = QuaternionRotateVector(invQ, rayDir);
		rayOrigin = QuaternionRotateVector(invQ, rayOrigin);
	}

	v3 unitDir = V3Normalize(rayDir);

	switch(type)
	{
	case COLLIDER_CONVEX_HULL:
	{
		const Resource *res = c->convexHull.meshRes;
		if (!res)
			return false;
		const ResourceCollisionMesh *collMeshRes = &res->collisionMesh;

		const u32 triangleCount = collMeshRes->triangleCount;
		const v3 *positions = collMeshRes->positionData;

		for (u32 triangleIdx = 0; triangleIdx < triangleCount; ++triangleIdx)
		{
			const IndexTriangle *indexTri = &collMeshRes->triangleData[triangleIdx];
			Triangle tri =
			{
				positions[indexTri->a] * c->convexHull.scale,
				positions[indexTri->b] * c->convexHull.scale,
				positions[indexTri->c] * c->convexHull.scale,
				indexTri->normal
			};

			if (V3Dot(rayDir, tri.normal) > 0)
				continue;

			v3 currHit;
			if (RayTriangleIntersection(rayOrigin, rayDir, infinite, &tri, &currHit))
			{
				*hit = currHit;
				*hitNor = tri.normal;
				result = true;
			}
		}
	} break;
	case COLLIDER_CUBE:
	{
		v3 center = c->cube.offset;
		f32 radius = c->cube.radius;

		// Check once for each axis
		for (int i = 0; i < 3; ++i)
		{
			// Use other two axes to check boundaries
			int j = (i + 1) % 3;
			int k = (i + 2) % 3;

			// Check only face looking at ray origin
			f32 top = center.v[i] - c->cube.radius * Sign(rayDir.v[i]);
			f32 dist = top - rayOrigin.v[i];
			if (dist * Sign(rayDir.v[i]) > 0)
			{
				// Check top/bottom face
				f32 factor = dist / rayDir.v[i];
				v3 proj = rayOrigin + rayDir * factor;

				v3 opposite = proj - center;
				if (opposite.v[j] >= center.v[j] - radius && opposite.v[j] <= center.v[j] + radius &&
					opposite.v[k] >= center.v[k] - radius && opposite.v[k] <= center.v[k] + radius)
				{
					// Limit reach
					if (factor < 0 || (!infinite && factor > 1))
						return false;

					*hit = proj;
					*hitNor = {};
					hitNor->v[i] = -Sign(rayDir.v[i]);
					result = true;
					break;
				}
			}
		}
	} break;
	case COLLIDER_SPHERE:
	{
		v3 center = c->sphere.offset;

		v3 towards = center - rayOrigin;
		f32 dot = V3Dot(unitDir, towards);
		if (dot <= 0)
			return false;

		v3 proj = rayOrigin + unitDir * dot;

		f32 distSqr = V3SqrLen(proj - center);
		f32 radiusSqr = c->sphere.radius * c->sphere.radius;
		if (distSqr > radiusSqr)
			return false;

		f32 td = Sqrt(radiusSqr - distSqr);
		*hit = proj - unitDir * td;

		// Limit reach
		if (!infinite && V3SqrLen(*hit - rayOrigin) > V3SqrLen(rayDir))
			return false;

		*hitNor = (*hit - center) / c->sphere.radius;
		result = true;
	} break;
	case COLLIDER_CYLINDER:
	{
		v3 center = c->cylinder.offset;

		v3 towards = center - rayOrigin;
		towards.z = 0;

		f32 dirLat = V2Length(unitDir.xy);
		v3 dirNormXY = unitDir / dirLat;
		v3 dirNormZ = unitDir / unitDir.z;
		f32 radiusSqr = c->cylinder.radius * c->cylinder.radius;

		// This is bottom instead of top when ray points upwards
		f32 top = center.z - c->cylinder.height * 0.5f * Sign(rayDir.z);
		f32 distZ = top - rayOrigin.z;
		if (distZ * Sign(rayDir.z) > 0)
		{
			// Check top/bottom face
			f32 factor = distZ / rayDir.z;
			v3 proj = rayOrigin + rayDir * factor;

			v2 opposite = proj.xy - center.xy;
			f32 distSqr = V2SqrLen(opposite);
			if (distSqr <= radiusSqr)
			{
				// Limit reach
				if (factor < 0 || (!infinite && factor > 1))
					return false;

				*hit = proj;
				*hitNor = { 0, 0, -Sign(rayDir.z) };
				result = true;
			}
		}

		// Otherwise check cylinder wall
		if (!result)
		{
			f32 dot = V3Dot(dirNormXY, towards);
			if (dot <= c->cylinder.radius)
				return false;

			v3 proj = rayOrigin + dirNormXY * dot;

			v3 opposite = proj - center;
			f32 distSqr = (opposite.x * opposite.x) + (opposite.y * opposite.y);
			if (distSqr > radiusSqr)
				return false;

			f32 td = Sqrt(radiusSqr - distSqr);
			proj = proj - dirNormXY * td;

			f32 distUp = Abs(proj.z - center.z);
			if (distUp > c->cylinder.height * 0.5f)
				return false;

			*hit = proj;

			// Limit reach
			if (!infinite && V3SqrLen(*hit - rayOrigin) > V3SqrLen(rayDir))
				return false;

			*hitNor = (*hit - center) / c->cylinder.radius;
			hitNor->z = 0;
			result = true;
		}
	} break;
	case COLLIDER_CAPSULE:
	{
		v3 center = c->capsule.offset;

		// Lower sphere
		v3 sphereCenter = center;
		sphereCenter.z -= c->capsule.height * 0.5f;

		v3 towards = sphereCenter - rayOrigin;
		f32 dot = V3Dot(unitDir, towards);
		if (dot > 0)
		{
			v3 proj = rayOrigin + unitDir * dot;

			f32 distSqr = V3SqrLen(proj - sphereCenter);
			f32 radiusSqr = c->capsule.radius * c->capsule.radius;
			if (distSqr <= radiusSqr)
			{
				f32 td = Sqrt(radiusSqr - distSqr);
				v3 sphereProj = proj - unitDir * td;
				if (sphereProj.z <= sphereCenter.z)
				{
					*hit = sphereProj;

					// Limit reach
					if (!infinite && V3SqrLen(*hit - rayOrigin) > V3SqrLen(rayDir))
						return false;

					*hitNor = (*hit - sphereCenter) / c->capsule.radius;
					result = true;
				}
			}
		}

		// Upper sphere
		if (!result)
		{
			sphereCenter.z += c->capsule.height;
			towards = sphereCenter - rayOrigin;
			dot = V3Dot(unitDir, towards);
			if (dot > 0)
			{
				v3 proj = rayOrigin + unitDir * dot;

				f32 distSqr = V3SqrLen(proj - sphereCenter);
				f32 radiusSqr = c->capsule.radius * c->capsule.radius;
				if (distSqr <= radiusSqr)
				{
					f32 td = Sqrt(radiusSqr - distSqr);
					v3 sphereProj = proj - unitDir * td;
					if (sphereProj.z >= sphereCenter.z)
					{
						*hit = sphereProj;

						// Limit reach
						if (!infinite && V3SqrLen(*hit - rayOrigin) > V3SqrLen(rayDir))
							return false;

						*hitNor = (*hit - sphereCenter) / c->capsule.radius;
						result = true;
					}
				}
			}
		}

		// Side
		if (!result)
		{
			towards = center - rayOrigin;
			towards.z = 0;

			f32 dirLat = Sqrt(unitDir.x * unitDir.x + unitDir.y * unitDir.y);
			v3 dirNormXY = unitDir / dirLat;
			f32 radiusSqr = c->capsule.radius * c->capsule.radius;

			dot = V3Dot(dirNormXY, towards);
			if (dot <= c->capsule.radius)
				return false;

			v3 proj = rayOrigin + dirNormXY * dot;

			v3 opposite = proj - center;
			f32 distSqr = (opposite.x * opposite.x) + (opposite.y * opposite.y);
			if (distSqr > radiusSqr)
				return false;

			f32 td = Sqrt(radiusSqr - distSqr);
			proj = proj - dirNormXY * td;

			f32 distUp = Abs(proj.z - center.z);
			if (distUp > c->capsule.height * 0.5f)
				return false;

			*hit = proj;

			// Limit reach
			if (!infinite && V3SqrLen(*hit - rayOrigin) > V3SqrLen(rayDir))
				return false;

			*hitNor = (*hit - center) / c->capsule.radius;
			hitNor->z = 0;
			result = true;
		}
	} break;
	default:
	{
		ASSERT(false);
	}
	}

	if (result)
	{
		if (type != COLLIDER_SPHERE)
		{
			*hit = QuaternionRotateVector(transform->rotation, *hit);
			*hitNor = QuaternionRotateVector(transform->rotation, *hitNor);
		}
		*hit += transform->translation;
	}

	return result;
}

void GetAABB(Transform *transform, Collider *c, v3 *min, v3 *max)
{
	ColliderType type = c->type;
	switch(type)
	{
	case COLLIDER_CONVEX_HULL:
	{
		*min = { INFINITY, INFINITY, INFINITY };
		*max = { -INFINITY, -INFINITY, -INFINITY };

		const Resource *res = c->convexHull.meshRes;
		if (!res)
			return;
		const ResourceCollisionMesh *collMeshRes = &res->collisionMesh;

		u32 pointCount = collMeshRes->positionCount;

		for (u32 i = 0; i < pointCount; ++i)
		{
			v3 v = collMeshRes->positionData[i];
			v = QuaternionRotateVector(transform->rotation, v);

			if (v.x < min->x) min->x = v.x;
			if (v.y < min->y) min->y = v.y;
			if (v.z < min->z) min->z = v.z;
			if (v.x > max->x) max->x = v.x;
			if (v.y > max->y) max->y = v.y;
			if (v.z > max->z) max->z = v.z;
		}

		*min *= c->convexHull.scale;
		*max *= c->convexHull.scale;
	} break;
	case COLLIDER_CUBE:
	{
		f32 r = c->cube.radius;
		*min = c->cube.offset - v3{ r, r, r };
		*max = c->cube.offset + v3{ r, r, r };
	} break;
	case COLLIDER_SPHERE:
	{
		f32 r = c->sphere.radius;
		*min = c->sphere.offset - v3{ r, r, r };
		*max = c->sphere.offset + v3{ r, r, r };
	} break;
	case COLLIDER_CYLINDER:
	{
		f32 halfH = c->cylinder.height * 0.5f;
		f32 r = c->cylinder.radius;
		*min = c->cylinder.offset - v3{ r, r, halfH };
		*max = c->cylinder.offset + v3{ r, r, halfH };
	} break;
	case COLLIDER_CAPSULE:
	{
		f32 halfH = c->cylinder.height * 0.5f;
		f32 r = c->cylinder.radius;
		*min = c->cylinder.offset - v3{ r, r, halfH + r };
		*max = c->cylinder.offset + v3{ r, r, halfH + r };
	} break;
	default:
	{
		ASSERT(false);
	}
	}

	if (type != COLLIDER_SPHERE && type != COLLIDER_CONVEX_HULL)
	{
		v3 corners[] =
		{
			{ min->x, min->y, min->z },
			{ max->x, min->y, min->z },
			{ min->x, max->y, min->z },
			{ max->x, max->y, min->z },
			{ min->x, min->y, max->z },
			{ max->x, min->y, max->z },
			{ min->x, max->y, max->z },
			{ max->x, max->y, max->z },
		};
		for (int i = 0; i < 8; ++i)
		{
			corners[i] = QuaternionRotateVector(transform->rotation, corners[i]);
			if (corners[i].x < min->x) min->x = corners[i].x;
			if (corners[i].y < min->y) min->y = corners[i].y;
			if (corners[i].z < min->z) min->z = corners[i].z;
			if (corners[i].x > max->x) max->x = corners[i].x;
			if (corners[i].y > max->y) max->y = corners[i].y;
			if (corners[i].z > max->z) max->z = corners[i].z;
		}
	}

	*min += transform->translation;
	*max += transform->translation;

#if DEBUG_BUILD
	if (g_debugContext->drawAABBs)
	{
		DrawDebugWiredBox(*min, *max);
	}
#endif
}

v3 FurthestInDirection(Transform *transform, Collider *c, v3 dir)
{
	v3 result = {};

	ColliderType type = c->type;

	// Un-rotate direction
	v4 invQ = transform->rotation;
	invQ.w = -invQ.w;
	v3 locDir = QuaternionRotateVector(invQ, dir);

	switch(type)
	{
	case COLLIDER_CONVEX_HULL:
	{
		f32 maxDist = -INFINITY;

		const Resource *res = c->convexHull.meshRes;
		if (!res)
			return {};
		const ResourceCollisionMesh *collMeshRes = &res->collisionMesh;

		const u32 pointCount = collMeshRes->positionCount;
		const v3 *scan = collMeshRes->positionData;
		for (u32 i = 0; i < pointCount; ++i)
		{
			v3 p = *scan++;
			f32 dot = V3Dot(p, locDir);
			if (dot > maxDist)
			{
				maxDist = dot;
				result = p;
			}
		}

		result *= c->convexHull.scale;

		// Transform point to world coordinates and return as result
		result = QuaternionRotateVector(transform->rotation, result);
		result += transform->translation;
	} break;
	case COLLIDER_CUBE:
	{
		f32 r = c->cube.radius;
		result = c->cube.offset;
		if (locDir.x)
			result.x += Sign(locDir.x) * r;
		if (locDir.y)
			result.y += Sign(locDir.y) * r;
		if (locDir.z)
			result.z += Sign(locDir.z) * r;

		// Transform point to world coordinates and return as result
		result = QuaternionRotateVector(transform->rotation, result);
		result += transform->translation;
	} break;
	case COLLIDER_SPHERE:
	{
		result = transform->translation + c->sphere.offset + V3Normalize(dir) * c->sphere.radius;
	} break;
	case COLLIDER_CYLINDER:
	{
		result = c->cylinder.offset;

		f32 halfH = c->cylinder.height * 0.5f;
		if (locDir.z != 0)
		{
			result.z += Sign(locDir.z) * halfH;
		}
		if (locDir.x != 0 || locDir.y != 0)
		{
			f32 lat = Sqrt(locDir.x * locDir.x + locDir.y * locDir.y);
			result.x += locDir.x / lat * c->cylinder.radius;
			result.y += locDir.y / lat * c->cylinder.radius;
		}

		// Transform point to world coordinates and return as result
		result = QuaternionRotateVector(transform->rotation, result);
		result += transform->translation;
	} break;
	case COLLIDER_CAPSULE:
	{
		result = c->cylinder.offset;

		f32 halfH = c->capsule.height * 0.5f;
		f32 lat = Sqrt(locDir.x * locDir.x + locDir.y * locDir.y);
		if (lat == 0)
		{
			// If direction is parallel to cylinder the answer is trivial
			result.z += Sign(locDir.z) * (halfH + c->capsule.radius);
		}
		else
		{
			// Project locDir into cylinder wall
			v3 d = locDir / lat;
			d = d * c->capsule.radius;

			// If it goes out the top, return furthest point from top sphere
			if (d.z > 0)
				result += v3{ 0, 0, halfH } + V3Normalize(locDir) * c->capsule.radius;
			// Analogue for bottom
			else
				result += v3{ 0, 0, -halfH } + V3Normalize(locDir) * c->capsule.radius;
		}

		// Transform point to world coordinates and return as result
		result = QuaternionRotateVector(transform->rotation, result);
		result += transform->translation;
	} break;
	default:
	{
		ASSERT(false);
	}
	}

#if DEBUG_BUILD
	if (g_debugContext->drawSupports)
		DrawDebugCubeAA(result, 0.04f, {0,1,1});
#endif

	return result;
}

inline GJKPoint GJKSupport(Transform *transformA, Transform *transformB, Collider *colliderA,
		Collider *colliderB, v3 dir)
{
	ASSERT(dir.x != 0 || dir.y != 0 || dir.z != 0);
	v3 a = FurthestInDirection(transformA, colliderA, dir);
	v3 b = FurthestInDirection(transformB, colliderB, -dir);
	return { a - b, a };
}

GJKResult GJKTest(Transform *transformA, Transform *transformB, Collider *colliderA,
		Collider *colliderB)
{
#if DEBUG_BUILD
	if (g_debugContext->GJKSteps[0] == nullptr)
	{
		for (u32 i = 0; i < ArrayCount(g_debugContext->GJKSteps); ++i)
			g_debugContext->GJKSteps[i] = ALLOC_N(TransientAllocator, DebugVertex, 12);
	}
#endif

	GJKResult result;
	result.hit = true;

	v3 minA, maxA;
	GetAABB(transformA, colliderA, &minA, &maxA);
	v3 minB, maxB;
	GetAABB(transformB, colliderB, &minB, &maxB);
	if ((minA.x >= maxB.x || minB.x >= maxA.x) ||
		(minA.y >= maxB.y || minB.y >= maxA.y) ||
		(minA.z >= maxB.z || minB.z >= maxA.z))
	{
		result.hit = false;
		return result;
	}

	int foundPointsCount = 1;
	v3 testDir = { 0, 1, 0 }; // Random initial test direction

	result.points[0] = GJKSupport(transformB, transformA, colliderB, colliderA, testDir); // @Check: why are these in reverse order?
	testDir = -result.points[0].dif;

	for (int iterations = 0; result.hit && foundPointsCount < 4; ++iterations)
	{
#if DEBUG_BUILD
		int i_ = iterations;
		if (!g_debugContext->freezeGJKGeom)
		{
			g_debugContext->GJKStepsVertexCounts[i_] = 0;
			g_debugContext->gjkStepCount = i_ + 1;
		}
#endif

		if (iterations >= 20)
		{
			//Log("ERROR! GJK: Reached iteration limit!\n");
			//ASSERT(false);
#if DEBUG_BUILD
			g_debugContext->freezeGJKGeom = true;
#endif
			result.hit = false;
			break;
		}

		GJKPoint a = GJKSupport(transformB, transformA, colliderB, colliderA, testDir);
		if (V3Dot(testDir, a.dif) <= 0)
		{
			result.hit = false;
			break;
		}

#if DEBUG_BUILD
		if ((!g_debugContext->freezeGJKGeom) && iterations)
			g_debugContext->GJKNewPoint[iterations - 1] = a.a;
#endif

		switch (foundPointsCount)
		{
			case 1: // Line
			{
				GJKPoint b = result.points[0];
				v3 ab = b.dif - a.dif;

				result.points[foundPointsCount++] = a;
				testDir = V3Cross(V3Cross(ab, -a.dif), ab);
				if (testDir.x == 0 && testDir.y == 0 && testDir.z == 0)
				{
					// If -a and ab are colinear, just pick some direction perpendicular to ab.
					testDir = { ab.y - ab.z, -ab.x + ab.z, ab.x - ab.y };
				}
			} break;
			case 2: // Plane
			{
				GJKPoint b = result.points[1];
				GJKPoint c = result.points[0];

				v3 ab = b.dif - a.dif;
				v3 ac = c.dif - a.dif;
				v3 nor = V3Cross(ac, ab);
				v3 abNor = V3Cross(nor, ab);
				v3 acNor = V3Cross(ac, nor);

#if DEBUG_BUILD
				if (!g_debugContext->freezeGJKGeom)
				{
					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ a.a, v3{1,0,0} };
					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ b.a, v3{1,0,0} };
					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ c.a, v3{1,0,0} };
					ASSERT(g_debugContext->GJKStepsVertexCounts[i_] == 3);
				}
#endif

				if (V3Dot(acNor, -a.dif) > 0)
				{
					result.points[0] = a;
					result.points[1] = c;
					testDir = V3Cross(V3Cross(ac, -a.dif), ac);
				}
				else if (V3Dot(abNor, -a.dif) > 0)
				{
					result.points[0] = a;
					result.points[1] = b;
					testDir = V3Cross(V3Cross(ab, -a.dif), ab);
				}
				else
				{
					if (V3Dot(nor, -a.dif) > 0)
					{
						result.points[foundPointsCount++] = a;
						testDir = nor;
					}
					else
					{
						result.points[0] = b;
						result.points[1] = c;
						result.points[2] = a;
						++foundPointsCount;
						testDir = -nor;
					}

					// Assert triangle is wound clockwise
					v3 A = result.points[2].dif;
					v3 B = result.points[1].dif;
					v3 C = result.points[0].dif;
					ASSERT(V3Dot(testDir, V3Cross(C - A, B - A)) >= 0);
				}
			} break;
			case 3: // Tetrahedron
			{
				GJKPoint b = result.points[2];
				GJKPoint c = result.points[1];
				GJKPoint d = result.points[0];

				v3 ab = b.dif - a.dif;
				v3 ac = c.dif - a.dif;
				v3 ad = d.dif - a.dif;

				v3 abcNor = V3Cross(ac, ab);
				v3 adbNor = V3Cross(ab, ad);
				v3 acdNor = V3Cross(ad, ac);

#if DEBUG_BUILD
				if (!g_debugContext->freezeGJKGeom)
				{
					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ b.a, v3{1,0,0} };
					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ d.a, v3{1,0,0} };
					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ c.a, v3{1,0,0} };

					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ a.a, v3{0,1,0} };
					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ b.a, v3{0,1,0} };
					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ c.a, v3{0,1,0} };

					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ a.a, v3{0,0,1} };
					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ d.a, v3{0,0,1} };
					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ b.a, v3{0,0,1} };

					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ a.a, v3{1,0,1} };
					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ c.a, v3{1,0,1} };
					g_debugContext->GJKSteps[i_][g_debugContext->GJKStepsVertexCounts[i_]++] =
						{ d.a, v3{1,0,1} };

					ASSERT(g_debugContext->GJKStepsVertexCounts[i_] == 3 * 4);
				}
#endif

				// Assert normals point outside
				if (V3Dot(d.dif - a.dif, abcNor) > 0)
				{
					Log("ERROR: ABC normal facing inward! (dot=%f)\n",
							V3Dot(d.dif - a.dif, abcNor));
					ASSERT(false);
				}
				if (V3Dot(c.dif - a.dif, adbNor) > 0)
				{
					Log("ERROR: ADB normal facing inward! (dot=%f)\n",
							V3Dot(c.dif - a.dif, adbNor));
					ASSERT(false);
				}
				if (V3Dot(b.dif - a.dif, acdNor) > 0)
				{
					Log("ERROR: ACD normal facing inward! (dot=%f)\n",
							V3Dot(b.dif - a.dif, acdNor));
					ASSERT(false);
				}

				if (V3Dot(abcNor, -a.dif) > 0)
				{
					if (V3Dot(adbNor, -a.dif) > 0)
					{
						result.points[0] = b;
						result.points[1] = a;
						foundPointsCount = 2;
						testDir = V3Cross(V3Cross(ab, -a.dif), ab);
					}
					else if (V3Dot(acdNor, -a.dif) > 0)
					{
						result.points[0] = c;
						result.points[1] = a;
						foundPointsCount = 2;
						testDir = V3Cross(V3Cross(ac, -a.dif), ac);
					}
					else if (V3Dot(V3Cross(abcNor, ab), -a.dif) > 0)
					{
						result.points[0] = b;
						result.points[1] = a;
						foundPointsCount = 2;
						testDir = V3Cross(V3Cross(ab, -a.dif), ab);
					}
					else if (V3Dot(V3Cross(ac, abcNor), -a.dif) > 0)
					{
						result.points[0] = c;
						result.points[1] = a;
						foundPointsCount = 2;
						testDir = V3Cross(V3Cross(ac, -a.dif), ac);
					}
					else
					{
						result.points[0] = c;
						result.points[1] = b;
						result.points[2] = a;
						testDir = abcNor;
					}
				}
				else
				{
					if (V3Dot(acdNor, -a.dif) > 0)
					{
						if (V3Dot(adbNor, -a.dif) > 0)
						{
							result.points[0] = d;
							result.points[1] = a;
							foundPointsCount = 2;
							testDir = V3Cross(V3Cross(ad, -a.dif), ad);
						}
						else if (V3Dot(V3Cross(acdNor, ac), -a.dif) > 0)
						{
							result.points[0] = c;
							result.points[1] = a;
							foundPointsCount = 2;
							testDir = V3Cross(V3Cross(ac, -a.dif), ac);
						}
						else if (V3Dot(V3Cross(ad, acdNor), -a.dif) > 0)
						{
							result.points[0] = d;
							result.points[1] = a;
							foundPointsCount = 2;
							testDir = V3Cross(V3Cross(ad, -a.dif), ad);
						}
						else
						{
							result.points[0] = d;
							result.points[1] = c;
							result.points[2] = a;
							testDir = acdNor;
						}
					}
					else if (V3Dot(adbNor, -a.dif) > 0)
					{
						if (V3Dot(V3Cross(adbNor, ad), -a.dif) > 0)
						{
							result.points[0] = d;
							result.points[1] = a;
							foundPointsCount = 2;
							testDir = V3Cross(V3Cross(ad, -a.dif), ad);
						}
						else if (V3Dot(V3Cross(ab, adbNor), -a.dif) > 0)
						{
							result.points[0] = b;
							result.points[1] = a;
							foundPointsCount = 2;
							testDir = V3Cross(V3Cross(ab, -a.dif), ab);
						}
						else
						{
							result.points[0] = b;
							result.points[1] = d;
							result.points[2] = a;
							testDir = adbNor;
						}
					}
					else
					{
						// Done!
						result.points[foundPointsCount++] = a;
					}
				}
			} break;
		}
	}

#if DEBUG_BUILD
	if (g_debugContext->drawSupports)
	{
		for (int i = 0; i < foundPointsCount; ++i)
		{
			DrawDebugCubeAA(result.points[i].a, 0.04f, {0,1,1});
			DrawDebugCubeAA(result.points[i].a - result.points[i].dif, 0.04f, {1,1,0});
		}
	}
#endif

	return result;
}

CollisionInfo TestCollision(Transform *transformA, Transform *transformB, Collider *colliderA,
		Collider *colliderB)
{
	GJKResult gjkResult = GJKTest(transformA, transformB, colliderA, colliderB);
	if (!gjkResult.hit)
		return {};

#if DEBUG_BUILD
	if (g_debugContext->polytopeSteps[0] == nullptr)
	{
		for (u32 i = 0; i < ArrayCount(g_debugContext->polytopeSteps); ++i)
			g_debugContext->polytopeSteps[i] = ALLOC_N(TransientAllocator, DebugVertex, 256);
	}
#endif

	VERBOSE_LOG("\n\nNew EPA calculation\n");

	// By now we should have the tetrahedron from GJK
	// TODO dynamic array?
	EPAFace polytope[256];
	int polytopeCount = 0;

	// Make all faces from tetrahedron
	{
		// We don't need depenetration if we have a degenerate polytope (a zero-volume polytope)
		f32 epsilon = 0.000001f;
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < i; ++j)
				if (V3EqualWithEpsilon(gjkResult.points[i].dif, gjkResult.points[j].dif, epsilon))
					return {};

		GJKPoint &a = gjkResult.points[3];
		GJKPoint &b = gjkResult.points[2];
		GJKPoint &c = gjkResult.points[1];
		GJKPoint &d = gjkResult.points[0];

		// We took care during GJK to ensure the BCD triangle faces away from the origin
		polytope[0] = { b, d, c };
		// And we know A is on the opposite side of the origin
		polytope[1] = { a, b, c };
		polytope[2] = { a, c, d };
		polytope[3] = { a, d, b };
		polytopeCount = 4;
	}

	EPAFace closestFeature;
	f32 lastLeastDistance = -1.0f;
	const int maxIterations = 64;
	for (int epaStep = 0; epaStep < maxIterations; ++epaStep)
	{
		ASSERT(epaStep < maxIterations - 1);

#if DEBUG_BUILD
		// Save polytope for debug visualization
		if (!g_debugContext->freezePolytopeGeom && epaStep < DebugContext::epaMaxSteps)
		{
			if (g_debugContext->drawEPAClosestFeature)
			{
				GenPolytopeMesh(&closestFeature, 1, g_debugContext->polytopeSteps[epaStep],
						&g_debugContext->polytopeStepsVertexCounts[epaStep]);
			}
			else
			{
				GenPolytopeMesh(polytope, polytopeCount, g_debugContext->polytopeSteps[epaStep],
						&g_debugContext->polytopeStepsVertexCounts[epaStep]);
			}
			g_debugContext->epaStepCount = epaStep + 1;
		}
#endif

		// Find closest feature to origin in polytope
		EPAFace newClosestFeature = {};
		int validFacesFound = 0;
		f32 leastDistance = INFINITY;
		VERBOSE_LOG("Looking for closest feature\n");
		for (int faceIdx = 0; faceIdx < polytopeCount; ++faceIdx)
		{
			EPAFace *face = &polytope[faceIdx];

			v3 normal = V3Cross(face->c.dif - face->a.dif, face->b.dif - face->a.dif);
			f32 sqrlen = V3SqrLen(normal);
			// Some features might be degenerate 0-area triangles
			if (sqrlen == 0)
				continue;

			normal = normal / Sqrt(sqrlen);
			f32 distToOrigin = V3Dot(normal, face->a.dif);
			if (distToOrigin >= leastDistance)
				continue;

			// Make sure projected origin is within triangle
			v3 abNor = V3Cross(face->a.dif - face->b.dif, normal);
			v3 bcNor = V3Cross(face->b.dif - face->c.dif, normal);
			v3 acNor = V3Cross(face->c.dif - face->a.dif, normal);
			if (V3Dot(abNor, -face->a.dif) > 0 ||
				V3Dot(bcNor, -face->b.dif) > 0 ||
				V3Dot(acNor, -face->a.dif) > 0)
				continue;

			leastDistance = distToOrigin;
			newClosestFeature = *face;
			++validFacesFound;
		}
		if (leastDistance == INFINITY)
		{
			//ASSERT(false);
			Log("ERROR: EPA: Couldn't find closest feature!");
			// Collision is probably on the very edge, we don't need depenetration
			return {};
		}
#if 0
		else if (leastDistance <= lastLeastDistance + 0.0001f)
		{
			// We tried to expand in this direction but failed! So we are at the edge of
			// the Minkowski difference
			// TODO do we really need two exit conditions? Something must be wrong with
			// the other one!
			VERBOSE_LOG("Ending because couldn't get any further away from the outer edge");
			break;
		}
#endif
		else
		{
			closestFeature = newClosestFeature;
			lastLeastDistance = leastDistance;
			VERBOSE_LOG("Picked face with distance %.02f out of %d faces\n", leastDistance,
					validFacesFound);
		}

		// Expand polytope!
		v3 testDir = V3Cross(closestFeature.c.dif - closestFeature.a.dif, closestFeature.b.dif - closestFeature.a.dif);
		if (testDir.x == 0 && testDir.y == 0 && testDir.z == 0)
		{
			// Feature is a line. Pick a direction towards the origin.
			if (closestFeature.a.dif.x != 0 || closestFeature.a.dif.y != 0 || closestFeature.a.dif.z != 0)
				testDir = -closestFeature.a.dif;
			else if (closestFeature.b.dif.x != 0 || closestFeature.b.dif.y != 0 || closestFeature.b.dif.z != 0)
				testDir = -closestFeature.b.dif;
			else if (closestFeature.c.dif.x != 0 || closestFeature.c.dif.y != 0 || closestFeature.c.dif.z != 0)
				testDir = -closestFeature.c.dif;
			else
			{
				// Give up
				Log("ERROR! EPA: Feature is a point at the origin. Don't know which way to continue.\n");
				//ASSERT(false);
				break;
			}
		}
		GJKPoint newPoint = GJKSupport(transformB, transformA, colliderB, colliderA, testDir);
		VERBOSE_LOG("Found new point { %.02f, %.02f. %.02f } while looking in direction { %.02f, %.02f. %.02f }\n",
				newPoint.dif.x, newPoint.dif.y, newPoint.dif.z, testDir.x, testDir.y, testDir.z);
#if DEBUG_BUILD
		if (!g_debugContext->freezePolytopeGeom && epaStep < DebugContext::epaMaxSteps)
			g_debugContext->epaNewPoint[epaStep] = newPoint.a;
#endif
		// Without a little epsilon here we can sometimes pick a point that's already part of the
		// polytope, resulting in weird artifacts later on. I guess we could manually check for that
		// but this should be good enough.
		const f32 epsilon = 0.000001f;
		if (V3Dot(testDir, newPoint.dif - closestFeature.a.dif) <= epsilon)
		{
			VERBOSE_LOG("Done! Couldn't find a closer point\n");
			break;
		}
#if DEBUG_BUILD
		else if (V3Dot(testDir, newPoint.dif - closestFeature.b.dif) <= epsilon)
		{
			Log("ERROR! EPA: Redundant check triggered (B)\n");
			//ASSERT(false);
			break;
		}
		else if (V3Dot(testDir, newPoint.dif - closestFeature.c.dif) <= epsilon)
		{
			Log("ERROR! EPA: Redundant check triggered (C)\n");
			//ASSERT(false);
			break;
		}
#endif
		EPAEdge holeEdges[256];
		int holeEdgesCount = 0;
		int oldPolytopeCount = polytopeCount;
		for (int faceIdx = 0; faceIdx < polytopeCount; )
		{
			EPAFace *face = &polytope[faceIdx];
			v3 normal = V3Cross(face->c.dif - face->a.dif, face->b.dif - face->a.dif);
			// Ignore if not facing new point
			if (V3Dot(normal, newPoint.dif - face->a.dif) <= 0)
			{
				++faceIdx;
				continue;
			}

			// Add/remove edges to the hole (XOR)
			EPAEdge faceEdges[3] =
			{
				{ face->a, face->b },
				{ face->b, face->c },
				{ face->c, face->a }
			};
			for (int edgeIdx = 0; edgeIdx < 3; ++edgeIdx)
			{
				const EPAEdge &edge = faceEdges[edgeIdx];
				// If it's already on the list, remove it
				bool found = false;
				for (int holeEdgeIdx = 0; holeEdgeIdx < holeEdgesCount; ++holeEdgeIdx)
				{
					const EPAEdge &holeEdge = holeEdges[holeEdgeIdx];
					if ((edge.a.dif == holeEdge.a.dif && edge.b.dif == holeEdge.b.dif) ||
						(edge.a.dif == holeEdge.b.dif && edge.b.dif == holeEdge.a.dif))
					{
						holeEdges[holeEdgeIdx] = holeEdges[--holeEdgesCount];
						found = true;
						break;
					}
				}
				// Otherwise add it
				if (!found)
					holeEdges[holeEdgesCount++] = edge;
			}
			// Remove face from polytope
			polytope[faceIdx] = polytope[--polytopeCount];
		}
		int deletedFaces = oldPolytopeCount - polytopeCount;
		VERBOSE_LOG("Deleted %d faces which were facing new point\n", deletedFaces);
		VERBOSE_LOG("Presumably left a hole with %d edges\n", holeEdgesCount);
		if (deletedFaces > 1 && holeEdgesCount >= deletedFaces * 3)
		{
			Log("ERROR! EPA: Multiple holes were made on the polytope!\n");
#if DEBUG_BUILD
			if (!g_debugContext->freezePolytopeGeom && epaStep < DebugContext::epaMaxSteps - 1)
			{
				GenPolytopeMesh(polytope, polytopeCount, g_debugContext->polytopeSteps[epaStep + 1],
						&g_debugContext->polytopeStepsVertexCounts[epaStep + 1]);
				g_debugContext->epaNewPoint[epaStep + 1] = newPoint.a;
				g_debugContext->epaStepCount = epaStep + 1;
				g_debugContext->freezePolytopeGeom = true;
			}
#endif
		}
		oldPolytopeCount = polytopeCount;
		// Now we should have a hole in the polytope, of which all edges are in holeEdges

		for (int holeEdgeIdx = 0; holeEdgeIdx < holeEdgesCount; ++holeEdgeIdx)
		{
			const EPAEdge &holeEdge = holeEdges[holeEdgeIdx];
			EPAFace newFace = { holeEdge.a, holeEdge.b, newPoint };
			polytope[polytopeCount++] = newFace;
		}
		VERBOSE_LOG("Added %d faces to fill the hole. Polytope now has %d faces\n",
				polytopeCount - oldPolytopeCount, polytopeCount);
	}

	CollisionInfo result;
	result.hit = true;

	v3 closestFeatureNor = V3Normalize(V3Cross(closestFeature.b.dif - closestFeature.a.dif,
			closestFeature.c.dif - closestFeature.a.dif));
	result.depenetrationVector = closestFeatureNor * V3Dot(closestFeatureNor, closestFeature.a.dif);

#if DEBUG_BUILD
	// Save last step
	int lastStep = g_debugContext->epaStepCount;
	if (!g_debugContext->freezePolytopeGeom && lastStep < DebugContext::epaMaxSteps)
	{
		if (g_debugContext->drawEPAClosestFeature)
		{
			GenPolytopeMesh(&closestFeature, 1, g_debugContext->polytopeSteps[lastStep],
					&g_debugContext->polytopeStepsVertexCounts[lastStep]);
		}
		else
		{
			GenPolytopeMesh(polytope, polytopeCount, g_debugContext->polytopeSteps[lastStep],
					&g_debugContext->polytopeStepsVertexCounts[lastStep]);
		}
		g_debugContext->epaStepCount = lastStep + 1;
	}

#if 0
	DrawDebugCubeAA((closestFeature.a.a + closestFeature.b.a + closestFeature.c.a) / 3, 0.04f, {0,1,0});
	DrawDebugCubeAA((closestFeature.a.a + closestFeature.b.a + closestFeature.c.a) / 3 -
			depenetrationVector / 2, 0.04f, {0,0,1});
	DrawDebugCubeAA((closestFeature.a.a + closestFeature.b.a + closestFeature.c.a) / 3 -
			depenetrationVector, 0.04f, {0,1,1});
#endif

	Triangle triangle = { closestFeature.a.dif, closestFeature.b.dif, closestFeature.c.dif };
	triangle.normal = closestFeatureNor;
	v3 hit;
	RayTriangleIntersection({}, result.depenetrationVector, true, &triangle, &hit);
	DrawDebugCubeAA(hit, 0.04f, {0,1,1});

	v3 bary = BarycentricCoordinates(&triangle, hit);
	v3 transp = closestFeature.a.a * bary.x + closestFeature.b.a * bary.y + closestFeature.c.a * bary.z;
	DrawDebugCubeAA(transp, 0.04f, {0,0,1});

	result.hitPoint = transp;

	//g_debugContext->epaNewPoint[lastStep] = (closestFeature.a.a + closestFeature.b.a +
		//closestFeature.c.a) / 3 - depenetrationVector / 2;
#endif

	return result;
}

#if DEBUG_BUILD
void GetGJKStepGeometry(int step, DebugVertex **buffer, u32 *vertexCount)
{
	*vertexCount = g_debugContext->GJKStepsVertexCounts[step];
	*buffer = g_debugContext->GJKSteps[step];
}

void GetEPAStepGeometry(int step, DebugVertex **buffer, u32 *vertexCount)
{
	*vertexCount = g_debugContext->polytopeStepsVertexCounts[step];
	*buffer = g_debugContext->polytopeSteps[step];
}
#endif

#undef VERBOSE_LOG
