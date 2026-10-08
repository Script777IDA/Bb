#include "mod_menu.hpp"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <imgui.h>

namespace mod_menu {
namespace {

const ImVec4 kBlue{0.082f, 0.349f, 0.902f, 1.0f};
const ImVec4 kRowDark{0.098f, 0.098f, 0.098f, 1.0f};
const ImVec4 kPanel{0.020f, 0.020f, 0.020f, 1.0f};
const ImVec4 kText{1.0f, 1.0f, 1.0f, 1.0f};
constexpr float kWidth = 1080.0f;
constexpr float kHeight = 960.0f;
constexpr float kSidebar = 110.0f;

struct Row {
    enum class Type { Toggle, Slider, Bind } type{Type::Toggle};
    std::string label;
    bool on{false};
    bool has_bind{false};
    bool has_gear{false};
    int key{0};
    float value{0.0f};
    float min{0.0f};
    float max{1.0f};
    float step{1.0f};
};

struct Tab {
    const char* id;
    const char* name;
    int icon;
    bool bottom{false};
    std::vector<Row> rows;
};

struct State {
    std::vector<Tab> tabs;
    std::size_t active{2};
    bool initialized{false};
    bool waiting_for_key{false};
    Row* bind_target{nullptr};
    float menu_opacity{1.0f};
    float blur{0.0f};
    bool show_fps{true};
    bool watermark{true};
    bool sound{true};
    int menu_key{VK_F1};
    std::array<bool, 256> previous_keys{};
};

State s;

Row T(const char* label, bool on = false, bool bind = false, bool gear = false) {
    Row r;
    r.type = Row::Type::Toggle;
    r.label = label;
    r.on = on;
    r.has_bind = bind;
    r.has_gear = gear;
    return r;
}

Row S(const char* label, float value, float min, float max, float step = 1.0f) {
    Row r;
    r.type = Row::Type::Slider;
    r.label = label;
    r.value = value;
    r.min = min;
    r.max = max;
    r.step = step;
    return r;
}

Row B(const char* label, int key = 0) {
    Row r;
    r.type = Row::Type::Bind;
    r.label = label;
    r.key = key;
    return r;
}

void add_tabs() {
    s.tabs = {
        {"combat", "Combat", 0, false, {
            S("AIM FOV", 90, 10, 180), T("AIM ASSIST", false, true),
            T("TRIGGER BOT"), T("NO RECOIL", true), T("NO SPREAD", true),
            T("RAPID FIRE"), B("QUICK SHOT"), B("MELEE BOOST")}},
        {"visual", "Visual", 1, false, {
            S("VIEW DISTANCE", 100, 0, 500, 5), T("NIGHT MODE", false, true),
            T("FULL BRIGHT"), T("NO FOG", true), T("XRAY", false, true), T("SHOW BOUNDS")}},
        {"actor", "Actor", 2, false, {
            S("HEALTH", 200, 0, 200), S("ARMOR", 200, 0, 200),
            T("GODMODE", false, true), T("SUPER JUMP", true, true),
            T("REGENERATE HEALTH", true, true, true), T("REGENERATE ARMOR", true, true, true),
            T("UNLIMITED STAMINA", true), T("NO RAGDOLL", true), T("NO FALL"), T("SEATBELT"),
            B("CLEAR TASKS"), B("SUICIDE")}},
        {"vehicle", "Vehicle", 3, false, {
            S("ENGINE POWER", 100, 0, 200), T("VEHICLE GODMODE", true, true),
            T("DRIFT MODE"), T("RAINBOW PAINT"), T("LIGHTS ALWAYS ON"), T("NO GRAVITY"),
            B("FIX VEHICLE"), B("BOOST")}},
        {"teleport", "Teleport", 4, false, {
            S("TP OFFSET", 0, 0, 100), T("AUTO TP WAYPOINT"),
            B("TP TO WAYPOINT"), B("TP TO OBJECTIVE"), B("SAVE POSITION"), B("TP TO SAVED")}},
        {"weapon", "Weapon", 5, false, {
            S("AMMO COUNT", 250, 0, 9999, 10), T("INFINITE AMMO", true, true, true),
            T("EXPLOSIVE AMMO"), T("FIRE AMMO"), T("ONE HIT KILL"), T("NO RELOAD", true),
            B("GIVE ALL WEAPONS"), B("REMOVE WEAPONS")}},
        {"online", "Online", 6, false, {
            S("DRAW RANGE", 300, 0, 1000, 10), T("SHOW PLAYERS", true), T("SHOW IDS", true),
            T("SPECTATE", false, true), B("TP TO PLAYER"), B("SEND DM")}},
        {"settings", "Settings", 7, true, {
            S("MENU OPACITY", 100, 0, 100), S("BLUR", 0, 0, 100),
            T("SHOW FPS", true), T("WATERMARK", true), T("SOUND", true), B("MENU KEY", VK_F1)}}
    };
}

ImU32 rgba(ImVec4 v) {
    return ImGui::ColorConvertFloat4ToU32(v);
}

ImU32 with_alpha(ImVec4 v, float a) {
    v.w *= a;
    return rgba(v);
}

float clamp_value(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

const char* key_name(int key) {
    if (key == 0) return "BIND";
    if (key == VK_ESCAPE) return "ESC";
    if (key == VK_INSERT) return "INSERT";
    if (key == VK_DELETE) return "DEL";
    if (key == VK_RETURN) return "ENTER";
    if (key == VK_SPACE) return "SPACE";
    if (key == VK_TAB) return "TAB";
    if (key >= VK_F1 && key <= VK_F24) {
        static char buf[8];
        std::snprintf(buf, sizeof(buf), "F%d", key - VK_F1 + 1);
        return buf;
    }
    static char buf[8];
    if (key >= 'A' && key <= 'Z') {
        buf[0] = static_cast<char>(key);
        buf[1] = '\0';
        return buf;
    }
    if (key >= '0' && key <= '9') {
        buf[0] = static_cast<char>(key);
        buf[1] = '\0';
        return buf;
    }
    std::snprintf(buf, sizeof(buf), "0x%02X", key);
    return buf;
}

bool pressed_edge(int vk) {
    if (vk < 0 || vk >= static_cast<int>(s.previous_keys.size())) return false;
    const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    const bool edge = down && !s.previous_keys[static_cast<std::size_t>(vk)];
    s.previous_keys[static_cast<std::size_t>(vk)] = down;
    return edge;
}

void draw_logo(ImDrawList* dl, ImVec2 base) {
    const ImU32 c = IM_COL32(255, 255, 255, 255);
    const ImVec2 a{base.x + 6, base.y + 6};
    dl->AddTriangleFilled({a.x - 4, a.y - 2}, {a.x, a.y + 8}, {a.x + 4, a.y - 2}, c);
    dl->AddLine({a.x - 12, a.y - 1}, {a.x - 5, a.y + 4}, c, 2.0f);
    dl->AddLine({a.x + 5, a.y + 4}, {a.x + 12, a.y - 1}, c, 2.0f);
}

void draw_icon(ImDrawList* dl, ImVec2 c, int icon, ImU32 col, float scale = 1.0f) {
    const float r = 9.0f * scale;
    switch (icon) {
    case 0: // crosshair
        dl->AddCircle(c, r * .72f, col, 24, 1.6f);
        dl->AddLine({c.x, c.y-r}, {c.x, c.y-r*.45f}, col, 1.6f);
        dl->AddLine({c.x, c.y+r*.45f}, {c.x, c.y+r}, col, 1.6f);
        dl->AddLine({c.x-r, c.y}, {c.x-r*.45f, c.y}, col, 1.6f);
        dl->AddLine({c.x+r*.45f, c.y}, {c.x+r, c.y}, col, 1.6f);
        dl->AddCircleFilled(c, 1.5f, col);
        break;
    case 1: // eye
        dl->PathClear();
        dl->PathLineTo({c.x-r, c.y});
        dl->PathBezierCubicTo({c.x-r*.55f,c.y-r*.62f},{c.x+r*.55f,c.y-r*.62f},{c.x+r,c.y});
        dl->PathBezierCubicTo({c.x+r*.55f,c.y+r*.62f},{c.x-r*.55f,c.y+r*.62f},{c.x-r,c.y});
        dl->PathStroke(col, true, 1.7f);
        dl->AddCircle(c, r*.28f, col, 16, 1.7f);
        break;
    case 2: // actor
        dl->AddCircle({c.x, c.y-r*.55f}, r*.23f, col, 16, 1.7f);
        dl->AddLine({c.x, c.y-r*.25f}, {c.x, c.y+r*.2f}, col, 1.7f);
        dl->AddLine({c.x-r*.44f, c.y-r*.02f}, {c.x+r*.44f, c.y-r*.02f}, col, 1.7f);
        dl->AddLine({c.x, c.y+r*.2f}, {c.x-r*.34f, c.y+r*.8f}, col, 1.7f);
        dl->AddLine({c.x, c.y+r*.2f}, {c.x+r*.34f, c.y+r*.8f}, col, 1.7f);
        break;
    case 3: // car
        dl->AddRect({c.x-r*.83f,c.y-r*.02f},{c.x+r*.83f,c.y+r*.52f},col,2,0,1.7f);
        dl->AddLine({c.x-r*.7f,c.y-r*.02f},{c.x-r*.52f,c.y-r*.65f},col,1.7f);
        dl->AddLine({c.x-r*.52f,c.y-r*.65f},{c.x+r*.52f,c.y-r*.65f},col,1.7f);
        dl->AddLine({c.x+r*.52f,c.y-r*.65f},{c.x+r*.7f,c.y-r*.02f},col,1.7f);
        dl->AddCircle({c.x-r*.52f,c.y+r*.55f},r*.16f,col,16,1.7f);
        dl->AddCircle({c.x+r*.52f,c.y+r*.55f},r*.16f,col,16,1.7f);
        break;
    case 4: // mountain
        dl->AddLine({c.x-r,c.y+r*.72f},{c.x-r*.35f,c.y-r*.7f},col,1.7f);
        dl->AddLine({c.x-r*.35f,c.y-r*.7f},{c.x+r*.06f,c.y+r*.25f},col,1.7f);
        dl->AddLine({c.x+r*.06f,c.y+r*.25f},{c.x+r*.4f,c.y-r*.14f},col,1.7f);
        dl->AddLine({c.x+r*.4f,c.y-r*.14f},{c.x+r,c.y+r*.72f},col,1.7f);
        dl->AddLine({c.x-r,c.y+r*.72f},{c.x+r,c.y+r*.72f},col,1.7f);
        break;
    case 5: // weapon
        dl->AddRect({c.x-r*.75f,c.y-r*.4f},{c.x+r*.78f,c.y+r*.02f},col,1.5f,0,1.7f);
        dl->AddLine({c.x-r*.08f,c.y+r*.02f},{c.x-r*.3f,c.y+r*.8f},col,1.7f);
        dl->AddLine({c.x-r*.74f,c.y+r*.02f},{c.x-r*.88f,c.y+r*.78f},col,1.7f);
        break;
    case 6: // list
        for (int i = -1; i <= 1; ++i) {
            dl->AddLine({c.x-r*.3f,c.y+i*r*.55f},{c.x+r*.9f,c.y+i*r*.55f},col,1.7f);
            dl->AddCircleFilled({c.x-r*.78f,c.y+i*r*.55f},1.5f,col);
        }
        break;
    case 7: // gear
        dl->AddCircle(c,r*.34f,col,20,1.7f);
        dl->AddCircle(c,r*.77f,col,20,1.7f);
        for (int i=0;i<8;i++) {
            const float a = i * 3.14159265f / 4.0f;
            const ImVec2 p1{c.x+std::cos(a)*r*.78f,c.y+std::sin(a)*r*.78f};
            const ImVec2 p2{c.x+std::cos(a)*r*.98f,c.y+std::sin(a)*r*.98f};
            dl->AddLine(p1,p2,col,2.0f);
        }
        break;
    }
}

void draw_deco(ImDrawList* dl, ImVec2 p, float width, bool upper, ImU32 col) {
    const float y = upper ? p.y + 16.0f : p.y + 10.0f;
    dl->AddLine({p.x, y}, {p.x + width, y}, col, 1.0f);
    const float marks[] = {0.18f,0.39f,0.70f,0.86f};
    for (float m : marks) {
        const float x = p.x + width * m;
        dl->AddLine({x, y}, {x+12.0f, y + (upper ? -7.0f : 7.0f)}, col, 1.0f);
        dl->AddLine({x+12.0f, y + (upper ? -7.0f : 7.0f)}, {x+56.0f, y + (upper ? -7.0f : 7.0f)}, col, 1.0f);
        dl->AddLine({x+56.0f, y + (upper ? -7.0f : 7.0f)}, {x+68.0f, y}, col, 1.0f);
    }
}

bool gear_button(ImDrawList* dl, ImVec2 min, ImVec2 max, const char* id) {
    ImGui::SetCursorScreenPos(min);
    ImGui::InvisibleButton(id, {max.x-min.x, max.y-min.y});
    const bool clicked = ImGui::IsItemClicked();
    const bool hover = ImGui::IsItemHovered();
    const ImU32 bg = hover ? IM_COL32(80,80,80,120) : IM_COL32(255,255,255,30);
    dl->AddRectFilled(min,max,bg,2.0f);
    draw_icon(dl, {(min.x+max.x)*.5f,(min.y+max.y)*.5f}, 7, IM_COL32(255,255,255,255), .65f);
    return clicked;
}

bool bind_button(const char* id, Row& r, ImVec2 min, ImVec2 max) {
    ImGui::SetCursorScreenPos(min);
    ImGui::InvisibleButton(id, {max.x-min.x, max.y-min.y});
    if (ImGui::IsItemClicked()) {
        s.waiting_for_key = true;
        s.bind_target = &r;
    }
    const bool listening = s.waiting_for_key && s.bind_target == &r;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 bg = listening ? IM_COL32(255,255,255,255) : IM_COL32(61,61,61,255);
    const ImU32 fg = listening ? IM_COL32(0,0,0,255) : IM_COL32(255,255,255,255);
    dl->AddRectFilled(min,max,bg,2.0f);
    const char* text = listening ? "..." : key_name(r.key);
    const ImVec2 sz = ImGui::CalcTextSize(text);
    dl->AddText({(min.x+max.x-sz.x)*.5f,(min.y+max.y-sz.y)*.5f},fg,text);
    return ImGui::IsItemHovered();
}

void finish_key_listening() {
    if (!s.waiting_for_key || !s.bind_target) return;
    if (pressed_edge(VK_ESCAPE)) {
        s.waiting_for_key = false;
        s.bind_target = nullptr;
        return;
    }
    for (int key = 8; key < 256; ++key) {
        if (pressed_edge(key)) {
            s.bind_target->key = key;
            if (s.bind_target->label == "MENU KEY") s.menu_key = key;
            s.waiting_for_key = false;
            s.bind_target = nullptr;
            return;
        }
    }
}

void draw_row(const Row& r, int row_index, ImVec2 origin, float width, float alpha) {
    Row& row = const_cast<Row&>(r);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = row.type == Row::Type::Slider ? 96.0f : 64.0f;
    const ImVec2 min{origin.x, origin.y};
    const ImVec2 max{origin.x + width, origin.y + h};
    const bool on = row.type == Row::Type::Toggle && row.on;
    const ImU32 bg = with_alpha(on ? kBlue : kRowDark, alpha);
    dl->AddRectFilled(min,max,bg,0.0f);

    const ImU32 fg = with_alpha(kText, alpha);
    if (row.type == Row::Type::Slider) {
        const char* text = row.label.c_str();
        dl->AddText({min.x+16, min.y+21}, fg, text);
        char valbuf[32];
        std::snprintf(valbuf, sizeof(valbuf), "%.2f", row.value);
        const ImVec2 vsize = ImGui::CalcTextSize(valbuf);
        dl->AddText({max.x-16-vsize.x, min.y+21}, fg, valbuf);

        ImVec2 trackMin{min.x+43, min.y+67};
        ImVec2 trackMax{max.x-43, min.y+70};
        const float p = (row.max > row.min) ? clamp_value((row.value-row.min)/(row.max-row.min),0.0f,1.0f) : 0.0f;
        dl->AddRectFilled(trackMin,trackMax,with_alpha(ImVec4{.227f,.227f,.227f,1},alpha),2.0f);
        dl->AddRectFilled(trackMin,{trackMin.x+(trackMax.x-trackMin.x)*p,trackMax.y},with_alpha(kText,alpha),2.0f);
        ImVec2 knob{trackMin.x+(trackMax.x-trackMin.x)*p, min.y+68.5f};
        dl->AddRectFilled({knob.x-4,knob.y-8},{knob.x+4,knob.y+8},fg,2.0f);

        const float bw = 18.0f;
        ImGui::SetCursorScreenPos({min.x+16,min.y+59});
        ImGui::InvisibleButton((std::string("minus##")+std::to_string(row_index)).c_str(),{bw,bw});
        if (ImGui::IsItemClicked()) row.value = clamp_value(row.value-row.step,row.min,row.max);
        dl->AddRectFilled({min.x+16,min.y+59},{min.x+34,min.y+77},IM_COL32(46,46,46,255),2.0f);
        dl->AddText({min.x+21,min.y+60},fg,"-");

        ImGui::SetCursorScreenPos({max.x-34,min.y+59});
        ImGui::InvisibleButton((std::string("plus##")+std::to_string(row_index)).c_str(),{bw,bw});
        if (ImGui::IsItemClicked()) row.value = clamp_value(row.value+row.step,row.min,row.max);
        dl->AddRectFilled({max.x-34,min.y+59},{max.x-16,min.y+77},IM_COL32(46,46,46,255),2.0f);
        dl->AddText({max.x-29,min.y+60},fg,"+");

        ImGui::SetCursorScreenPos(trackMin);
        ImGui::InvisibleButton((std::string("range##")+std::to_string(row_index)).c_str(),{trackMax.x-trackMin.x,24});
        if (ImGui::IsItemActive()) {
            const float mouse = ImGui::GetIO().MousePos.x;
            const float t = clamp_value((mouse-trackMin.x)/(trackMax.x-trackMin.x),0.0f,1.0f);
            row.value = row.min + t*(row.max-row.min);
            if (row.step > 0.0f) row.value = row.min + std::round((row.value-row.min)/row.step)*row.step;
        }
        return;
    }

    dl->AddText({min.x+16, min.y+(h-16)*0.5f}, fg, row.label.c_str());
    const float right = max.x - 16;

    if (row.type == Row::Type::Bind) {
        bind_button((std::string("bind##")+std::to_string(row_index)).c_str(), row,
                    {right-74,min.y+20},{right,min.y+44});
        return;
    }

    float cursor = right;
    if (row.has_bind) {
        bind_button((std::string("tbind##")+std::to_string(row_index)).c_str(), row,
                    {cursor-74,min.y+20},{cursor,min.y+44});
        cursor -= 82;
    }
    if (row.has_gear) {
        gear_button(dl,{cursor-22,min.y+21},{cursor,min.y+43},(std::string("gear##")+std::to_string(row_index)).c_str());
        cursor -= 30;
    }

    const ImVec2 tgMin{cursor-46,min.y+20};
    const ImVec2 tgMax{cursor,min.y+44};
    ImGui::SetCursorScreenPos(tgMin);
    ImGui::InvisibleButton((std::string("toggle##")+std::to_string(row_index)).c_str(),{46,24});
    if (ImGui::IsItemClicked()) row.on = !row.on;
    dl->AddRectFilled(tgMin,tgMax,row.on ? IM_COL32(255,255,255,255) : IM_COL32(69,69,69,255),3.0f);
    const float knobX = row.on ? tgMax.x-12.0f : tgMin.x+12.0f;
    dl->AddRectFilled({knobX-9,tgMin.y+3},{knobX+9,tgMin.y+21},row.on ? rgba(kBlue) : IM_COL32(185,185,185,255),2.0f);
}

void settings_sync() {
    if (s.tabs.size() != 8) return;
    s.menu_opacity = s.tabs[7].rows[0].value / 100.0f;
    s.blur = s.tabs[7].rows[1].value;
    s.show_fps = s.tabs[7].rows[2].on;
    s.watermark = s.tabs[7].rows[3].on;
    s.sound = s.tabs[7].rows[4].on;
    if (s.tabs[7].rows[5].key) s.menu_key = s.tabs[7].rows[5].key;
}

void render_tabs(ImDrawList* dl, ImVec2 menuPos, float alpha) {
    for (std::size_t i=0;i<s.tabs.size();++i) {
        if (s.tabs[i].bottom) continue;
        const ImVec2 min{menuPos.x+34, menuPos.y+78+static_cast<float>(i)*57.0f};
        const ImVec2 max{min.x+42, min.y+42};
        ImGui::SetCursorScreenPos(min);
        ImGui::InvisibleButton((std::string("tab##")+std::to_string(i)).c_str(),{42,42});
        if (ImGui::IsItemClicked()) s.active=i;
        const bool active = s.active==i;
        dl->AddRectFilled(min,max,active?with_alpha(kBlue,alpha):with_alpha(ImVec4{0,0,0,0},0),4.0f);
        if (ImGui::IsItemHovered() && !active) dl->AddRectFilled(min,max,with_alpha(ImVec4{.114f,.114f,.114f,1},alpha),4.0f);
        draw_icon(dl,{(min.x+max.x)*.5f,(min.y+max.y)*.5f},s.tabs[i].icon,with_alpha(kText,alpha),0.95f);
    }

    const std::size_t idx = 7;
    const ImVec2 min{menuPos.x+34, menuPos.y+kHeight-68};
    const ImVec2 max{min.x+42, min.y+42};
    ImGui::SetCursorScreenPos(min);
    ImGui::InvisibleButton("settings-tab##",{42,42});
    if (ImGui::IsItemClicked()) s.active=idx;
    const bool active = s.active==idx;
    dl->AddRectFilled(min,max,active?with_alpha(kBlue,alpha):with_alpha(ImVec4{0,0,0,0},0),4.0f);
    if (ImGui::IsItemHovered() && !active) dl->AddRectFilled(min,max,with_alpha(ImVec4{.114f,.114f,.114f,1},alpha),4.0f);
    draw_icon(dl,{(min.x+max.x)*.5f,(min.y+max.y)*.5f},7,with_alpha(kText,alpha),0.95f);
}

} // namespace

void initialize() {
    if (s.initialized) return;
    s.initialized = true;
    add_tabs();
}

void render(bool& menu_open) {
    initialize();
    settings_sync();

    if (s.menu_key > 0 && pressed_edge(s.menu_key)) menu_open = !menu_open;
    finish_key_listening();
    if (!menu_open) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& st = ImGui::GetStyle();

    const float alpha = clamp_value(s.menu_opacity, 0.0f, 1.0f);
    const float fit_x = (io.DisplaySize.x - 24.0f) / kWidth;
    const float fit_y = (io.DisplaySize.y - 24.0f) / kHeight;
    const float scale = std::max(0.55f, std::min(1.0f, std::min({fit_x, fit_y, io.DisplayFramebufferScale.x})));
    const ImVec2 size{kWidth*scale, kHeight*scale};
    const ImVec2 pos{std::max(12.0f, io.DisplaySize.x-size.x-12.0f), 12.0f};

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("##mod_menu", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings);

    st.WindowPadding = {0,0};
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const float W = ImGui::GetWindowWidth();
    const float H = ImGui::GetWindowHeight();
    const float sidebar = kSidebar*scale;

    ImVec2 poly[8] = {
        {wp.x+16,wp.y},{wp.x+W-16,wp.y},{wp.x+W,wp.y+16},{wp.x+W,wp.y+H-16},
        {wp.x+W-16,wp.y+H},{wp.x+16,wp.y+H},{wp.x,wp.y+H-16},{wp.x,wp.y+16}
    };
    dl->AddConvexPolyFilled(poly,8,with_alpha(kPanel,alpha));
    dl->AddRectFilled({wp.x,wp.y+16},{wp.x+sidebar,wp.y+H-16},with_alpha(ImVec4{.020f,.020f,.020f,1},alpha));
    dl->AddLine({wp.x+sidebar,wp.y+18},{wp.x+sidebar,wp.y+H-18},with_alpha(ImVec4{.07f,.07f,.07f,1},alpha),1.0f);

    draw_logo(dl,{wp.x+sidebar*.5f-6,wp.y+22});
    render_tabs(dl,wp,alpha);

    const float bodyX = wp.x+sidebar+8.0f*scale;
    const float bodyY = wp.y+26.0f*scale;
    const float bodyW = W-sidebar-38.0f*scale;
    draw_deco(dl,{bodyX,bodyY},bodyW,true,with_alpha(ImVec4{.141f,.141f,.141f,1},alpha));

    const Tab& tab = s.tabs[s.active];
    const char* title = tab.name;
    const ImVec2 titleSize = ImGui::CalcTextSize(title);
    const ImVec2 titlePos{bodyX+bodyW*.5f-titleSize.x*.5f-13, bodyY+8};
    dl->AddRectFilled({titlePos.x,titlePos.y},{titlePos.x+titleSize.x+26,titlePos.y+22},with_alpha(ImVec4{.082f,.082f,.082f,1},alpha));
    dl->AddRect({titlePos.x,titlePos.y},{titlePos.x+titleSize.x+26,titlePos.y+22},with_alpha(ImVec4{.137f,.137f,.137f,1},alpha),0.0f,0,1.0f);
    dl->AddText({titlePos.x+13,titlePos.y+4},with_alpha(kText,alpha),title);

    const float gridTop = bodyY+48.0f;
    const float colGap = 20.0f*scale;
    const float rowGap = 18.0f*scale;
    const float colW = (bodyW-colGap)/2.0f;

    float y[2] = {gridTop,gridTop};
    for (std::size_t i=0;i<tab.rows.size();++i) {
        int col = static_cast<int>(i%2);
        draw_row(tab.rows[i],static_cast<int>(i),{bodyX+col*(colW+colGap),y[col]},colW,alpha);
        y[col] += (tab.rows[i].type == Row::Type::Slider ? 96.0f : 64.0f) + rowGap;
    }

    draw_deco(dl,{bodyX,wp.y+H-36.0f},bodyW,false,with_alpha(ImVec4{.141f,.141f,.141f,1},alpha));

    if (s.watermark) {
        dl->AddText({bodyX,wp.y+H-58},with_alpha(ImVec4{.65f,.65f,.65f,1},alpha),"Mod Menu — Actor");
    }
    if (s.show_fps) {
        char fps[32];
        std::snprintf(fps,sizeof(fps),"%.0f FPS",io.Framerate);
        const ImVec2 fs=ImGui::CalcTextSize(fps);
        dl->AddText({wp.x+W-20-fs.x,wp.y+H-58},with_alpha(kText,alpha),fps);
    }

    ImGui::End();
}

int menu_key() {
    initialize();
    return s.menu_key;
}

} // namespace mod_menu
