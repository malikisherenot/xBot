#include "../core/Bot.hpp"

#include <Geode/Geode.hpp>
#include <imgui-cocos.hpp>

#include <cfloat>
#include <cstring>

using namespace geode::prelude;
using namespace xbot;

namespace {

float scale() {
#ifdef GEODE_IS_ANDROID
	return 1.7f;
#else
	return 1.f;
#endif
}

void applyStyle() {
	ImGui::StyleColorsDark();
	auto& style = ImGui::GetStyle();
	style.WindowRounding = 8.f;
	style.FrameRounding = 6.f;
	style.GrabRounding = 6.f;
	style.WindowTitleAlign = {0.5f, 0.5f};
	style.ScaleAllSizes(scale());
	ImGui::GetIO().FontGlobalScale = scale();
}

void shortcuts() {
	auto& bot = Bot::get();
	auto& io = ImGui::GetIO();
	if (io.WantTextInput) return;

	if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) bot.menuOpen = !bot.menuOpen;
	if (ImGui::IsKeyPressed(ImGuiKey_V, false)) {
		bot.frozen = !bot.frozen;
		bot.applyAudio();
	}
	if (bot.frozen && ImGui::IsKeyPressed(ImGuiKey_C, true)) bot.pendingSteps++;
}

void floatingButton() {
#ifdef GEODE_IS_ANDROID
	auto& bot = Bot::get();
	static bool dragged = false;
	float size = 56.f * scale();

	ImGui::SetNextWindowPos({20.f, 120.f}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
	ImGui::Begin(
		"##xbot-button", nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
	);

	bool clicked = ImGui::Button("xBot", {size, size});
	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 8.f)) {
		auto pos = ImGui::GetWindowPos();
		auto delta = ImGui::GetIO().MouseDelta;
		ImGui::SetWindowPos({pos.x + delta.x, pos.y + delta.y});
		dragged = true;
	}
	if (clicked && !dragged) bot.menuOpen = !bot.menuOpen;
	if (!ImGui::IsItemActive()) dragged = false;

	ImGui::End();
	ImGui::PopStyleVar();
#endif
}

void menu() {
	auto& bot = Bot::get();
	if (!bot.menuOpen) return;

	static char nameBuffer[96] = {};
	static bool ready = false;
	static bool dirty = true;
	static std::vector<std::string> files;
	static std::string status;

	if (!ready) {
		std::strncpy(nameBuffer, bot.name.c_str(), sizeof(nameBuffer) - 1);
		ready = true;
	}
	if (dirty) {
		files = bot.list();
		dirty = false;
	}

	ImGui::SetNextWindowPos({100.f, 100.f}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize({340.f * scale(), 0.f}, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("xBot", &bot.menuOpen, ImGuiWindowFlags_NoCollapse)) {
		ImGui::End();
		return;
	}

	static char const* labels[] = {"Idle", "Record", "Replay"};
	for (int i = 0; i < 3; i++) {
		if (i > 0) ImGui::SameLine();
		bool active = static_cast<int>(bot.mode) == i;
		if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		if (ImGui::Button(labels[i])) bot.setMode(static_cast<Mode>(i));
		if (active) ImGui::PopStyleColor();
	}

	ImGui::Separator();

	float speed = bot.speed;
	if (ImGui::InputFloat("Speed", &speed, 0.1f, 1.f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue)) {
		bot.setSpeed(speed);
	}

	int tps = static_cast<int>(bot.tps);
	if (ImGui::InputInt("TPS", &tps, 1, 60, ImGuiInputTextFlags_EnterReturnsTrue)) {
		bot.setTps(tps);
	}

	if (ImGui::Checkbox("Frame Stepper", &bot.frozen)) bot.applyAudio();
	if (bot.frozen) {
		ImGui::SameLine();
		ImGui::PushButtonRepeat(true);
		if (ImGui::Button("Step")) bot.pendingSteps++;
		ImGui::PopButtonRepeat();
	}

	if (auto layer = PlayLayer::get()) {
		ImGui::Text("Frame: %d | Inputs: %zu", layer->m_gameState.m_currentProgress, bot.macro.inputs.size());
	}
	else {
		ImGui::Text("Inputs: %zu", bot.macro.inputs.size());
	}

	ImGui::Separator();

	ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer));
	if (ImGui::Button("Save")) {
		bot.name = nameBuffer;
		status = bot.save(nameBuffer) ? "Saved" : "Save failed";
		dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Load")) {
		bot.name = nameBuffer;
		status = bot.load(nameBuffer) ? "Loaded" : "Load failed";
	}
	ImGui::SameLine();
	if (ImGui::Button("Refresh")) dirty = true;

	if (ImGui::BeginListBox("##macros", {-FLT_MIN, 6.f * ImGui::GetTextLineHeightWithSpacing()})) {
		for (auto const& file : files) {
			if (ImGui::Selectable(file.c_str(), file == nameBuffer)) {
				std::strncpy(nameBuffer, file.c_str(), sizeof(nameBuffer) - 1);
			}
		}
		ImGui::EndListBox();
	}

	if (!status.empty()) ImGui::TextUnformatted(status.c_str());

	ImGui::End();
}

}

$on_mod(Loaded) {
	Bot::get().restore();
	ImGuiCocos::get().setup(applyStyle).draw([] {
		shortcuts();
		floatingButton();
		menu();
	});
}
