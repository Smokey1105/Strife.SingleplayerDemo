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
    pathFollower = AddComponent<PathFollowerComponent>(rigidBody);

    team = AddComponent<TeamComponent>();

    SetDimensions({ 30, 30 });
    auto box = rigidBody->CreateBoxCollider(Dimensions());

    box->SetDensity(1);
    box->SetFriction(0);

    scene->GetService<InputService>()->players.push_back(this);
    //gridSensor = AddComponent<GridSensorComponent<40, 40>>(Vector2(16, 16));
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

    /*for (auto spawn : scene->GetService<InputService>()->spawns)
    {
        if (spawn->playerId == playerId)
        {
            spawn->SpawnPlayer();
            break;
        }
    }*/
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

                pathFollower->Stop(true);
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
    pathFollower->SetTarget(position);
    lastMoveVector = position;
    state = PlayerState::Moving;
}

void PlayerEntity::GetObservation(Observation& input)
{
    // Initialize
    input.players.clear();
    input.minions.clear();
    input.buildings.clear();

    // Self
	input.self.position = Center() / 4160.0f;
	input.self.velocity = rigidBody->GetVelocity() / 200.0f;
    input.self.health = health->health / static_cast<float>(health->maxHealth);

    for (auto player : scene->GetEntitiesOfType<PlayerEntity>()) 
    {
        PlayerObservation playerObs;
        playerObs.position = (player->Center() - Center()) / 4160.0f;
        playerObs.velocity = player->rigidBody->GetVelocity() / 200.0f;
        playerObs.health = player->health->health / static_cast<float>(player->health->maxHealth);

        input.players.push_back(playerObs);
    }

    for (auto minion : scene->GetEntitiesOfType<MinionEntity>())
    {
        MinionObservation minionObs;
        minionObs.position = (minion->Center() - Center()) / 4160.0f;

        RigidBodyComponent* rb;
        if (minion->TryGetComponent(rb))
        {
            minionObs.velocity = rb->GetVelocity() / 200.0f;
        }

        HealthBarComponent* health;
        if (minion->TryGetComponent(health))
        {
            minionObs.health = health->health / static_cast<float>(health->maxHealth);
        }
        
        input.minions.push_back(minionObs);
    }

    for (auto tower : scene->GetEntitiesOfType<TowerEntity>())
    {
        BuildingObservation buildingObs;
        buildingObs.position = (tower->Center() - Center()) / 4160.0f;
        
        HealthBarComponent* health;
        if (tower->TryGetComponent(health))
        {
            buildingObs.health = health->health / static_cast<float>(health->maxHealth);
        }

        input.buildings.push_back(buildingObs);
    }

    for (auto castle : scene->GetEntitiesOfType<CastleEntity>())
    {
        BuildingObservation buildingObs;
        buildingObs.position = (castle->Center() - Center()) / 4160.0f;

        HealthBarComponent* health;
        if (castle->TryGetComponent(health))
        {
            buildingObs.health = health->health / static_cast<float>(health->maxHealth);
        }
        else
        {
            buildingObs.health = 1;
        }

        input.buildings.push_back(buildingObs);
    }
}
