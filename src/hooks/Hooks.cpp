#include "../core/Bot.hpp"

#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

using namespace geode::prelude;
using namespace xbot;

class $modify(XScheduler, CCScheduler) {
	void update(float dt) {
		auto& bot = Bot::get();
		if (bot.speed != 1.f && this == CCDirector::get()->getScheduler() && bot.inLevel()) {
			dt *= bot.speed;
		}
		CCScheduler::update(dt);
	}
};

class $modify(XGameLayer, GJBaseGameLayer) {
	static void onModify(auto& self) {
		(void)self.setHookPriority("GJBaseGameLayer::update", Priority::First);
		(void)self.setHookPriority("GJBaseGameLayer::getModifiedDelta", Priority::First);
	}

	void update(float dt) {
		auto& bot = Bot::get();
		if (m_isEditor || !bot.inLevel()) {
			GJBaseGameLayer::update(dt);
			return;
		}

		float step = bot.stepDelta();

		if (bot.frozen) {
			if (bot.pendingSteps > 0) {
				bot.pendingSteps--;
				GJBaseGameLayer::update(step);
			}
			return;
		}

		bot.pendingSteps = 0;

		if (bot.tps == 240.f) {
			GJBaseGameLayer::update(dt);
			return;
		}

		double total = static_cast<double>(dt) + bot.accumulator;
		int steps = static_cast<int>(std::floor(total / step + 1e-6));

		auto start = std::chrono::steady_clock::now();
		int done = 0;
		while (done < steps) {
			GJBaseGameLayer::update(step);
			done++;
			if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(16)) break;
		}

		bot.accumulator = done < steps ? 0.0 : std::max(0.0, total - done * static_cast<double>(step));
	}

	double getModifiedDelta(float dt) {
		auto& bot = Bot::get();
		if (m_isEditor || !bot.inLevel() || bot.tps == 240.f) {
			return GJBaseGameLayer::getModifiedDelta(dt);
		}

		double step = bot.stepDelta();

		if (m_resumeTimer > 0) {
			m_resumeTimer--;
			dt = 0.f;
		}

		double warp = std::min(static_cast<double>(m_gameState.m_timeWarp), 1.0);
		double total = static_cast<double>(dt) + m_extraDelta;
		double applied = std::round(total / (warp * step)) * warp * step;
		m_extraDelta = total - applied;
		return applied;
	}

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
		if (!m_isEditor && bot.inLevel()) {
			if (bot.restart) {
				bot.restart = false;
				PlayLayer::get()->resetLevelFromStart();
				return;
			}
			if (bot.mode == Mode::Replay) {
				bot.inject(this, static_cast<uint32_t>(m_gameState.m_currentProgress));
			}
		}
		GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
	}
};

class $modify(XPlayLayer, PlayLayer) {
	void resetLevel() {
		PlayLayer::resetLevel();
		auto& bot = Bot::get();
		m_extraDelta = 0.0;
		bot.accumulator = 0.0;
		bot.rewind(static_cast<uint32_t>(m_gameState.m_currentProgress));
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
