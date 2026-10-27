#include "Bot.hpp"
#include "src/formats/Formats.hpp"

#include <algorithm>

using namespace geode::prelude;

namespace xbot {

Bot& Bot::get() {
	static Bot instance;
	return instance;
}

bool Bot::inLevel() const {
	return PlayLayer::get() != nullptr;
}

bool Bot::vanilla() const {
	return !frozen && speed == 1.f && tps == 240.f;
}

float Bot::stepDelta() const {
	return 1.f / tps;
}

void Bot::restore() {
	auto mod = Mod::get();
	speed = mod->getSavedValue<float>("speed", 1.f);
	tps = mod->getSavedValue<float>("tps", 240.f);
	name = mod->getSavedValue<std::string>("name", "macro");
}

void Bot::setMode(Mode value) {
	mode = value;
	if (value == Mode::Record) {
		macro.inputs.clear();
		macro.tps = tps;
	}
	if (auto layer = PlayLayer::get()) {
		rewind(layer->m_gameState.m_currentProgress);
	}
	else {
		m_cursor = 0;
	}
}

void Bot::setSpeed(float value) {
	speed = std::clamp(value, 0.01f, 10.f);
	Mod::get()->setSavedValue("speed", speed);
	applyAudio();
}

void Bot::setTps(int value) {
	tps = static_cast<float>(std::clamp(value, 1, 10000));
	Mod::get()->setSavedValue("tps", tps);
}

void Bot::applyAudio() {
	auto engine = FMODAudioEngine::sharedEngine();
	if (!engine || !engine->m_globalChannel) return;
	bool active = inLevel();
	engine->m_globalChannel->setPitch(active ? speed : 1.f);
	engine->m_globalChannel->setPaused(active && frozen);
}

void Bot::resetAudio() {
	auto engine = FMODAudioEngine::sharedEngine();
	if (!engine || !engine->m_globalChannel) return;
	engine->m_globalChannel->setPitch(1.f);
	engine->m_globalChannel->setPaused(false);
}

void Bot::record(uint32_t frame, int button, bool down, bool player2) {
	macro.inputs.push_back({frame, button, down, player2});
}

void Bot::rewind(uint32_t frame) {
	auto& inputs = macro.inputs;
	if (mode == Mode::Record) {
		while (!inputs.empty() && inputs.back().frame >= frame) {
			inputs.pop_back();
		}
		return;
	}
	auto it = std::lower_bound(
		inputs.begin(), inputs.end(), frame,
		[](Input const& input, uint32_t value) { return input.frame < value; }
	);
	m_cursor = static_cast<size_t>(it - inputs.begin());
	m_resync = true;
}

void Bot::press(GJBaseGameLayer* layer, bool down, int button, bool player2) {
	injecting = true;
	layer->handleButton(down, button, !player2);
	injecting = false;
}

void Bot::inject(GJBaseGameLayer* layer, uint32_t frame) {
	auto& inputs = macro.inputs;

	if (m_resync) {
		m_resync = false;
		bool held[4][2] = {};
		for (size_t i = 0; i < m_cursor && i < inputs.size(); i++) {
			auto const& input = inputs[i];
			if (input.button >= 1 && input.button <= 3) {
				held[input.button][input.player2 ? 1 : 0] = input.down;
			}
		}
		for (int button = 1; button <= 3; button++) {
			for (int side = 0; side < 2; side++) {
				if (held[button][side]) press(layer, true, button, side == 1);
			}
		}
	}

	while (m_cursor < inputs.size() && inputs[m_cursor].frame <= frame) {
		auto const& input = inputs[m_cursor++];
		press(layer, input.down, input.button, input.player2);
	}
}

std::filesystem::path Bot::dir() const {
	auto path = Mod::get()->getSaveDir() / "macros";
	std::error_code ec;
	std::filesystem::create_directories(path, ec);
	return path;
}

std::vector<std::string> Bot::list() const {
	std::vector<std::string> files;
	std::error_code ec;
	for (auto const& entry : std::filesystem::directory_iterator(dir(), ec)) {
		if (entry.is_regular_file() && formats::supported(entry.path())) {
			files.push_back(utils::string::pathToString(entry.path().filename()));
		}
	}
	std::sort(files.begin(), files.end());
	return files;
}

bool Bot::save(std::string const& file) {
	if (file.empty()) return false;
	auto path = dir() / file;
	if (!path.has_extension()) path += ".gdr";

	int id = 0;
	std::string levelName;
	if (auto layer = PlayLayer::get()) {
		id = layer->m_level->m_levelID.value();
		levelName = layer->m_level->m_levelName;
	}

	macro.tps = tps;
	Mod::get()->setSavedValue("name", file);
	return formats::save(path, macro, id, levelName);
}

bool Bot::load(std::string const& file) {
	if (file.empty()) return false;
	auto path = dir() / file;
	if (!path.has_extension()) path += ".gdr";

	Macro loaded;
	if (!formats::load(path, loaded)) return false;

	macro = std::move(loaded);
	setTps(static_cast<int>(macro.tps));
	Mod::get()->setSavedValue("name", file);

	if (auto layer = PlayLayer::get()) {
		rewind(layer->m_gameState.m_currentProgress);
	}
	else {
		m_cursor = 0;
	}
	return true;
}

}
