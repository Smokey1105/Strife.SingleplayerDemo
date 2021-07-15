#pragma once

#include "Math/Vector2.hpp"
#include "ML/ML.hpp"
#include "TensorPacking.hpp"
#include <torch/torch.h>
#include "ML/GridSensor.hpp"

#include "Tools/MetricsManager.hpp"

struct PlayerObservation : StrifeML::ISerializable
{
    void Serialize(StrifeML::ObjectSerializer& serializer) override;

    Vector2 position;
    Vector2 velocity;
    float health;
};

struct MinionObservation : StrifeML::ISerializable
{
    void Serialize(StrifeML::ObjectSerializer& serializer) override;

    Vector2 position;
    Vector2 velocity;
    float health;
};

struct BuildingObservation : StrifeML::ISerializable
{
    void Serialize(StrifeML::ObjectSerializer& serializer) override;

    Vector2 position;
    float health;
};

struct Observation : StrifeML::ISerializable
{
    void Serialize(StrifeML::ObjectSerializer& serializer) override;

    //std::vector<PlayerObservation> players;
    //std::vector<MinionObservation> minions;
    //std::vector<BuildingObservation> buildings;

    //temp bug fix:
    PlayerObservation nearestPlayer;
    MinionObservation nearestMinion;
    BuildingObservation nearestBuilding;
};

struct TrainingLabel : StrifeML::ISerializable
{
    void Serialize(StrifeML::ObjectSerializer& serializer) override;

    int actionIndex;
};

struct PlayerNetwork : StrifeML::NeuralNetwork<Observation, TrainingLabel>
{
    torch::nn::Linear playerEmbed1{ nullptr }, playerEmbed2{ nullptr }, playerEmbed3{ nullptr };
    torch::nn::Linear minionEmbed1{ nullptr }, minionEmbed2{ nullptr }, minionEmbed3{ nullptr };
    torch::nn::Linear buildingEmbed1{ nullptr }, buildingEmbed2{ nullptr }, buildingEmbed3{ nullptr };

    torch::nn::Linear dense1{ nullptr }, dense2{ nullptr }, dense3{ nullptr };
    torch::Device device = torch::Device(torch::kCUDA);
    std::shared_ptr<torch::optim::Adam> optimizer;

    PlayerNetwork();

    void TrainBatch(Grid<const SampleType> input, StrifeML::TrainingBatchResult& outResult) override;
    void MakeDecision(Grid<const InputType> input, gsl::span<OutputType> output) override;
    torch::Tensor PlayerNetwork::PartialForward(const torch::Tensor& input, torch::nn::Linear layer1, torch::nn::Linear layer2, torch::nn::Linear layer3);
    torch::Tensor Forward(const torch::Tensor& playerInput, const torch::Tensor& minionInput, const torch::Tensor& buildingInput);
};

struct PlayerDecider : StrifeML::Decider<PlayerNetwork>
{

};

struct PlayerTrainer : StrifeML::Trainer<PlayerNetwork>
{
    PlayerTrainer(Metric* lossMetric);

    void LogStartup() const;
    void ReceiveSample(const SampleType& sample) override;
    bool TrySelectSequenceSamples(gsl::span<SampleType> outSequence) override;
    void OnTrainingComplete(const StrifeML::TrainingBatchResult& result) override;

    StrifeML::SampleSet<SampleType>* samples;
    StrifeML::GroupedSampleView<SampleType, int>* samplesByActionType;
    Metric* lossMetric;
};