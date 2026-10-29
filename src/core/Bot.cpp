#include "Bot.hpp"
#include "../formats/Formats.hpp"

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
	tps = std::max(1.f, mod->getSavedValue<float>("tps", 240.f));
	autoSave = mod->getSavedValue<bool>("autosave", true);
	frameOffset = mod->getSavedValue<int>("frameoffset", 0);
	overwrite = mod->getSavedValue<bool>("overwrite", true);
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
		restart = value != Mode::Idle;
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

void Bot::setTps(double value) {
	tps = static_cast<float>(std::max(1.0, value));
	Mod::get()->setSavedValue("tps", tps);
}

void Bot::setName(std::string const& value) {
	name = value;
	Mod::get()->setSavedValue("name", name);
}

void Bot::setFrameOffset(int value) {
	frameOffset = std::clamp(value, -1000, 1000);
	Mod::get()->setSavedValue("frameoffset", frameOffset);
}

void Bot::setAutoSave(bool value) {
	autoSave = value;
	Mod::get()->setSavedValue("autosave", autoSave);
}

void Bot::setOverwrite(bool value) {
	overwrite = value;
	Mod::get()->setSavedValue("overwrite", overwrite);
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
	if (!macro.inputs.empty()) {
		auto const& last = macro.inputs.back();
		if (last.frame == frame && last.button == button && last.down == down && last.player2 == player2) return;
	}
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
	auto target = static_cast<uint32_t>(std::max<int64_t>(0, static_cast<int64_t>(frame) - frameOffset));
	auto it = std::lower_bound(
		inputs.begin(), inputs.end(), target,
		[](Input const& input, uint32_t value) { return input.frame < value; }
	);
	m_cursor = static_cast<size_t>(it - inputs.begin());
}

void Bot::press(GJBaseGameLayer* layer, bool down, int button, bool player2) {
	layer->queueButton(button, down, player2, 0.0);
}

void Bot::inject(GJBaseGameLayer* layer, uint32_t frame) {
	auto& inputs = macro.inputs;
	int64_t current = static_cast<int64_t>(frame) - frameOffset;
	if (current < 0) return;

	while (m_cursor < inputs.size() && static_cast<int64_t>(inputs[m_cursor].frame) <= current) {
		auto const& input = inputs[m_cursor++];
		press(layer, input.down, input.button, input.player2);
	}
}

std::filesystem::path Bot::dir() const {
	auto path = geode::dirs::getGeodeDir() / "game";
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
	return formats::save(path, macro, id, levelName);
}

std::string Bot::uniqueName(std::string const& base) const {
	auto exists = [&](std::string const& candidate) {
		std::error_code ec;
		return std::filesystem::exists(dir() / (candidate + ".gdr"), ec);
	};
	if (!exists(base)) return base;
	for (int i = 1; i < 100000; i++) {
		auto candidate = base + "_" + std::to_string(i);
		if (!exists(candidate)) return candidate;
	}
	return base;
}

void Bot::finishSession() {
	if (mode != Mode::Record) return;
	mode = Mode::Idle;
	if (macro.inputs.empty()) return;

	if (!autoSave) {
		status = "Recording stopped (not saved)";
		return;
	}

	std::string base = name.empty() ? "macro" : name;
	auto path = std::filesystem::path(base);
	if (path.has_extension()) base = utils::string::pathToString(path.stem());
	if (!overwrite) base = uniqueName(base);

	status = save(base) ? "Auto-saved: " + base : "Auto-save failed";
}

bool Bot::load(std::string const& file) {
	if (file.empty()) return false;
	auto path = dir() / file;
	if (!path.has_extension()) path += ".gdr";

	Macro loaded;
	if (!formats::load(path, loaded)) return false;

	macro = std::move(loaded);
	setTps(macro.tps);

	if (auto layer = PlayLayer::get()) {
		rewind(layer->m_gameState.m_currentProgress);
	}
	else {
		m_cursor = 0;
	}
	return true;
}

}
