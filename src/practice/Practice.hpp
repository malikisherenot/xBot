#pragma once

#include "Fields.hpp"

#include <Geode/Geode.hpp>

#include <type_traits>

namespace xbot::practice {

struct PlayerData {
	cocos2d::CCPoint position;
	float rotation = 0.f;
#define X(name) std::decay_t<decltype(PlayerObject::name)> name{};
	XBOT_PLAYER_FIELDS(X)
#undef X
};

struct CheckpointData {
	PlayerData player1;
	PlayerData player2;
};

PlayerData save(PlayerObject* player);
void apply(PlayerObject* player, PlayerData const& data);

void remember(CheckpointObject* checkpoint, PlayerObject* player1, PlayerObject* player2);
CheckpointData const* find(CheckpointObject* checkpoint);
void clear();

}
