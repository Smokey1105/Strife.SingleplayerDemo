#include "Math/Vector2.hpp"
#include "ML/ML.hpp"
#include "TensorPacking.hpp"
#include <torch/torch.h>
#include "ML/GridSensor.hpp"
#include "GameML.hpp"

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
        .Add(nearestPlayer, "players")
        .Add(nearestMinion, "minions")
        .Add(nearestBuilding, "buildings");
}

void TrainingLabel::Serialize(StrifeML::ObjectSerializer& serializer)
{
    serializer
        .Add(actionIndex, "action");
}

PlayerNetwork::PlayerNetwork()
    : NeuralNetwork<Observation, TrainingLabel>(1)
{
    playerEmbed1 = module->register_module("playerEmbed1", torch::nn::Linear(3, 6));
    playerEmbed2 = module->register_module("playerEmbed2", torch::nn::Linear(6, 12));
    playerEmbed3 = module->register_module("playerEmbed3", torch::nn::Linear(12, 24));

    minionEmbed1 = module->register_module("minionEmbed1", torch::nn::Linear(3, 6));
    minionEmbed2 = module->register_module("minionEmbed2", torch::nn::Linear(6, 12));
    minionEmbed3 = module->register_module("minionEmbed3", torch::nn::Linear(12, 24));

    buildingEmbed1 = module->register_module("buildingEmbed1", torch::nn::Linear(2, 4));
    buildingEmbed2 = module->register_module("buildingEmbed2", torch::nn::Linear(4, 8));
    buildingEmbed3 = module->register_module("buildingEmbed3", torch::nn::Linear(8, 16));

    dense1 = module->register_module("dense1", torch::nn::Linear(24, 24));
    dense2 = module->register_module("dense2", torch::nn::Linear(24, 24));
    dense3 = module->register_module("dense2", torch::nn::Linear(24, 9));
    optimizer = std::make_shared<torch::optim::Adam>(module->parameters(), 1e-3);
    module->to(device);
}

void PlayerNetwork::TrainBatch(Grid<const SampleType> input, StrifeML::TrainingBatchResult& outResult)
{
    //Log("Train batch start\n");
    optimizer->zero_grad();

    //Log("Pack spatial\n");
    //Changed from vectors to singletons (TEMP FIX)
    torch::Tensor playerInput = PackIntoTensor(input, [=](auto& sample) { return sample.input.closestPlayer; }).to(device);
    torch::Tensor minionInput = PackIntoTensor(input, [=](auto& sample) { return sample.input.closestMinion; }).to(device);
    torch::Tensor buildingInput = PackIntoTensor(input, [=](auto& sample) { return sample.input.closestBuilding; }).to(device);

    //Log("Pack labels\n");
    torch::Tensor labels = PackIntoTensor(input, [=](auto& sample) { return static_cast<int64_t>(sample.output.actionIndex); }).to(device).squeeze();

    //Log("Predicting...\n");
    torch::Tensor prediction = Forward(playerInput, minionInput, buildingInput).squeeze();

    std::cout << prediction.sizes() << std::endl;
    //std::cout << labels << std::endl;

    //Log("Calculate loss\n");
    torch::Tensor loss = torch::nn::functional::nll_loss(prediction, labels);

    //Log("Call backward\n");
    loss.backward();

    //Log("Call optimizer step\n");
    optimizer->step();

    outResult.loss = loss.item<float>();

    //Log("Train batch end\n");
}

void PlayerNetwork::MakeDecision(Grid<const InputType> input, gsl::span<OutputType> output)
{
    torch::Device cpu(torch::kCPU);
    module->to(cpu);
    module->eval();
    //Changed from vectors to singletons (TEMP FIX)
    auto playerInput = PackIntoTensor(input, [=](auto& sample) { return sample.closestPlayer; });
    auto minionInput = PackIntoTensor(input, [=](auto& sample) { return sample.closestMinion; });
    auto buildingInput = PackIntoTensor(input, [=](auto& sample) { return sample.closestBuilding; });
    torch::Tensor action = Forward(playerInput, minionInput, buildingInput).squeeze();

    for (int i = 0; i < output.size(); ++i)
    {
        torch::Tensor index = std::get<1>(torch::max(action.index({ i }), 0));
        output[i].actionIndex = *index.data_ptr<int64_t>();
    }
}

torch::Tensor PlayerNetwork::PartialForward(const torch::Tensor& input, torch::nn::Linear layer1, torch::nn::Linear layer2, torch::nn::Linear layer3)
{
    //std::cout << input.sizes() << std::endl;

    torch::Tensor x;

    x = relu(layer1->forward(x));

    //std::cout << x.sizes() << std::endl;

    x = relu(layer2->forward(x));
    /*x = dropout(x, 0.5, module->is_training());
    x = max_pool2d(x, { 2, 2 });*/

    //std::cout << x.sizes() << std::endl;

    x = relu(layer3->forward(x));
    /*x = dropout(x, 0.5, module->is_training());
    x = max_pool2d(x, { 2, 2 });*/

    x.unsqueeze(3);

    //std::cout << x.sizes() << std::endl;

    return x;

}

torch::Tensor PlayerNetwork::Forward(const torch::Tensor& playerInput, const torch::Tensor& minionInput, const torch::Tensor& buildingInput)
{
    torch::Tensor pEmbed = PartialForward(playerInput, playerEmbed1, playerEmbed2, playerEmbed3);
    torch::Tensor mEmbed = PartialForward(minionInput, minionEmbed1, minionEmbed2, minionEmbed3);
    torch::Tensor bEmbed = PartialForward(buildingInput, buildingEmbed1, buildingEmbed2, buildingEmbed3);

    torch::Tensor x = torch::cat({pEmbed, mEmbed, bEmbed}, 3);

    x = avg_pool1d(x, 3).squeeze();

    //if (sequenceLength > 1)
    //{
    //    x = spatialInput.view({ -1, height, width });
    //}
    //else
    //{
    //    x = squeeze(spatialInput, 1);
    //}

    x = x.permute({ 0, 3, 1, 2 }); 

    x = relu(dense1->forward(x));
    x = dropout(x, 0.5, module->is_training());
    x = max_pool2d(x, { 2, 2 });

    x = relu(dense2->forward(x));
    x = dropout(x, 0.5, module->is_training());
    x = max_pool2d(x, { 2, 2 });

    x = relu(dense3->forward(x));
    x = dropout(x, 0.5, module->is_training());
    x = max_pool2d(x, { 2, 2 });


    //if (sequenceLength > 1)
    //{
    //    x = x.view({ sequenceLength, batchSize, 128 });
    //}
    //else
    //{
    //    x = x.view({ batchSize, 128 });
    //}

    x = log_softmax(x, 1);

    return x;
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