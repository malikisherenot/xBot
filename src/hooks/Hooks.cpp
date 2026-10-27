#include "../core/Bot.hpp"

#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;
using namespace xbot;

class $modify(XScheduler, CCScheduler) {
	static void onModify(auto& self) {
		(void)self.setHookPriority("cocos2d::CCScheduler::update", Priority::First);
	}

	void update(float dt) {
		auto& bot = Bot::get();
		if (this != CCDirector::get()->getScheduler() || !bot.inLevel() || bot.vanilla()) {
			CCScheduler::update(dt);
			return;
		}

		float step = bot.stepDelta();

		if (bot.frozen) {
			if (bot.pendingSteps > 0) {
				bot.pendingSteps--;
				CCScheduler::update(step);
			}
			return;
		}

		bot.pendingSteps = 0;
		bot.accumulator += static_cast<double>(dt) * bot.speed;

		int ticks = 0;
		while (bot.accumulator >= step && ticks < 2048) {
			CCScheduler::update(step);
			bot.accumulator -= step;
			ticks++;
		}
		if (ticks == 2048) bot.accumulator = 0.0;
	}
};

class $modify(XGameLayer, GJBaseGameLayer) {
	void handleButton(bool down, int button, bool isPlayer1) {
		auto& bot = Bot::get();
		if (!m_isEditor && bot.inLevel()) {
			if (bot.mode == Mode::Replay && !bot.injecting) return;
			if (bot.mode == Mode::Record) {
				bot.record(static_cast<uint32_t>(m_gameState.m_currentProgress), button, down, !isPlayer1);
			}
		}
		GJBaseGameLayer::handleButton(down, button, isPlayer1);
	}

	void processCommands(float dt, bool isHalfTick, bool isLastTick) {
		auto& bot = Bot::get();
		if (!m_isEditor && bot.mode == Mode::Replay && bot.inLevel()) {
			bot.inject(this, static_cast<uint32_t>(m_gameState.m_currentProgress));
		}
		GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
	}
};

class $modify(XPlayLayer, PlayLayer) {
	void resetLevel() {
		PlayLayer::resetLevel();
		auto& bot = Bot::get();
		bot.rewind(static_cast<uint32_t>(m_gameState.m_currentProgress));
		bot.accumulator = 0.0;
		bot.applyAudio();
	}

	void onQuit() {
		Bot::get().resetAudio();
		PlayLayer::onQuit();
	}

	void destroyPlayer(PlayerObject* player, GameObject* object) {
		auto& bot = Bot::get();
		if (bot.noclip && object != m_anticheatSpike) {
			if (player == m_player1 && bot.noclipP1) return;
			if (player == m_player2 && bot.noclipP2) return;
		}
		PlayLayer::destroyPlayer(player, object);
	}
};
