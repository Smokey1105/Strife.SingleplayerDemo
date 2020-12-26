
#include "InputService.hpp"

#include <slikenet/BitStream.h>
#include <slikenet/peerinterface.h>

#include "Engine.hpp"
#include "PuckEntity.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Memory/Util.hpp"
#include "Net/NetworkPhysics.hpp"
#include "Net/ReplicationManager.hpp"
#include "Physics/PathFinding.hpp"
#include "Renderer/Renderer.hpp"
#include "Tools/Console.hpp"
#include "MessageHud.hpp"

#include "CastleEntity.hpp"

InputButton g_quit = InputButton(SDL_SCANCODE_ESCAPE);
InputButton g_upButton(SDL_SCANCODE_W);
InputButton g_downButton(SDL_SCANCODE_S);
InputButton g_leftButton(SDL_SCANCODE_A);
InputButton g_rightButton(SDL_SCANCODE_D);

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

void InputService::OnAdded()
{

}

void InputService::ReceiveEvent(const IEntityEvent& ev)
{
    if (ev.Is<SceneLoadedEvent>())
    {
        spawnPositions.push_back({ 1000, 1000 });

        auto spawnPoint = spawnPositions[spawnPositions.size() - 1];
        spawnPositions.pop_back();

        auto spawn = scene->CreateEntity<CastleEntity>(spawnPoint);
        spawn->playerId = 0;

        for (int i = 0; i < 4; ++i)
        {
            spawn->SpawnPlayer();
        }

        spawns.push_back(spawn);

        scene->GetCameraFollower()->FollowMouse();
    }
    if (ev.Is<UpdateEvent>())
    {
        HandleInput();
    }
    else if (auto renderEvent = ev.Is<RenderEvent>())
    {
        Render(renderEvent->renderer);
    }
    else if (auto joinedServerEvent = ev.Is<JoinedServerEvent>())
    {
        scene->replicationManager->localClientId = joinedServerEvent->selfId;
    }
    else if (auto connectedEvent = ev.Is<PlayerConnectedEvent>())
    {
        if (scene->isServer)
        {

        }
    }
}

void InputService::HandleInput()
{
    if (g_quit.IsPressed())
    {
        scene->GetEngine()->QuitGame();
    }

    if (!scene->isServer)
    {
        if (scene->deltaTime == 0)
        {
            return;
        }

        auto mouse = scene->GetEngine()->GetInput()->GetMouse();

        if (mouse->LeftPressed())
        {
            for (auto player : players)
            {
                if (player->Bounds().ContainsPoint(scene->GetCamera()->ScreenToWorld(mouse->MousePosition()))
                    && player->playerId == 0)
                {
                    PlayerEntity* oldPlayer;
                    if (activePlayer.TryGetValue(oldPlayer))
                    {
                        //oldPlayer->GetComponent<PlayerEntity::NeuralNetwork>()->mode = NeuralNetworkMode::Deciding;
                    }

                    activePlayer = player;
                    //player->GetComponent<PlayerEntity::NeuralNetwork>()->mode = NeuralNetworkMode::CollectingSamples;

                    scene->GetCameraFollower()->FollowEntity(player);

                    break;
                }
            }
        }

        PlayerEntity* self;
        if (activePlayer.TryGetValue(self))
        {
            auto direction = GetInputDirection();
            self->rigidBody->SetVelocity(MoveDirectionToVector2(direction).Normalize() * 200);

            if (mouse->RightPressed())
            {
                bool attack = false;
                for (auto entity : scene->GetEntities())
                {
                    auto netComponent = entity->GetComponent<NetComponent>(false);

                    if (netComponent == nullptr)
                    {
                        continue;
                    }

                    if (entity->GetComponent<HealthBarComponent>(false) == nullptr)
                    {
                        continue;
                    }

                    if (entity->Bounds().ContainsPoint(scene->GetCamera()->ScreenToWorld(mouse->MousePosition())))
                    {

                    }
                }
            }
        }
    }
}

void InputService::Render(Renderer* renderer)
{
    PlayerEntity* currentPlayer;
    if (activePlayer.TryGetValue(currentPlayer))
    {
        renderer->RenderRectangleOutline(currentPlayer->Bounds(), Color::White(), -1);
    }
}

MoveDirection InputService::GetInputDirection()
{
    Vector2 inputDir;
    if (g_leftButton.IsDown()) --inputDir.x;
    if (g_rightButton.IsDown()) ++inputDir.x;
    if (g_upButton.IsDown()) --inputDir.y;
    if (g_downButton.IsDown()) ++inputDir.y;

    return GetClosestMoveDirection(inputDir);
}