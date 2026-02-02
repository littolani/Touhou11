#include "StdVm.h"

BOOL StdVm::checkEntityVisibility(Camera* camera, D3DXVECTOR3* offset, float cullDistanceSq, Entity* entity)
{
    // Calculate the Entity's Center Position in World Space
    // The 'offset' parameter appears to be a translation offset applied to the entity.
    D3DXVECTOR3 entityWorldPos = entity->position + *offset;

    // Calculate vector from Camera Eye to Entity
    D3DXVECTOR3 viewDiff = entityWorldPos - (camera->offset + camera->eye);
    float distanceSq = D3DXVec3LengthSq(&viewDiff);

    // If the entity is too far away, cull it immediately.
    if (distanceSq > cullDistanceSq)
        return 1;

    // Construct the AABB Corners
    // We define the 8 corners of the entity's bounding box relative to its position.
    float halfW = entity->width * 0.5f;
    float halfH = entity->height * 0.5f;
    float halfD = entity->depth * 0.5f;

    D3DXVECTOR3 corners[8];
    // WorldPoint = offset + entity->pos + cornerOffset

    float minX = entity->position.x - halfW;
    float maxX = entity->position.x + halfW;
    float minY = entity->position.y - halfH;
    float maxY = entity->position.y + halfH;
    float minZ = entity->position.z - halfD;
    float maxZ = entity->position.z + halfD;

    corners[0] = D3DXVECTOR3(maxX, maxY, maxZ);
    corners[1] = D3DXVECTOR3(maxX, minY, maxZ);
    corners[2] = D3DXVECTOR3(maxX, maxY, minZ);
    corners[3] = D3DXVECTOR3(maxX, minY, minZ);
    corners[4] = D3DXVECTOR3(minX, maxY, maxZ);
    corners[5] = D3DXVECTOR3(minX, minY, maxZ);
    corners[6] = D3DXVECTOR3(minX, maxY, minZ);
    corners[7] = D3DXVECTOR3(minX, minY, minZ);

    // Project Corners to Screen Space
    D3DXVECTOR3 projectedPoints[8];
    D3DXMATRIX worldMatrix;

    // The translation is 'offset'. The corners already contain 'entity->position'.
    D3DXMatrixTranslation(&worldMatrix, offset->x, offset->y, offset->z);

    D3DXVec3ProjectArray(
        projectedPoints,
        sizeof(D3DXVECTOR3),
        corners,
        sizeof(D3DXVECTOR3),
        &camera->viewport,
        &camera->projectionMatrix,
        &camera->viewMatrix,
        &worldMatrix,
        8
    );

    // Calculate Screen-Space Bounding Box (Min/Max)
    // Initialize with values that ensure we capture the range (inverted defaults)
    float screenMinX = FLT_MAX;
    float screenMaxX = -FLT_MAX;
    float screenMinY = FLT_MAX;
    float screenMaxY = -FLT_MAX;

    bool anyValidPoint = false;

    for (int i = 0; i < 8; ++i)
    {
        // Only consider points in front of the camera (0 < z <= 1)
        if (projectedPoints[i].z > 0.0f && projectedPoints[i].z <= 1.0f)
        {
            if (projectedPoints[i].x < screenMinX)
                screenMinX = projectedPoints[i].x;

            if (projectedPoints[i].x > screenMaxX)
                screenMaxX = projectedPoints[i].x;

            if (projectedPoints[i].y < screenMinY)
                screenMinY = projectedPoints[i].y;

            if (projectedPoints[i].y > screenMaxY)
                screenMaxY = projectedPoints[i].y;

            anyValidPoint = true;
        }
    }

    // If no points were valid (all behind camera), default to culled.
    if (!anyValidPoint)
        return TRUE;

    // Intersection Check with Gameplay Viewport
    // The hardcoded constants represent the active gameplay area
    // Left: 32, Right: 416, Top: 16, Bottom: 464.
    const float GAME_VIEW_LEFT = 32.0f;
    const float GAME_VIEW_RIGHT = 416.0f;
    const float GAME_VIEW_TOP = 16.0f;
    const float GAME_VIEW_BOTTOM = 464.0f;
    bool overlapsX = (screenMinX < GAME_VIEW_RIGHT) && (screenMaxX > GAME_VIEW_LEFT);
    bool overlapsY = (screenMinY < GAME_VIEW_BOTTOM) && (screenMaxY > GAME_VIEW_TOP);

    if (overlapsX && overlapsY)
        return FALSE;

    return TRUE;
}