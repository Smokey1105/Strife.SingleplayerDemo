#include "InputService.hpp"
#include "Components/RigidBodyComponent.hpp"
#include "Physics/PathFinding.hpp"
#include "Renderer/Renderer.hpp"
#include "Tools/Console.hpp"

#include "CastleEntity.hpp"
#include "TowerEntity.hpp"
#include "MinionEntity.hpp"
#include "TeamComponent.hpp"

InputButton g_quit = InputButton(SDL_SCANCODE_ESCAPE);
InputButton g_upButton(SDL_SCANCODE_W);
InputButton g_downButton(SDL_SCANCODE_S);
InputButton g_leftButton(SDL_SCANCODE_A);
InputButton g_rightButton(SDL_SCANCODE_D);

void InputService::ReceiveEvent(const IEntityEvent& ev)
{
    if (ev.Is<SceneLoadedEvent>())
    {
        CastleEntity* castle1 = scene->CreateEntity<CastleEntity>(Vector2(327.5, 2080));
        CastleEntity* castle2 = scene->CreateEntity<CastleEntity>(Vector2(3832.5, 2080));
        TowerEntity* tower1 = scene->CreateEntity<TowerEntity>(Vector2(1260, 1997));
        TowerEntity* tower2 = scene->CreateEntity<TowerEntity>(Vector2(2900, 1997));
        MinionSpawner* minionSpawner1 = scene->CreateEntity<MinionSpawner>(Vector2(520, 2163));
        MinionSpawner* minionSpawner2 = scene->CreateEntity<MinionSpawner>(Vector2(3640, 2163));

        castle1->tower = tower1;
        castle1->minionSpawner = minionSpawner1;
        castle2->tower = tower2;
        castle2->minionSpawner = minionSpawner2;
        tower1->castle = castle1;
        tower2->castle = castle2;

        auto spawnPoints = scene->GetEntitiesOfType<CastleEntity>();

        int id = 0;
        for (auto i = spawnPoints.begin(); i != spawnPoints.end(); ++i)
        {
            SpawnPlayer(*i, id++);
        }     
    }
    if (ev.Is<UpdateEvent>())
    {
        HandleInput();
    }
    else if (auto renderEvent = ev.Is<RenderEvent>())
    {
        Render(renderEvent->renderer);
    }
}

void InputService::HandleInput()
{
    if (g_quit.IsPressed())
    {
        scene->GetEngine()->QuitGame();
    }
   
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
        self->SetMoveDirection(MoveDirectionToVector2(direction) * 200);
        self->lastDirection = direction;

        if (mouse->RightPressed())
        {
            for (auto entity : scene->GetEntities())
            {
                if (entity->GetComponent<HealthBarComponent>(false) == nullptr)
                {
                    continue;
                }

                if (entity->Bounds().ContainsPoint(scene->GetCamera()->ScreenToWorld(mouse->MousePosition())))
                {
                    self->Attack(entity);
                    break;
                }

                self->MoveTo(scene->GetCamera()->ScreenToWorld(mouse->MousePosition()));
            }
        }
    }
    
}

void InputService::Render(Renderer* renderer)
{
    PlayerEntity* currentPlayer;
    if (activePlayer.TryGetValue(currentPlayer))
    {
        renderer->RenderRectangleOutline(Rectangle(currentPlayer->ScreenCenter() - Vector2(16), Vector2(32)), Color::White(), -1);
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

void InputService::SpawnPlayer(CastleEntity* spawn, int playerId)
{
    spawn->playerId = playerId;
    spawn->tower->playerId = playerId;
    spawn->team->teamId = playerId;
    spawn->tower->team->teamId = playerId;
    spawn->minionSpawner->team->teamId = playerId;

    for (int i = 0; i < 2; ++i)
    {
        spawn->SpawnPlayer();
    }
  
    spawns.push_back(spawn);

    if (playerId == 0)
    {
        scene->GetCameraFollower()->CenterOn(spawn->Center());
    }
}
