#include "CastleEntity.hpp"

#include "Engine.hpp"
#include "PlayerEntity.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Components/SpriteComponent.hpp"
#include "Physics/PathFinding.hpp"
#include "Net/ReplicationManager.hpp"

void CastleEntity::DoSerialize(EntitySerializer& serializer)
{

}

void CastleEntity::OnAdded()
{
    spriteComponent = AddComponent<SpriteComponent>("castleSprite");
    spriteComponent->scale = Vector2(5.0f);

    Vector2 size{ 67 * 5, 55 * 5 };
    SetDimensions(size);
    scene->GetService<PathFinderService>()->AddObstacle(Bounds());

    auto rigidBody = AddComponent<RigidBodyComponent>(b2_staticBody);
    rigidBody->CreateBoxCollider(size);
    
    team = AddComponent<TeamComponent>();

    auto offset = size / 2 + Vector2(40, 40);

    _spawnSlots[0] = Center() + offset.YVector();
    _spawnSlots[1] = Center() - offset.YVector();

    _light = AddComponent<LightComponent<PointLight>>();
    _light->position = Center();
    _light->intensity = 0.5;
    _light->maxDistance = 500;
}

void CastleEntity::Update(float deltaTime)
{
    int teamCount = 0;
    for (auto player : scene->GetEntitiesOfType<PlayerEntity>())
    {
        if (player->team->teamId == team->teamId)
        {
            teamCount++;
        }
    }

    if (teamCount < 2)
    {
        SpawnPlayer();
    }

    _light->color = playerId == 0
        ? Color::Green()
        : Color::White();
}

void CastleEntity::SpawnPlayer()
{
    auto position = _spawnSlots[_nextSpawnSlotId];
    _nextSpawnSlotId = (_nextSpawnSlotId + 1) % 2;

    auto player = scene->CreateEntity<PlayerEntity>({ position });
    player->playerId = playerId;

    TeamComponent* playerTeam;

    if (player->TryGetComponent(playerTeam))
    {
        playerTeam->teamId = playerId;
    }
}

void CastleEntity::OnDestroyed()
{
    for (auto player : scene->GetEntitiesOfType<PlayerEntity>())
    {
        if (player->playerId == playerId)
        {
            player->Destroy();
        }
    }

    scene->GetService<PathFinderService>()->RemoveObstacle(Bounds());
}

void CastleEntity::ReceiveEvent(const IEntityEvent& ev)
{
    if (ev.Is<OutOfHealthEvent>())
    {
        Destroy();
    }

    if (ev.Is<TowerDestroyedEvent>())
    {
        auto hb = AddComponent<HealthBarComponent>();
        hb->offsetFromCenter = -Vector2(67 * 5, 55 * 5).YVector() / 2 - Vector2(0, 5);
        hb->maxHealth = 1000;
        hb->health = 1000;
    }
}
