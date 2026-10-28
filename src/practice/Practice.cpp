#include "Practice.hpp"

#include <unordered_map>

using namespace geode::prelude;

namespace xbot::practice {

namespace {

std::unordered_map<CheckpointObject*, CheckpointData>& checkpoints() {
	static std::unordered_map<CheckpointObject*, CheckpointData> map;
	return map;
}

}

PlayerData save(PlayerObject* player) {
	PlayerData data;
	data.position = player->getPosition();
	data.rotation = player->getRotation();
#define X(name) data.name = player->name;
	XBOT_PLAYER_FIELDS(X)
#undef X
	return data;
}

void apply(PlayerObject* player, PlayerData const& data) {
#define X(name) player->name = data.name;
	XBOT_PLAYER_FIELDS(X)
#undef X
	player->setPosition(data.position);
	player->setRotation(data.rotation);
}

void remember(CheckpointObject* checkpoint, PlayerObject* player1, PlayerObject* player2) {
	if (!checkpoint || !player1) return;
	CheckpointData data;
	data.player1 = save(player1);
	if (player2) data.player2 = save(player2);
	checkpoints()[checkpoint] = std::move(data);
}

CheckpointData const* find(CheckpointObject* checkpoint) {
	auto& map = checkpoints();
	auto it = map.find(checkpoint);
	return it == map.end() ? nullptr : &it->second;
}

void clear() {
	checkpoints().clear();
}

}
