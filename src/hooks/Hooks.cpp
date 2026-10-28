#include "../core/Bot.hpp"
#include "../practice/Practice.hpp"

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
			if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(50)) break;
		}

		bot.accumulator = std::clamp(total - done * static_cast<double>(step), 0.0, 1.0);
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

	void processQueuedButtons(float dt, bool clearInputQueue) {
		auto& bot = Bot::get();
		if (m_isEditor || !bot.inLevel() || bot.mode == Mode::Idle) {
			GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
			return;
		}

		auto frame = static_cast<uint32_t>(m_gameState.m_currentProgress);
		bool alive = m_player1 && !m_player1->m_isDead && !m_levelEndAnimationStarted;

		if (bot.mode == Mode::Replay) {
			m_queuedButtons.clear();
			if (alive) bot.inject(this, frame);
		}
		else if (bot.mode == Mode::Record && alive && !bot.resetting) {
			for (auto const& command : m_queuedButtons) {
				bot.record(frame, static_cast<int>(command.m_button), command.m_isPush, command.m_isPlayer2);
			}
		}

		GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
	}

	void processCommands(float dt, bool isHalfTick, bool isLastTick) {
		auto& bot = Bot::get();
		if (!m_isEditor && bot.inLevel()) {
			if (bot.restart) {
				bot.restart = false;
				PlayLayer::get()->resetLevelFromStart();
				return;
			}
		}
		GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
	}
};

class $modify(XPlayLayer, PlayLayer) {
	void resetLevel() {
		auto& bot = Bot::get();
		bot.resetting = true;
		PlayLayer::resetLevel();
		bot.resetting = false;
		m_extraDelta = 0.0;
		bot.accumulator = 0.0;
		bot.rewind(static_cast<uint32_t>(m_gameState.m_currentProgress));
		bot.applyAudio();

		if (bot.mode == Mode::Record && m_uiLayer && m_player1) {
			bool twoPlayer = m_levelSettings->m_twoPlayerMode;
			bool holding1 = m_uiLayer->m_p1Jumping || m_uiLayer->m_p1TouchId != -1;
			bool holding2 = m_uiLayer->m_p2Jumping || m_uiLayer->m_p2TouchId != -1;
			holding1 = holding1 || (holding2 && !twoPlayer);

			if (holding1 != m_player1->m_holdingButtons[1]) {
				queueButton(1, holding1, false, 0.0);
			}
			if (twoPlayer && m_player2 && holding2 != m_player2->m_holdingButtons[1]) {
				queueButton(1, holding2, true, 0.0);
			}
		}
	}

	void onQuit() {
		auto& bot = Bot::get();
		bot.finishSession();
		bot.resetAudio();
		xbot::practice::clear();
		PlayLayer::onQuit();
	}

	void levelComplete() {
		Bot::get().finishSession();
		PlayLayer::levelComplete();
	}

	void togglePracticeMode(bool practice) {
		if (!practice) Bot::get().finishSession();
		PlayLayer::togglePracticeMode(practice);
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
