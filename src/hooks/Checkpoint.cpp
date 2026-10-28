#include "../core/Bot.hpp"
#include "../practice/Practice.hpp"

#include <Geode/modify/CheckpointObject.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;
using namespace xbot;

class $modify(XCheckpoint, CheckpointObject) {
#ifdef GEODE_IS_WINDOWS
	bool init() {
		bool result = CheckpointObject::init();
		if (auto layer = PlayLayer::get()) {
			practice::remember(this, layer->m_player1, layer->m_player2);
		}
		return result;
	}
#else
	static CheckpointObject* create() {
		auto result = CheckpointObject::create();
		if (result) {
			if (auto layer = PlayLayer::get()) {
				practice::remember(result, layer->m_player1, layer->m_player2);
			}
		}
		return result;
	}
#endif
};

class $modify(XCheckpointLayer, PlayLayer) {
	void loadFromCheckpoint(CheckpointObject* checkpoint) {
		PlayLayer::loadFromCheckpoint(checkpoint);

		if (!checkpoint || Bot::get().mode == Mode::Idle) return;
		auto data = practice::find(checkpoint);
		if (!data) return;

		practice::apply(m_player1, data->player1);
		if (m_gameState.m_isDualMode && m_player2) {
			practice::apply(m_player2, data->player2);
		}
	}
};
