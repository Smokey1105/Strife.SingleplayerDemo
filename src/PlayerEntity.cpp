#include <Memory/Util.hpp>
#include <Components/LightComponent.hpp>
#include "PlayerEntity.hpp"
#include "InputService.hpp"
#include "Components/RigidBodyComponent.hpp"

#include "Renderer/Renderer.hpp"

#include "CastleEntity.hpp"
#include "FireballEntity.hpp"

Vector2 MoveDirectionToVector2(MoveDirection direction)
{
    const float halfSqrt2 = sqrt(2) / 2;

    switch(direction)
    {
    case MoveDirection::None: return Vector2(0, 0);
    case MoveDirection::West: return Vector2(-1, 0);;
    case MoveDirection::East: return Vector2(1, 0);;
    case MoveDirection::North: return Vector2(0, -1);;
    case MoveDirection::South: return Vector2(0, 1);;
    case MoveDirection::NorthEast: return Vector2(halfSqrt2, -halfSqrt2);;
    case MoveDirection::SouthEast: return Vector2(halfSqrt2, halfSqrt2);;
    case MoveDirection::NorthWest: return Vector2(-halfSqrt2, -halfSqrt2);;
    case MoveDirection::SouthWest: return Vector2(-halfSqrt2, halfSqrt2);;
    default: return Vector2(0, 0);
    }
}

MoveDirection GetClosestMoveDirection(Vector2 v)
{
    if (v == Vector2(0, 0))
    {
        return MoveDirection::None;
    }

    float maxDot = -INFINITY;
    MoveDirection closestDirection;

    for (int i = 1; i < (int)MoveDirection::TotalDirections; ++i)
    {
        auto direction = static_cast<MoveDirection>(i);
        Vector2 dir = MoveDirectionToVector2(direction);
        float dot = dir.Dot(v);

        if (dot > maxDot)
        {
            maxDot = dot;
            closestDirection = direction;
        }
    }

    return closestDirection;
}

void PlayerEntity::OnAdded()
{
    auto light = AddComponent<LightComponent<PointLight>>();
    light->position = Center();
    light->color = Color(255, 255, 255, 255);
    light->maxDistance = 400;
    light->intensity = 0.6;

    health = AddComponent<HealthBarComponent>();
    health->offsetFromCenter = Vector2(0, -20);

    rigidBody = AddComponent<RigidBodyComponent>(b2_dynamicBody);
    //pathFollower = AddComponent<PathFollowerComponent>(rigidBody);

    team = AddComponent<TeamComponent>();

    SetDimensions({ 30, 30 });
    auto box = rigidBody->CreateBoxCollider(Dimensions());

    box->SetDensity(1);
    box->SetFriction(0);

    scene->GetService<InputService>()->players.push_back(this);
    gridSensor = AddComponent<GridSensorComponent<40, 40>>(Vector2(16, 16));
}

void PlayerEntity::ReceiveEvent(const IEntityEvent& ev)
{
    if (auto outOfHealth = ev.Is<OutOfHealthEvent>())
    {
        Die(outOfHealth);
    }
}

void PlayerEntity::Die(const OutOfHealthEvent* outOfHealth)
{
    Destroy();

    for (auto spawn : scene->GetService<InputService>()->spawns)
    {
        if (spawn->playerId == playerId)
        {
            spawn->StartTimer(10, [=]
            {
                spawn->SpawnPlayer();
            });

            break;
        }
    }
}

void PlayerEntity::OnDestroyed()
{
    RemoveFromVector(scene->GetService<InputService>()->players, this);
}

void PlayerEntity::Render(Renderer* renderer)
{
    auto position = Center();

    // Render player
    {
        Color c[5] = {
            Color::CornflowerBlue(),
            Color::Green(),
            Color::Orange(),
            Color::HotPink(),
            Color::Yellow()
        };

        auto color = c[playerId];
        renderer->RenderRectangle(Rectangle(position - Dimensions() / 2, Dimensions()), color, -0.99);
    }
}

void PlayerEntity::FixedUpdate(float deltaTime)
{
    attackCoolDown -= deltaTime;

    if (state == PlayerState::Attacking)
    {
        Entity* target;
        RaycastResult hitResult;
        if (attackTarget.TryGetValue(target))
        {
            bool withinAttackingRange = (target->Center() - Center()).Length() < 200;
            bool canSeeTarget = scene->Raycast(Center(), target->Center(), hitResult)
                                && hitResult.handle.OwningEntity() == target;

            if (withinAttackingRange && canSeeTarget)
            {
                rigidBody->SetVelocity({ 0, 0 });
                auto dir = (target->Center() - Center()).Normalize();

                if (attackCoolDown <= 0)
                {
                    auto fireball = scene->CreateEntity<FireballEntity>(Center(), dir * 400);
                    fireball->playerId = playerId;
                    fireball->ownerId = id;

                    attackCoolDown = 1;
                }

                //pathFollower->Stop(true);
                return;
            }
        }
        else
        {
            state = PlayerState::None;
        }
    }
}

void PlayerEntity::Attack(Entity* entity)
{
    attackTarget = entity;
    state = PlayerState::Attacking;
}

void PlayerEntity::SetMoveDirection(Vector2 direction)
{
    rigidBody->SetVelocity(direction);

    if (direction != Vector2(0, 0))
    {
        state = PlayerState::Moving;
    }
}

void PlayerEntity::MoveTo(Vector2 position)
{
    //pathFollower->SetTarget(position);
    state = PlayerState::Moving;
}

void PlayerEntity::GetObservation(Observation& input)
{
    //temp fix:
    PlayerEntity* closestPlayer;
    float minDistance;
    for (auto player : scene->GetEntitiesOfType<PlayerEntity>())
    {
        auto distance = (player->Center() - Center()).Length();
        if (closestPlayer == nullptr || distance < minDistance)
        {
            closestPlayer = player;
            minDistance = distance;
        }
    }

    PlayerObservation playerObs;
    playerObs.position = closestPlayer->Center() - Center();
    playerObs.velocity = closestPlayer->rigidBody->GetVelocity();
    playerObs.health = closestPlayer->health->health;

    MinionEntity* closestMinion;
    for (auto minion : scene->GetEntitiesOfType<MinionEntity>())
    {
        auto distance = (minion->Center() - Center()).Length();
        if (closestMinion == nullptr || distance < minDistance)
        {
            closestMinion = minion;
            minDistance = distance;
        }
    }

    MinionObservation minionObs;
    minionObs.position = closestMinion->Center() - Center();

    RigidBodyComponent* rb;
    if (closestMinion->TryGetComponent(rb))
    {
        minionObs.velocity = rb->GetVelocity();
    }

    HealthBarComponent* minionHealth;
    if (closestMinion->TryGetComponent(minionHealth))
    {
        minionObs.health = minionHealth->health;
    }

    CastleEntity* closestCastle;
    float minCastleDistance;
    for (auto castle : scene->GetEntitiesOfType<CastleEntity>())
    {
        auto distance = (castle->Center() - Center()).Length();
        if (closestCastle == nullptr || distance < minCastleDistance)
        {
            closestCastle = castle;
            minCastleDistance = distance;
        }
    }

    TowerEntity* closestTower;
    float minTowerDistance;
    for (auto tower : scene->GetEntitiesOfType<TowerEntity>())
    {
        auto distance = (tower->Center() - Center()).Length();
        if (closestTower == nullptr || distance < minTowerDistance)
        {
            closestTower = tower;
            minTowerDistance = distance;
        }
    }

    Entity* closestBuilding = closestTower;
    if (minCastleDistance < minTowerDistance)
    {
        closestBuilding = closestCastle;
    }

    BuildingObservation buildingObs;
    buildingObs.position = closestBuilding->Center() - Center();

    HealthBarComponent* buildingHealth;
    if (closestBuilding->TryGetComponent(buildingHealth))
    {
        buildingObs.health = buildingHealth->health;
    }

    /*for (auto player : scene->GetEntitiesOfType<PlayerEntity>()) 
    {
        PlayerObservation playerObs;
        playerObs.position = player->Center() - Center();
        playerObs.velocity = player->rigidBody->GetVelocity();
        playerObs.health = player->health->health;

        input.players.push_back(playerObs);
    }

    for (auto minion : scene->GetEntitiesOfType<MinionEntity>())
    {
        MinionObservation minionObs;
        minionObs.position = minion->Center() - Center();

        RigidBodyComponent* rb;
        if (minion->TryGetComponent(rb))
        {
            minionObs.velocity = rb->GetVelocity();
        }

        HealthBarComponent* health;
        if (minion->TryGetComponent(health))
        {
            minionObs.health = health->health;
        }
        
        input.minions.push_back(minionObs);
    }

    for (auto tower : scene->GetEntitiesOfType<TowerEntity>())
    {
        BuildingObservation buildingObs;
        buildingObs.position = tower->Center() - Center();
        
        HealthBarComponent* health;
        if (tower->TryGetComponent(health))
        {
            buildingObs.health = health->health;
        }

        input.buildings.push_back(buildingObs);
    }

    for (auto castle : scene->GetEntitiesOfType<CastleEntity>())
    {
        BuildingObservation buildingObs;
        buildingObs.position = castle->Center() - Center();

        HealthBarComponent* health;
        if (castle->TryGetComponent(health))
        {
            buildingObs.health = health->health;
        }

        input.buildings.push_back(buildingObs);
    }*/
}
