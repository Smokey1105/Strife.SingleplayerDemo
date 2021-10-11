#include "Math/Vector2.hpp"
#include "ML/ML.hpp"
#include "TensorPacking.hpp"
#include <torch/torch.h>
#include "ML/GridSensor.hpp"
#include "GameML.hpp"
#include "Sample.hpp"

#include "Tools/MetricsManager.hpp"

void PlayerObservation::Serialize(StrifeML::ObjectSerializer& serializer)
{
    serializer
        .Add(position, "position")
        .Add(velocity, "velocity")
        .Add(health, "health");
}


void MinionObservation::Serialize(StrifeML::ObjectSerializer& serializer)
{
    serializer
        .Add(position, "position")
        .Add(velocity, "velocity")
        .Add(health, "health");
}

void BuildingObservation::Serialize(StrifeML::ObjectSerializer& serializer)
{
    serializer
        .Add(position, "position")
        .Add(health, "health");
}

void Observation::Serialize(StrifeML::ObjectSerializer& serializer)
{
    serializer
        .Add(players, "players")
        .Add(minions, "minions")
        .Add(buildings, "buildings");
}

void TrainingLabel::Serialize(StrifeML::ObjectSerializer& serializer)
{
    serializer
        .Add(actionIndex, "action")
        .Add(moveCoord, "move")
        .Add(entityChoice, "entity");
}

PlayerNetwork::PlayerNetwork()
    : NeuralNetwork<Observation, TrainingLabel>(1)
{
    playerEmbed1 = module->register_module("playerEmbed1", torch::nn::Linear(5, 6));
    playerEmbed2 = module->register_module("playerEmbed2", torch::nn::Linear(6, 12));
    playerEmbed3 = module->register_module("playerEmbed3", torch::nn::Linear(12, 24));

    minionEmbed1 = module->register_module("minionEmbed1", torch::nn::Linear(5, 6));
    minionEmbed2 = module->register_module("minionEmbed2", torch::nn::Linear(6, 12));
    minionEmbed3 = module->register_module("minionEmbed3", torch::nn::Linear(12, 24));

    buildingEmbed1 = module->register_module("buildingEmbed1", torch::nn::Linear(3, 6));
    buildingEmbed2 = module->register_module("buildingEmbed2", torch::nn::Linear(6, 12));
    buildingEmbed3 = module->register_module("buildingEmbed3", torch::nn::Linear(12, 24));

    action1 = module->register_module("action1", torch::nn::Linear(72, 72));
    action2 = module->register_module("action2", torch::nn::Linear(72, 72));
    action3 = module->register_module("action3", torch::nn::Linear(72, 3));

    move1 = module->register_module("move1", torch::nn::Linear(72, 72));
    move2 = module->register_module("move2", torch::nn::Linear(72, 72));
    move3 = module->register_module("move3", torch::nn::Linear(72, 2));

    entity1 = module->register_module("entity1", torch::nn::Linear(72, 72));
    entity2 = module->register_module("entity2", torch::nn::Linear(72, 72));
    entity3 = module->register_module("entity3", torch::nn::Linear(72, 3));

    optimizer = std::make_shared<torch::optim::Adam>(module->parameters(), 1e-3);
    module->to(device);
}

FixedSizeGrid<float, 4, 5> ConvertPlayer(const Observation sample)
{
    FixedSizeGrid<float, 4, 5> grid;

    for (int i = 0; i < sample.players.size(); i++)
    {
        grid[i][0] = sample.players[i].position.x;
        grid[i][1] = sample.players[i].position.y;
        grid[i][2] = sample.players[i].velocity.x;
        grid[i][3] = sample.players[i].velocity.y;
        grid[i][4] = sample.players[i].health;
    }

	for (int i = sample.players.size(); i < 4; i++)
    {
        grid[i][0] = 0;
        grid[i][1] = 0;
        grid[i][2] = 0;
        grid[i][3] = 0;
        grid[i][4] = 0;
    }

    return grid;
}

FixedSizeGrid<float, 12, 5> ConvertMinion(const Observation sample)
{
    FixedSizeGrid<float, 12, 5> grid;

    for (int i = 0; i < sample.minions.size(); i++)
    {
        grid[i][0] = sample.minions[i].position.x;
        grid[i][1] = sample.minions[i].position.y;
        grid[i][2] = sample.minions[i].velocity.x;
        grid[i][3] = sample.minions[i].velocity.y;
        grid[i][4] = sample.minions[i].health;
    }

    for (int i = sample.minions.size(); i < 12; i++)
    {
        grid[i][0] = 0;
        grid[i][1] = 0;
        grid[i][2] = 0;
        grid[i][3] = 0;
        grid[i][4] = 0;
    }

    return grid;
}

FixedSizeGrid<float, 4, 3> ConvertBuilding(const Observation sample)
{
    FixedSizeGrid<float, 4, 3> grid;

    for (int i = 0; i < sample.buildings.size(); i++)
    {
        grid[i][0] = sample.buildings[i].position.x;
        grid[i][1] = sample.buildings[i].position.y;
        grid[i][2] = sample.buildings[i].health;
    }

    // todo there should be a faster way to fill the grid with 0s
	for (int i = sample.buildings.size(); i < 4; i++)
    {
        grid[i][0] = 0;
        grid[i][1] = 0;
        grid[i][2] = 0;
    }

    return grid;
}

FixedSizeGrid<float, 1, 2> ConvertMove(const Vector2 coord)
{
    FixedSizeGrid<float, 1, 2> grid;

    grid[0][0] = coord.x;
    grid[0][1] = coord.y;

    return grid;
}

void PlayerNetwork::TrainBatch(Grid<const SampleType> input, StrifeML::TrainingBatchResult& outResult)
{
    //Log("Train batch start\n");
    optimizer->zero_grad();

    //Log("Pack spatial\n");
    torch::Tensor playerInput = PackIntoTensor(input, [=](auto& sample) { return ConvertPlayer(sample.input); }).to(device);
    torch::Tensor minionInput = PackIntoTensor(input, [=](auto& sample) { return ConvertMinion(sample.input); }).to(device);
    torch::Tensor buildingInput = PackIntoTensor(input, [=](auto& sample) { return ConvertBuilding(sample.input); }).to(device);

    //Log("Pack labels\n");
    torch::Tensor actionLabel = PackIntoTensor(input, [=](auto& sample) { return static_cast<int64_t>(sample.output.actionIndex); }).to(device).squeeze();
    torch::Tensor moveLabel = PackIntoTensor(input, [=](auto& sample) { return ConvertMove(sample.output.moveCoord); }).to(device).squeeze();
    torch::Tensor entityLabel = PackIntoTensor(input, [=](auto& sample) { return static_cast<int64_t>(sample.output.entityChoice); }).to(device).squeeze();

    //Log("Predicting...\n");
    auto prediction = Forward(playerInput, minionInput, buildingInput);

    //std::cout << std::get<0>(prediction) << std::endl;
    //std::cout << actionLabel << std::endl;

    //Log("Calculate loss\n");
    auto actionPrediction = std::get<0>(prediction);

    //std::cout << actionPrediction.sizes() << std::endl;
    //std::cout << actionLabel << std::endl;

    torch::Tensor choiceLoss = torch::nn::functional::nll_loss(actionPrediction, actionLabel);
    
    auto movePrediction = std::get<1>(prediction);
    auto entityPrediction = std::get<2>(prediction);

    //std::cout << "move: " << std::endl << movePrediction << std::endl;
    //std::cout << moveLabel << std::endl;

    //std::cout << "attack: " << std::endl << entityPrediction << std::endl;
    //std::cout << entityLabel << std::endl;

    torch::Tensor moveLoss = torch::nn::functional::mse_loss(movePrediction, moveLabel, torch::nn::MSELossOptions(torch::kNone));
    torch::Tensor entityLoss = torch::nn::functional::nll_loss(entityPrediction, entityLabel, torch::nn::NLLLossOptions().reduction(torch::kNone));

    for (int i = 0; i < actionLabel.size(0); i++)
    {
        auto actionId = actionLabel.index({ i }).item<int64_t>();

        //std::cout << actionId << std::endl; 

        try
        {
            if (actionId != 1)
            {
                moveLoss[i] *= 0;
            }

            if (actionId != 2)
            {
                entityLoss[i] *= 0;
            }
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << std::endl;
            throw;
        }
    }

    //std::cout << "losses: " << std::endl << moveLoss << std::endl;
    //std::cout << entityLoss << std::endl;

    moveLoss = mean(moveLoss);
    entityLoss = mean(entityLoss);

    //std::cout << moveLoss << std::endl;
    //std::cout << entityLoss << std::endl;

    torch::Tensor loss = choiceLoss + moveLoss + entityLoss;

    if (loss.item<float>() > 10.0f)
    {
        std::cout << "move: " << std::endl << movePrediction << std::endl;
        std::cout << moveLabel << std::endl;
        std::cout << "attack: " << std::endl << entityPrediction << std::endl;
        std::cout << entityLabel << std::endl;
        std::cout << "losses: " << std::endl << moveLoss << std::endl;
        std::cout << entityLoss << std::endl;
    }

    //Log("Call backward\n");
    loss.backward();

    //Log("Call optimizer step\n");
    optimizer->step();

    outResult.loss = loss.item<float>();

    //Log("Train batch end\n");
}

void PlayerNetwork::MakeDecision(Grid<const InputType> input, gsl::span<OutputType> output)
{
    try
    {
        torch::Device cpu(torch::kCPU);
        module->to(cpu);
        module->eval();
        auto playerInput = PackIntoTensor(input, [=](auto& sample) { return ConvertPlayer(sample); });
        auto minionInput = PackIntoTensor(input, [=](auto& sample) { return ConvertMinion(sample); });
        auto buildingInput = PackIntoTensor(input, [=](auto& sample) { return ConvertBuilding(sample); });
        auto action = Forward(playerInput, minionInput, buildingInput);

        //std::cout << "choice: " << std::endl << std::get<0>(action) << std::endl;
        //std::cout << "move: " << std::endl << std::get<1>(action) << std::endl;
        //std::cout << "attack: " << std::endl << std::get<2>(action) << std::endl;

        for (int i = 0; i < output.size(); ++i)
        {
            torch::Tensor index = std::get<1>(torch::max(std::get<0>(action).index({ i }), 0));
            output[i].actionIndex = *index.data_ptr<int64_t>();

            torch::Tensor move = std::get<1>(action).index({ i });
            output[i].moveCoord.x = *move.index({ 0 }).data_ptr<float>();
            output[i].moveCoord.y = *move.index({ 1 }).data_ptr<float>(); 

            //std::cout << output[i].moveCoord.x << ", " << output[i].moveCoord.y << std::endl;

            torch::Tensor entityIndex = std::get<1>(torch::max(std::get<2>(action).index({ i }), 0));
            output[i].entityChoice = *entityIndex.data_ptr<int64_t>();
        }
    }
    catch (const std::exception& e)
    {
        for (int i = 0; i < output.size(); ++i)
        {
            output[i].actionIndex = 0;
            output[i].moveCoord = Vector2(0, 0);
            output[i].entityChoice = 0;
        }

        std::cout << e.what() << std::endl;
        throw;
    }
}

torch::Tensor PlayerNetwork::PartialForward(const torch::Tensor& input, torch::nn::Linear layer1, torch::nn::Linear layer2, torch::nn::Linear layer3)
{
    try 
    {
        torch::Tensor x;
        x = relu(layer1->forward(input));
        x = relu(layer2->forward(x));
        x = layer3->forward(x);  // todo brendan do we need relu here?
        x = sum(x, 2).squeeze();
        
        return x;
    }
    catch (const std::exception& e) 
    {
        std::cout << e.what() << std::endl;
        throw;
    }
}

std::tuple<torch::Tensor, torch::Tensor, torch::Tensor> PlayerNetwork::Forward(const torch::Tensor& playerInput, const torch::Tensor& minionInput, const torch::Tensor& buildingInput)
{
    //std::cout << "player: " << playerInput << std::endl;
    //std::cout << "minion: " << minionInput << std::endl;
    //std::cout << "building: " << buildingInput << std::endl;

    torch::Tensor pEmbed = PartialForward(playerInput, playerEmbed1, playerEmbed2, playerEmbed3);
    torch::Tensor mEmbed = PartialForward(minionInput, minionEmbed1, minionEmbed2, minionEmbed3);
    torch::Tensor bEmbed = PartialForward(buildingInput, buildingEmbed1, buildingEmbed2, buildingEmbed3);

    torch::Tensor postRep = torch::cat({pEmbed, mEmbed, bEmbed}, 1);

    //std::cout << "postRep-mean: " << std::endl;
	//std::cout << mean(postRep, 1) << std::endl;
    
    torch::Tensor action = relu(action1->forward(postRep));
    action = relu(action2->forward(action));
    action = action3->forward(action);

    torch::Tensor move = relu(move1->forward(postRep));
    move = relu(move2->forward(move));
    move = move3->forward(move);

    torch::Tensor entity = relu(entity1->forward(postRep));
    entity = relu(entity2->forward(entity));
    entity = entity3->forward(entity);

    action = log_softmax(action, 1).squeeze();
    move = sigmoid(move).squeeze();
    entity = log_softmax(entity, 1).squeeze();

    return std::make_tuple(action, move, entity);
}

PlayerTrainer::PlayerTrainer(Metric* lossMetric)
    : Trainer<PlayerNetwork>(32, 10000, 1),
    lossMetric(lossMetric)
{
    LogStartup();
    samples = sampleRepository.CreateSampleSet("player-samples");
    samplesByActionType = samples
        ->CreateGroupedView<int>()
        ->GroupBy([=](const SampleType& sample) { return sample.output.actionIndex; });
}

void PlayerTrainer::LogStartup() const
{
    std::cout << "Trainer starting" << std::endl;

    std::cout << std::boolalpha;
    std::cout << "CUDA is available: " << torch::cuda::is_available() << std::endl;

    std::cout << torch::show_config() << std::endl;
    std::cout << "Torch Inter-Op Threads: " << torch::get_num_interop_threads() << std::endl;
    std::cout << "Torch Intra-Op Threads: " << torch::get_num_threads() << std::endl;
}

void PlayerTrainer::ReceiveSample(const SampleType& sample) 
{
    samples->AddSample(sample);
}

bool PlayerTrainer::TrySelectSequenceSamples(gsl::span<SampleType> outSequence) 
{
    return samplesByActionType->TryPickRandomSequence(outSequence);
}

void PlayerTrainer::OnTrainingComplete(const StrifeML::TrainingBatchResult& result)
{
    lossMetric->Add(result.loss);
}