#include <SDL2/SDL.h>

#include "Engine.hpp"
#include "InputService.hpp"
#include "PlayerEntity.hpp"
#include "TowerEntity.hpp"
#include "CastleEntity.hpp"
#include "MinionEntity.hpp"
#include "PlayerNeuralNetworkService.hpp"
#include "Scene/IGame.hpp"
#include "Scene/Scene.hpp"
#include "Scene/TilemapEntity.hpp"
#include "Tools/Console.hpp"

struct Game : IGame
{
    void ConfigureGame(GameConfig& config) override
    {
        config
            .SetDefaultScene("erebor"_sid)
            .SetWindowCaption("Strife Singleplayer Demo")
            .SetGameName("Strife Singleplayer Demo")
            .ExecuteUserConfig("user.cfg")
            .EnableDevConsole("console-font");
    }

    void LoadResources(ResourceManager* resourceManager)
    {
        resourceManager->LoadContentFile("Content.json");
    }

    void ConfigureEngine(EngineConfig& config) override
    {
        config.initialConsoleCmd = initialConsoleCmd;
    }

    void BuildScene(Scene* scene) override
    {
    	auto neuralNetworkManager = GetEngine()->GetNeuralNetworkManager();
    	auto inputService = scene->AddService<InputService>();
        scene->AddService<PlayerNeuralNetworkService>(neuralNetworkManager->GetNetwork<PlayerNetwork>("nn"), inputService);
    }

    void OnGameStart() override
    {
        auto engine = GetEngine();
        auto neuralNetworkManager = engine->GetNeuralNetworkManager();

        // Create networks
        {
            auto playerDecider = neuralNetworkManager->CreateDecider<PlayerDecider>();
            auto playerTrainer = neuralNetworkManager->CreateTrainer<PlayerTrainer>(engine->GetMetricsManager()->GetOrCreateMetric("loss"));

        	int sequenceLength = 1;
            neuralNetworkManager->CreateNetwork("nn", playerDecider, playerTrainer, sequenceLength);
        }

        // Add types of objects the sensors can pick up
        {
            SensorObjectDefinition sensorDefinition;
            sensorDefinition.Add<PlayerEntity>(1).SetColor(Color::Red()).SetPriority(1);
            sensorDefinition.Add<TilemapEntity>(2).SetColor(Color::Gray()).SetPriority(0);
            sensorDefinition.Add<CastleEntity>(3).SetColor(Color::Blue()).SetPriority(0.7);
            sensorDefinition.Add<TowerEntity>(4).SetColor(Color::Orange()).SetPriority(0.9);
            sensorDefinition.Add<MinionEntity>(5).SetColor(Color::Green()).SetPriority(0.8);

            neuralNetworkManager->SetSensorObjectDefinition(sensorDefinition);
        }

        engine->StartSinglePlayerGame("erebor");
    }

    std::string initialConsoleCmd;
};

int main(int argc, char* argv[])
{
    Game game;

    if (argc >= 2)
    {
        game.initialConsoleCmd = argv[1];
    }

    game.Run();

    return 0;
}
