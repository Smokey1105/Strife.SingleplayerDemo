#pragma once

#include "PlayerNeuralNetworkService.hpp"
#include "InputService.hpp"
#include "MinionEntity.hpp"
#include "CastleEntity.hpp"

PlayerNeuralNetworkService::PlayerNeuralNetworkService(StrifeML::NetworkContext<PlayerNetwork>* context, InputService* inputService)
	: NeuralNetworkService<PlayerEntity, PlayerNetwork>(context, 128),
	inputService(inputService)
{
}

void PlayerNeuralNetworkService::CollectInput(PlayerEntity* entity, InputType& input)
{
	entity->GetObservation(input);
}

void PlayerNeuralNetworkService::ReceiveDecision(PlayerEntity* entity, OutputType& output)
{
	//output.actionIndex = 0 = none = continue previous action
	PlayerEntity* player;
	if (inputService->activePlayer.TryGetValue(player) && player == entity)
	{
		return;
	}

	if (output.actionIndex == 1)
	{
		entity->MoveTo(Vector2(output.moveCoord.x * 4096.0f + 32.0f, output.moveCoord.y * 1408.0f + 1376.0f));
	}
	else if (output.actionIndex == 2)
	{
		switch (output.entityChoice)
		{
			case 0:
			{
				auto player = std::get<0>(entity->NearestEntityOfType<PlayerEntity>());
				entity->Attack(player);
			}
			break;
			case 1:
			{
				auto minion = std::get<0>(entity->NearestEntityOfType<MinionEntity>());
				entity->Attack(minion);
			}
			break;
			case 2:
			{
				auto towerTuple = entity->NearestEntityOfType<TowerEntity>();
				auto castleTuple = entity->NearestEntityOfType<CastleEntity>();

				if (std::get<1>(castleTuple) < std::get<1>(towerTuple))
				{
					entity->Attack(std::get<0>(castleTuple));
				}
				else
				{
					entity->Attack(std::get<0>(towerTuple));
				}
			}
			break;
		}
	}
}

void PlayerNeuralNetworkService::CollectTrainingSamples(TrainerType* trainer)
{
	PlayerEntity* player;
	if (inputService->activePlayer.TryGetValue(player))
	{
		SampleType sample;
		CollectInput(player, sample.input);

		sample.output.actionIndex = static_cast<int>(player->state);
		sample.output.moveCoord = Vector2(0.0f, 0.0f);
		sample.output.entityChoice = 0;

		if (player->state == PlayerState::Moving)
		{
			sample.output.moveCoord = Vector2((player->lastMoveVector.x -  32.0f)/4096.0f, (player->lastMoveVector.y - 1376.0f)/1408.0f);
		}
		else if (player->state == PlayerState::Attacking)
		{
			auto target = player->attackTarget.GetValueOrNull();
			if (target != nullptr)
			{
				if (target->Is<PlayerEntity>())
				{
					sample.output.entityChoice = 0;
				}
				else if (target->Is<MinionEntity>())
				{
					sample.output.entityChoice = 1;
				}
				else
				{
					sample.output.entityChoice = 2;
				}
			}
		}

		trainer->AddSample(sample);
	}
}
