#pragma once

#include "PlayerFields.hpp"

#include <Geode/Geode.hpp>

#include <type_traits>
#include <utility>

namespace xbot::practice {

namespace detail {

struct Missing {};

#define X(name) \
	template <class P> auto type_##name(int) -> std::decay_t<decltype(std::declval<P&>().name)>; \
	template <class P> Missing type_##name(...);
XBOT_PLAYER_FIELDS(X)
#undef X

}

struct PlayerData {
	cocos2d::CCPoint position;
	float rotation = 0.f;
#define X(name) decltype(detail::type_##name<PlayerObject>(0)) name{};
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
