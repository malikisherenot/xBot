# Compatibility patch for matcool/gd-imgui-cocos against the Geode Cocos
# callback signatures present in the build logs. This modifies only CPM's
# fetched dependency source in the build tree, not the upstream repository.

if (NOT DEFINED gd-imgui-cocos_SOURCE_DIR OR NOT EXISTS "${gd-imgui-cocos_SOURCE_DIR}/include/imgui-cocos.hpp")
    message(FATAL_ERROR "Could not locate the fetched gd-imgui-cocos source directory")
endif()

set(_imgui_header "${gd-imgui-cocos_SOURCE_DIR}/include/imgui-cocos.hpp")
set(_imgui_source "${gd-imgui-cocos_SOURCE_DIR}/src/main.cpp")

file(READ "${_imgui_header}" _header)
file(READ "${_imgui_source}" _source)

# CCIMEDelegate::insertText now receives the key code as a third argument.
string(REPLACE
    "void insertText(const char* text, int len) override;"
    "void insertText(const char* text, int len, cocos2d::enumKeyCodes) override;"
    _header "${_header}")
string(REPLACE
    "void ImGuiNode::insertText(const char* text, int len) {"
    "void ImGuiNode::insertText(const char* text, int len, cocos2d::enumKeyCodes) {"
    _source "${_source}")

# Geode's keyboard callbacks include a timestamp argument.
string(REPLACE
    "void keyDown(cocos2d::enumKeyCodes key) override;"
    "void keyDown(cocos2d::enumKeyCodes key, double timestamp) override;"
    _header "${_header}")
string(REPLACE
    "void ImGuiNode::keyDown(cocos2d::enumKeyCodes key) {"
    "void ImGuiNode::keyDown(cocos2d::enumKeyCodes key, double) {"
    _source "${_source}")
string(REPLACE
    "void keyUp(cocos2d::enumKeyCodes key) override;"
    "void keyUp(cocos2d::enumKeyCodes key, double timestamp) override;"
    _header "${_header}")
string(REPLACE
    "void ImGuiNode::keyUp(cocos2d::enumKeyCodes key) {"
    "void ImGuiNode::keyUp(cocos2d::enumKeyCodes key, double) {"
    _source "${_source}")

file(WRITE "${_imgui_header}" "${_header}")
file(WRITE "${_imgui_source}" "${_source}")
