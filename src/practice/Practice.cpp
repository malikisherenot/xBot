#include "Practice.hpp"

#include <map>
#include <memory>

using namespace geode::prelude;

namespace xbot::practice {

namespace {

std::map<CheckpointObject*, std::unique_ptr<CheckpointData>>& checkpoints() {
	static std::map<CheckpointObject*, std::unique_ptr<CheckpointData>> map;
	return map;
}

template <class P, class D>
void copyOut(P* player, D& data) {
#define X(name) \
	if constexpr (requires { player->name; }) data.name = player->name;
	XBOT_PLAYER_FIELDS(X)
#undef X
}

template <class P, class D>
void copyIn(P* player, D const& data) {
#define X(name) \
	if constexpr (requires { player->name; }) player->name = data.name;
	XBOT_PLAYER_FIELDS(X)
#undef X
}

}

PlayerData save(PlayerObject* player) {
	PlayerData data;
	data.position = player->getPosition();
	data.rotation = player->getRotation();
	copyOut(player, data);
	return data;
}

void apply(PlayerObject* player, PlayerData const& data) {
	copyIn(player, data);
	player->setPosition(data.position);
	player->setRotation(data.rotation);
}

void remember(CheckpointObject* checkpoint, PlayerObject* player1, PlayerObject* player2) {
	if (!checkpoint || !player1) return;
	auto data = std::make_unique<CheckpointData>();
	data->player1 = save(player1);
	if (player2) data->player2 = save(player2);
	checkpoints()[checkpoint] = std::move(data);
}

CheckpointData const* find(CheckpointObject* checkpoint) {
	auto& map = checkpoints();
	auto it = map.find(checkpoint);
	return it == map.end() ? nullptr : it->second.get();
}

void clear() {
	checkpoints().clear();
}

}
