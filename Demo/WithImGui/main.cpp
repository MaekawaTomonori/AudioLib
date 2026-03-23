#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"

#include "AudioLib.hpp"

#undef min
#undef max

using Microsoft::WRL::ComPtr;

// ================================================================
//  DAW visual constants
// ================================================================
namespace DawStyle {

    static const ImVec4 kChannelColors[] = {
        { 0.18f, 0.52f, 0.62f, 1.f },  // teal
        { 0.72f, 0.40f, 0.12f, 1.f },  // orange
        { 0.52f, 0.28f, 0.68f, 1.f },  // purple
        { 0.22f, 0.58f, 0.28f, 1.f },  // green
        { 0.62f, 0.22f, 0.22f, 1.f },  // red
        { 0.28f, 0.38f, 0.68f, 1.f },  // blue
    };
    static constexpr int kColorCount = 6;

    static constexpr float kChannelWidth  = 90.f;
    static constexpr float kMasterWidth   = 104.f;
    static constexpr float kFaderHeight   = 130.f;
    static constexpr float kVUWidth       = 10.f;
    static constexpr float kVUHeight      = 90.f;
    static constexpr float kHeaderHeight  = 26.f;
    static constexpr float kAddTrackWidth = 44.f;

    static const ImVec4 kTextDim      = { 0.50f, 0.50f, 0.50f, 1.f };
    static const ImVec4 kMuteActive   = { 0.85f, 0.25f, 0.25f, 1.f };
    static const ImVec4 kLoopActive   = { 0.25f, 0.55f, 0.85f, 1.f };
    static const ImVec4 kPlayActive   = { 0.22f, 0.72f, 0.38f, 1.f };
    static const ImVec4 kPauseActive  = { 0.72f, 0.60f, 0.10f, 1.f };
    static const ImVec4 kStopAll      = { 0.55f, 0.25f, 0.08f, 1.f };
    static const ImVec4 kStopAllHover = { 0.72f, 0.33f, 0.10f, 1.f };

} // namespace DawStyle

// ================================================================
//  VU meter helpers
// ================================================================
static void DrawVUBar(ImDrawList* dl, float x, float y, float w, float h, float level) {
    dl->AddRectFilled({ x, y }, { x + w, y + h }, IM_COL32(20, 20, 20, 255), 2.f);
    if (level <= 0.001f) return;
    float fillH   = h * std::min(level, 1.0f);
    float greenH  = h * 0.75f;
    float yellowH = h * 0.15f;
    float greenTop  = y + h - greenH;
    float yellowTop = greenTop - yellowH;
    if (fillH <= greenH) {
        dl->AddRectFilled({ x, y + h - fillH }, { x + w, y + h }, IM_COL32(40, 200, 60, 255), 2.f);
    } else if (fillH <= greenH + yellowH) {
        float yf = fillH - greenH;
        dl->AddRectFilled({ x, greenTop },      { x + w, y + h },     IM_COL32(40, 200, 60,  255), 2.f);
        dl->AddRectFilled({ x, greenTop - yf }, { x + w, greenTop },  IM_COL32(220, 200, 30, 255), 2.f);
    } else {
        float rf = fillH - greenH - yellowH;
        dl->AddRectFilled({ x, greenTop },       { x + w, y + h },    IM_COL32(40, 200, 60,  255), 2.f);
        dl->AddRectFilled({ x, yellowTop },      { x + w, greenTop }, IM_COL32(220, 200, 30, 255), 2.f);
        dl->AddRectFilled({ x, yellowTop - rf }, { x + w, yellowTop}, IM_COL32(220, 60,  40, 255), 2.f);
    }
}

static float SimulateVU(int seed, float time) {
    float f1 = fabsf(sinf(time *  8.5f + seed * 2.1f));
    float f2 = fabsf(sinf(time * 17.3f + seed * 0.9f));
    float f3 = fabsf(sinf(time * 33.7f + seed * 1.5f));
    return std::clamp(0.55f * f1 + 0.30f * f2 + 0.15f * f3, 0.0f, 1.0f);
}

// ================================================================
//  AudioDebugPanel
// ================================================================
class AudioDebugPanel {
public:
    void Draw();

private:
    // --- Loaded audio asset ---
    struct LoadedSound {
        std::string   path;
        Audio::Handle handle;
        int           colorIdx = 0;
    };

    // --- Single active playback within a Track ---
    struct PlayingItem {
        std::string           soundName;
        Audio::PlaybackHandle playback;
        float volume = 1.0f;
        float pitch  = 1.0f;
        float pan    = 0.0f;
        bool  muted  = false;
        bool  loop   = false;
        bool  paused = false;
    };

    // --- Mixer track (maps to Audio::TrackHandle / ma_sound_group) ---
    struct Track {
        std::string        name;
        Audio::TrackHandle handle;
        float volume   = 1.0f;
        float pan      = 0.0f;
        float pitch    = 1.0f;
        int   colorIdx = 0;
        std::vector<PlayingItem> items;
    };

    // --- Windows ---
    void DrawLoaderWindow();
    void DrawPlayerWindow();
    void DrawMixerWindow();

    // --- Mixer helpers ---
    void DrawMasterStrip(float groupX);
    void DrawTrackStrip(Track& t, int idx, float groupX, bool& remove);
    void DrawPlaybackList(Track& t);
    void DrawDualVU(int seed, float volume, bool active, float stripWidth);

    // --- Actions ---
    void AddTrack();
    void RemoveTrack(int idx);

    // --- State ---
    char  pathBuf_[256]  = "fx.wav";
    float masterVolume_  = 1.0f;
    int   nextColorIdx_  = 0;
    int   selectedSound_ = -1;
    int   targetTrack_   = -1;   // which track to play into (Player)
    int   focusedTrack_  = -1;   // which track's playback list is shown (Mixer right panel)

    float playerVolume_    = 1.0f;
    float playerPitch_     = 1.0f;
    float playerPan_       = 0.0f;
    bool  playerLoop_      = false;
    float listPanelWidth_  = 390.f;  // Mixer right panel width (resizable)

    std::vector<LoadedSound> loadedSounds_;
    std::vector<Track>       tracks_;
};

// ----------------------------------------------------------------
void AudioDebugPanel::Draw() {
    DrawLoaderWindow();
    DrawPlayerWindow();
    DrawMixerWindow();
}

// ----------------------------------------------------------------
void AudioDebugPanel::AddTrack() {
    Track t;
    t.name     = "Track " + std::to_string(tracks_.size() + 1);
    t.handle   = Audio::CreateTrack();
    t.colorIdx = nextColorIdx_;
    nextColorIdx_ = (nextColorIdx_ + 1) % DawStyle::kColorCount;
    tracks_.push_back(std::move(t));
    focusedTrack_ = static_cast<int>(tracks_.size()) - 1;
    if (targetTrack_ < 0) targetTrack_ = 0;
}

// ----------------------------------------------------------------
void AudioDebugPanel::RemoveTrack(int idx) {
    Audio::DestroyTrack(tracks_[idx].handle);
    tracks_.erase(tracks_.begin() + idx);
    if (focusedTrack_ == idx)       focusedTrack_ = -1;
    else if (focusedTrack_ > idx)   --focusedTrack_;
    if (targetTrack_ == idx)        targetTrack_ = tracks_.empty() ? -1 : 0;
    else if (targetTrack_ > idx)    --targetTrack_;
}

// ================================================================
//  Track Loader window
// ================================================================
void AudioDebugPanel::DrawLoaderWindow() {
    ImGui::SetNextWindowSize({ 400.f, 210.f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({  10.f,  10.f  }, ImGuiCond_FirstUseEver);
    ImGui::Begin("Track Loader");

    ImGui::SetNextItemWidth(220.f);
    ImGui::InputText("##path", pathBuf_, sizeof(pathBuf_));
    ImGui::SameLine(0, 6.f);

    ImGui::PushStyleColor(ImGuiCol_Button,        { 0.18f, 0.42f, 0.18f, 1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.24f, 0.55f, 0.24f, 1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.12f, 0.30f, 0.12f, 1.f });
    if (ImGui::Button("Load") && pathBuf_[0] != '\0') {
        loadedSounds_.push_back({ pathBuf_, Audio::Handle(pathBuf_), nextColorIdx_ });
        nextColorIdx_  = (nextColorIdx_ + 1) % DawStyle::kColorCount;
        selectedSound_ = static_cast<int>(loadedSounds_.size()) - 1;
    }
    ImGui::PopStyleColor(3);

    ImGui::Separator();

    ImGui::BeginChild("##SoundList", { 0.f, 0.f }, false);
    int removeIdx = -1;
    for (int i = 0; i < static_cast<int>(loadedSounds_.size()); ++i) {
        ImGui::PushID(i);
        const ImVec4& col = DawStyle::kChannelColors[loadedSounds_[i].colorIdx];
        ImGui::PushStyleColor(ImGuiCol_Header,        col);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, { col.x + 0.1f, col.y + 0.1f, col.z + 0.1f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  { col.x - 0.1f, col.y - 0.1f, col.z - 0.1f, 1.f });
        const bool selected = (selectedSound_ == i);
        if (ImGui::Selectable(loadedSounds_[i].path.c_str(), selected,
            ImGuiSelectableFlags_None, { ImGui::GetContentRegionAvail().x - 30.f, 0.f }))
            selectedSound_ = i;
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 4.f);
        ImGui::PushStyleColor(ImGuiCol_Button,        { 0.35f, 0.14f, 0.14f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.55f, 0.18f, 0.18f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.22f, 0.08f, 0.08f, 1.f });
        if (ImGui::SmallButton(" x ")) removeIdx = i;
        ImGui::PopStyleColor(3);
        ImGui::PopID();
    }
    if (removeIdx >= 0) {
        loadedSounds_.erase(loadedSounds_.begin() + removeIdx);
        if (selectedSound_ == removeIdx)      selectedSound_ = -1;
        else if (selectedSound_ > removeIdx)  --selectedSound_;
    }
    ImGui::EndChild();
    ImGui::End();
}

// ================================================================
//  Player window
//  - Select sound + target Track, adjust params, PLAY
// ================================================================
void AudioDebugPanel::DrawPlayerWindow() {
    ImGui::SetNextWindowSize({ 450.f, 250.f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({ 420.f,  10.f  }, ImGuiCond_FirstUseEver);
    ImGui::Begin("Player");

    const bool hasSelection = (selectedSound_ >= 0 &&
                               selectedSound_ < static_cast<int>(loadedSounds_.size()));
    const bool hasTracks    = !tracks_.empty();

    // --- Selected sound ---
    if (hasSelection) {
        const ImVec4& hdr = DawStyle::kChannelColors[loadedSounds_[selectedSound_].colorIdx];
        ImGui::PushStyleColor(ImGuiCol_Text, hdr);
        ImGui::TextUnformatted(loadedSounds_[selectedSound_].path.c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, DawStyle::kTextDim);
        ImGui::TextUnformatted("-- No sound selected (click a row in Track Loader) --");
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    ImGui::BeginDisabled(!hasSelection);

    // --- Target Track selector ---
    ImGui::SetNextItemWidth(260.f);
    if (!hasTracks) {
        ImGui::BeginDisabled(true);
        ImGui::BeginCombo("Target Track", "-- No tracks (add one in Mixer) --");
        // BeginCombo is always closed when disabled; EndCombo must NOT be called here
        ImGui::EndDisabled();
        targetTrack_ = -1;
    } else {
        if (targetTrack_ < 0 || targetTrack_ >= static_cast<int>(tracks_.size()))
            targetTrack_ = 0;
        const char* preview = tracks_[targetTrack_].name.c_str();
        if (ImGui::BeginCombo("Target Track", preview)) {
            for (int i = 0; i < static_cast<int>(tracks_.size()); ++i) {
                const bool sel = (i == targetTrack_);
                if (ImGui::Selectable(tracks_[i].name.c_str(), sel)) targetTrack_ = i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Spacing();

    // --- Pre-play parameters ---
    ImGui::SetNextItemWidth(260.f);
    ImGui::SliderFloat("Volume##p", &playerVolume_, 0.f, 1.f, "%.2f");
    ImGui::SetNextItemWidth(260.f);
    ImGui::SliderFloat("Pitch##p",  &playerPitch_,  0.1f, 3.f, "%.2fx");
    ImGui::SetNextItemWidth(260.f);
    ImGui::SliderFloat("Pan##p",    &playerPan_,    -1.f, 1.f, "%.2f");

    {
        const bool wasLoop = playerLoop_;
        if (wasLoop) {
            ImGui::PushStyleColor(ImGuiCol_Button,        DawStyle::kLoopActive);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.40f, 0.70f, 1.f, 1.f });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.10f, 0.35f, 0.7f, 1.f });
        }
        if (ImGui::Button(playerLoop_ ? "Loop: ON " : "Loop: OFF"))
            playerLoop_ = !playerLoop_;
        if (wasLoop) ImGui::PopStyleColor(3);
    }

    ImGui::SameLine(0, 12.f);

    // --- PLAY button ---
    const bool canPlay = hasSelection && hasTracks && targetTrack_ >= 0;
    ImGui::BeginDisabled(!canPlay);
    ImGui::PushStyleColor(ImGuiCol_Button,        DawStyle::kPlayActive);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.35f, 0.90f, 0.50f, 1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.12f, 0.50f, 0.22f, 1.f });
    if (ImGui::Button("  >  PLAY  ") && canPlay) {
        auto& sound = loadedSounds_[selectedSound_];
        auto& track = tracks_[targetTrack_];

        PlayingItem item;
        item.soundName = sound.path;
        item.playback  = sound.handle.Play(playerVolume_, playerPitch_, playerPan_, playerLoop_, track.handle);
        item.volume    = playerVolume_;
        item.pitch     = playerPitch_;
        item.pan       = playerPan_;
        item.loop      = playerLoop_;
        track.items.push_back(std::move(item));
        focusedTrack_  = targetTrack_;
    }
    ImGui::PopStyleColor(3);
    ImGui::EndDisabled();

    ImGui::EndDisabled();
    ImGui::End();
}

// ================================================================
//  Mixer window
//  Left : Master + Track strips (horizontal scroll)
//  Right: Playback list for focused track
// ================================================================
void AudioDebugPanel::DrawMixerWindow() {
    ImGui::SetNextWindowSize({ 1260.f, 500.f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({   10.f,  270.f }, ImGuiCond_FirstUseEver);
    ImGui::Begin("Mixer");

    const float kSplitterW      = 6.f;
    const float availWidth      = ImGui::GetContentRegionAvail().x;
    listPanelWidth_ = std::clamp(listPanelWidth_, 160.f, availWidth - 200.f);
    const float stripPanelWidth = availWidth - listPanelWidth_ - kSplitterW;

    // === Left: Track strips ===
    ImGui::BeginChild("##Strips", { stripPanelWidth, 0.f }, false,
        ImGuiWindowFlags_HorizontalScrollbar);

    float gx = ImGui::GetCursorPosX();
    DrawMasterStrip(gx);
    ImGui::SameLine(0, 0.f);

    ImGui::PushStyleColor(ImGuiCol_Separator, { 0.32f, 0.32f, 0.32f, 1.f });
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 8.f);

    int removeIdx = -1;
    for (int i = 0; i < static_cast<int>(tracks_.size()); ++i) {
        bool remove = false;
        DrawTrackStrip(tracks_[i], i, ImGui::GetCursorPosX(), remove);
        if (remove) removeIdx = i;
        ImGui::SameLine(0, 8.f);
    }
    if (removeIdx >= 0) RemoveTrack(removeIdx);

    // Add Track button (fills available height)
    {
        const float btnH = ImGui::GetContentRegionAvail().y;
        ImGui::PushStyleColor(ImGuiCol_Button,        { 0.12f, 0.20f, 0.12f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.18f, 0.32f, 0.18f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.08f, 0.14f, 0.08f, 1.f });
        if (ImGui::Button("+\nT\nr\na\nc\nk", { DawStyle::kAddTrackWidth, btnH }))
            AddTrack();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add Track");
        ImGui::PopStyleColor(3);
    }

    ImGui::EndChild();

    // === Splitter (drag to resize right panel) ===
    ImGui::SameLine(0, 0.f);
    {
        const float splitterH = ImGui::GetContentRegionAvail().y;
        ImGui::InvisibleButton("##splitter", { kSplitterW, splitterH });
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive())
            listPanelWidth_ -= ImGui::GetIO().MouseDelta.x;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        const ImU32 col = ImGui::IsItemHovered() || ImGui::IsItemActive()
            ? IM_COL32(120, 120, 120, 255)
            : IM_COL32(50,  50,  50,  255);
        dl->AddLine({ p.x - kSplitterW * 0.5f, p.y },
                    { p.x - kSplitterW * 0.5f, p.y + splitterH }, col, 1.f);
    }
    ImGui::SameLine(0, 0.f);

    // === Right: Playback list ===
    ImGui::BeginChild("##PlaybackPanel", { listPanelWidth_, 0.f }, false,
        ImGuiWindowFlags_HorizontalScrollbar);
    if (focusedTrack_ >= 0 && focusedTrack_ < static_cast<int>(tracks_.size()))
        DrawPlaybackList(tracks_[focusedTrack_]);
    else {
        ImGui::PushStyleColor(ImGuiCol_Text, DawStyle::kTextDim);
        ImGui::TextUnformatted("Click a track header to view its playbacks");
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    ImGui::End();
}

// ----------------------------------------------------------------
void AudioDebugPanel::DrawDualVU(int seed, float volume, bool active, float stripWidth) {
    float time   = static_cast<float>(ImGui::GetTime());
    float levelL = active ? SimulateVU(seed,     time) * volume : 0.f;
    float levelR = active ? SimulateVU(seed + 7, time) * volume : 0.f;

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    ImVec2      pos  = ImGui::GetCursorScreenPos();
    float       gap  = 3.f;
    float       totW = DawStyle::kVUWidth * 2.f + gap;
    float       offX = (stripWidth - totW) * 0.5f;
    DrawVUBar(dl, pos.x + offX,                              pos.y, DawStyle::kVUWidth, DawStyle::kVUHeight, levelL);
    DrawVUBar(dl, pos.x + offX + DawStyle::kVUWidth + gap,   pos.y, DawStyle::kVUWidth, DawStyle::kVUHeight, levelR);
    ImGui::Dummy({ stripWidth, DawStyle::kVUHeight });
}

// ----------------------------------------------------------------
void AudioDebugPanel::DrawMasterStrip(float groupX) {
    bool anyActive = false;
    for (auto& t : tracks_)
        for (auto& item : t.items)
            if (item.playback.IsPlaying() && !item.paused) { anyActive = true; break; }

    ImGui::BeginGroup();

    // Header
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetCursorScreenPos();
        float w = DawStyle::kMasterWidth, h = DawStyle::kHeaderHeight;
        dl->AddRectFilled(pos, { pos.x + w, pos.y + h }, IM_COL32(70, 70, 70, 255), 4.f, ImDrawFlags_RoundCornersTop);
        const char* lbl = "MASTER";
        ImVec2 tsz = ImGui::CalcTextSize(lbl);
        dl->AddText({ pos.x + (w - tsz.x) * 0.5f, pos.y + (h - tsz.y) * 0.5f }, IM_COL32(230, 230, 230, 255), lbl);
        ImGui::Dummy({ w, h });
    }

    // LED
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetCursorScreenPos();
        float r = 5.f;
        ImVec2 ctr = { pos.x + DawStyle::kMasterWidth * 0.5f, pos.y + r + 3.f };
        ImU32 col = anyActive ? IM_COL32(50, 255, 80, 255) : IM_COL32(28, 60, 28, 255);
        dl->AddCircleFilled(ctr, r, col);
        dl->AddCircle(ctr, r, IM_COL32(0, 0, 0, 160), 16, 1.2f);
        ImGui::Dummy({ DawStyle::kMasterWidth, r * 2.f + 6.f });
    }

    DrawDualVU(99, masterVolume_, anyActive, DawStyle::kMasterWidth);

    // Fader
    {
        float fw = 40.f;
        ImGui::SetCursorPosX(groupX + (DawStyle::kMasterWidth - fw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg,          { 0.06f, 0.06f, 0.06f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_SliderGrab,        { 0.82f, 0.82f, 0.82f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,  { 1.00f, 1.00f, 1.00f, 1.f });
        if (ImGui::VSliderFloat("##master", { fw, DawStyle::kFaderHeight }, &masterVolume_, 0.f, 1.f, ""))
            Audio::SetMasterVolume(masterVolume_);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Master: %.2f", masterVolume_);
        ImGui::PopStyleColor(3);
    }

    // Readout
    {
        char buf[12]; snprintf(buf, sizeof(buf), "%.2f", masterVolume_);
        ImGui::SetCursorPosX(groupX + (DawStyle::kMasterWidth - ImGui::CalcTextSize(buf).x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, DawStyle::kTextDim);
        ImGui::TextUnformatted(buf);
        ImGui::PopStyleColor();
    }

    ImGui::EndGroup();
}

// ----------------------------------------------------------------
void AudioDebugPanel::DrawTrackStrip(Track& t, int idx, float groupX, bool& remove) {
    // Auto-remove finished playbacks each frame
    t.items.erase(std::remove_if(t.items.begin(), t.items.end(),
        [](const PlayingItem& item) { return !item.playback.IsPlaying() && !item.paused; }),
        t.items.end());

    const bool    focused  = (idx == focusedTrack_);
    const ImVec4& baseCol  = DawStyle::kChannelColors[t.colorIdx];
    const ImVec4  hdrCol   = focused
        ? ImVec4(std::min(baseCol.x + 0.18f, 1.f), std::min(baseCol.y + 0.18f, 1.f),
                 std::min(baseCol.z + 0.18f, 1.f), 1.f)
        : baseCol;

    bool anyActive = false;
    for (auto& item : t.items)
        if (item.playback.IsPlaying() && !item.paused) { anyActive = true; break; }

    ImGui::PushID(idx);
    ImGui::BeginGroup();

    // Colored header (click to focus)
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetCursorScreenPos();
        float w = DawStyle::kChannelWidth, h = DawStyle::kHeaderHeight;
        dl->AddRectFilled(pos, { pos.x + w, pos.y + h },
            ImGui::ColorConvertFloat4ToU32(hdrCol), 4.f, ImDrawFlags_RoundCornersTop);
        // Focus dot
        if (focused)
            dl->AddCircleFilled({ pos.x + 8.f, pos.y + h * 0.5f }, 3.f, IM_COL32(255, 255, 255, 210));
        std::string lbl = t.name;
        if (lbl.size() > 9) lbl = lbl.substr(0, 8) + "~";
        ImVec2 tsz = ImGui::CalcTextSize(lbl.c_str());
        dl->AddText({ pos.x + (w - tsz.x) * 0.5f, pos.y + (h - tsz.y) * 0.5f },
            IM_COL32(230, 230, 230, 255), lbl.c_str());
        ImGui::InvisibleButton("##hdr", { w, h });
        if (ImGui::IsItemClicked()) focusedTrack_ = idx;
    }

    // LED
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetCursorScreenPos();
        float r = 5.f;
        ImVec2 ctr = { pos.x + DawStyle::kChannelWidth * 0.5f, pos.y + r + 3.f };
        ImU32 col = anyActive ? IM_COL32(50, 255, 80, 255) : IM_COL32(28, 60, 28, 255);
        dl->AddCircleFilled(ctr, r, col);
        dl->AddCircle(ctr, r, IM_COL32(0, 0, 0, 160), 16, 1.2f);
        ImGui::Dummy({ DawStyle::kChannelWidth, r * 2.f + 6.f });
    }

    DrawDualVU(idx, t.volume, anyActive, DawStyle::kChannelWidth);

    // Volume fader (Track-level)
    {
        float fw = 34.f;
        ImGui::SetCursorPosX(groupX + (DawStyle::kChannelWidth - fw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg,          { 0.06f, 0.06f, 0.06f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_SliderGrab,        { 0.72f, 0.72f, 0.72f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,  { 1.00f, 1.00f, 1.00f, 1.f });
        if (ImGui::VSliderFloat("##vol", { fw, DawStyle::kFaderHeight }, &t.volume, 0.f, 1.f, ""))
            t.handle.SetVolume(t.volume);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Track Volume: %.2f", t.volume);
        ImGui::PopStyleColor(3);
    }

    // Volume readout
    {
        char buf[12]; snprintf(buf, sizeof(buf), "%.2f", t.volume);
        ImGui::SetCursorPosX(groupX + (DawStyle::kChannelWidth - ImGui::CalcTextSize(buf).x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, DawStyle::kTextDim);
        ImGui::TextUnformatted(buf);
        ImGui::PopStyleColor();
    }

    // Pan
    ImGui::PushStyleColor(ImGuiCol_Text, DawStyle::kTextDim);
    ImGui::SetCursorPosX(groupX);
    ImGui::TextUnformatted("PAN");
    ImGui::PopStyleColor();
    ImGui::SetCursorPosX(groupX);
    ImGui::SetNextItemWidth(DawStyle::kChannelWidth);
    if (ImGui::SliderFloat("##pan", &t.pan, -1.f, 1.f, "%.2f"))
        t.handle.SetPan(t.pan);

    // Pitch
    ImGui::PushStyleColor(ImGuiCol_Text, DawStyle::kTextDim);
    ImGui::SetCursorPosX(groupX);
    ImGui::TextUnformatted("PITCH");
    ImGui::PopStyleColor();
    ImGui::SetCursorPosX(groupX);
    ImGui::SetNextItemWidth(DawStyle::kChannelWidth);
    if (ImGui::SliderFloat("##pit", &t.pitch, 0.1f, 3.f, "%.2fx"))
        t.handle.SetPitch(t.pitch);

    ImGui::Spacing();

    // Stop All
    ImGui::SetCursorPosX(groupX);
    ImGui::PushStyleColor(ImGuiCol_Button,        DawStyle::kStopAll);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, DawStyle::kStopAllHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.35f, 0.15f, 0.04f, 1.f });
    if (ImGui::Button("Stop All", { DawStyle::kChannelWidth, 0.f })) {
        t.handle.StopAll();
        for (auto& item : t.items) item.paused = false;
    }
    ImGui::PopStyleColor(3);

    ImGui::Spacing();

    // Remove
    ImGui::SetCursorPosX(groupX);
    ImGui::PushStyleColor(ImGuiCol_Button,        { 0.22f, 0.10f, 0.10f, 1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.45f, 0.16f, 0.16f, 1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.15f, 0.06f, 0.06f, 1.f });
    if (ImGui::Button("Remove", { DawStyle::kChannelWidth, 0.f }))
        remove = true;
    ImGui::PopStyleColor(3);

    ImGui::EndGroup();
    ImGui::PopID();
}

// ----------------------------------------------------------------
//  Playback list for the focused track
// ----------------------------------------------------------------
void AudioDebugPanel::DrawPlaybackList(Track& t) {
    // Header
    {
        const ImVec4& col = DawStyle::kChannelColors[t.colorIdx];
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Text("%s", t.name.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, DawStyle::kTextDim);
        ImGui::Text("(%d active)", static_cast<int>(t.items.size()));
        ImGui::PopStyleColor();
    }
    ImGui::Separator();

    if (t.items.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, DawStyle::kTextDim);
        ImGui::TextUnformatted("No active playbacks");
        ImGui::PopStyleColor();
        return;
    }

    // Table: Sound | Vol | Pan | M | L | Transport | LED
    constexpr ImGuiTableFlags kTableFlags =
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("##items", 7, kTableFlags)) return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Sound",  ImGuiTableColumnFlags_WidthFixed, 82.f);
    ImGui::TableSetupColumn("Vol",    ImGuiTableColumnFlags_WidthFixed, 72.f);
    ImGui::TableSetupColumn("Pan",    ImGuiTableColumnFlags_WidthFixed, 72.f);
    ImGui::TableSetupColumn("M",      ImGuiTableColumnFlags_WidthFixed, 22.f);
    ImGui::TableSetupColumn("L",      ImGuiTableColumnFlags_WidthFixed, 22.f);
    ImGui::TableSetupColumn("Trans",  ImGuiTableColumnFlags_WidthFixed, 58.f);
    ImGui::TableSetupColumn("##led",  ImGuiTableColumnFlags_WidthFixed, 14.f);
    ImGui::TableHeadersRow();

    for (int i = 0; i < static_cast<int>(t.items.size()); ++i) {
        PlayingItem& item    = t.items[i];
        const bool   playing = item.playback.IsPlaying();

        ImGui::PushID(i);
        ImGui::TableNextRow();

        // Sound name
        ImGui::TableSetColumnIndex(0);
        std::string lbl = item.soundName;
        const size_t sl = lbl.find_last_of("/\\");
        if (sl != std::string::npos) lbl = lbl.substr(sl + 1);
        if (lbl.size() > 9) lbl = lbl.substr(0, 8) + "~";
        ImGui::TextUnformatted(lbl.c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", item.soundName.c_str());

        // Volume
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(68.f);
        if (ImGui::SliderFloat("##vol", &item.volume, 0.f, 1.f, "%.2f"))
            item.playback.SetVolume(item.volume);

        // Pan
        ImGui::TableSetColumnIndex(2);
        ImGui::SetNextItemWidth(68.f);
        if (ImGui::SliderFloat("##pan", &item.pan, -1.f, 1.f, "%.2f"))
            item.playback.SetPan(item.pan);

        // Mute
        ImGui::TableSetColumnIndex(3);
        {
            const bool wasMuted = item.muted;
            if (wasMuted) {
                ImGui::PushStyleColor(ImGuiCol_Button,        DawStyle::kMuteActive);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1.f, 0.4f, 0.4f, 1.f });
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.6f, 0.1f, 0.1f, 1.f });
            }
            if (ImGui::SmallButton("M")) { item.muted = !item.muted; item.playback.Mute(item.muted); }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mute");
            if (wasMuted) ImGui::PopStyleColor(3);
        }

        // Loop
        ImGui::TableSetColumnIndex(4);
        {
            const bool wasLoop = item.loop;
            if (wasLoop) {
                ImGui::PushStyleColor(ImGuiCol_Button,        DawStyle::kLoopActive);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.4f, 0.7f, 1.f, 1.f });
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.1f, 0.35f, 0.7f, 1.f });
            }
            if (ImGui::SmallButton("L")) { item.loop = !item.loop; item.playback.SetLoop(item.loop); }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Loop");
            if (wasLoop) ImGui::PopStyleColor(3);
        }

        // Transport: Pause/Resume | Stop
        ImGui::TableSetColumnIndex(5);
        {
            const bool wasPaused = item.paused;
            if (wasPaused) {
                ImGui::PushStyleColor(ImGuiCol_Button,        DawStyle::kPauseActive);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.9f, 0.78f, 0.18f, 1.f });
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.5f, 0.40f, 0.05f, 1.f });
            }
            if (ImGui::SmallButton("||")) {
                if (playing && !item.paused)     { item.playback.Pause();  item.paused = true;  }
                else if (item.paused)            { item.playback.Resume(); item.paused = false; }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause / Resume");
            if (wasPaused) ImGui::PopStyleColor(3);

            ImGui::SameLine(0, 3.f);

            if (ImGui::SmallButton("[]")) { item.playback.Stop(); item.paused = false; }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop");
        }

        // LED
        ImGui::TableSetColumnIndex(6);
        {
            ImDrawList* dl  = ImGui::GetWindowDrawList();
            ImVec2      pos = ImGui::GetCursorScreenPos();
            float r = 5.f;
            ImVec2 ctr = { pos.x + r, pos.y + r + 3.f };
            ImU32 col = (playing && !item.paused) ? IM_COL32(50, 255, 80,  255)
                       : item.paused              ? IM_COL32(220, 190, 30, 255)
                                                  : IM_COL32(28, 60, 28,   255);
            dl->AddCircleFilled(ctr, r, col);
            dl->AddCircle(ctr, r, IM_COL32(0, 0, 0, 160), 12, 1.f);
            ImGui::Dummy({ r * 2.f, r * 2.f + 6.f });
        }

        ImGui::PopID();
    }

    ImGui::EndTable();
}

// ================================================================
//  Global dark DAW style
// ================================================================
static void ApplyDawStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 4.f;
    s.FrameRounding     = 3.f;
    s.ScrollbarRounding = 3.f;
    s.GrabRounding      = 3.f;
    s.ItemSpacing       = { 6.f, 5.f };
    s.FramePadding      = { 6.f, 4.f };
    s.ScrollbarSize     = 10.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = { 0.10f, 0.10f, 0.10f, 1.f };
    c[ImGuiCol_ChildBg]              = { 0.12f, 0.12f, 0.12f, 1.f };
    c[ImGuiCol_PopupBg]              = { 0.14f, 0.14f, 0.14f, 0.95f };
    c[ImGuiCol_FrameBg]              = { 0.13f, 0.13f, 0.13f, 1.f };
    c[ImGuiCol_FrameBgHovered]       = { 0.20f, 0.20f, 0.20f, 1.f };
    c[ImGuiCol_FrameBgActive]        = { 0.10f, 0.10f, 0.10f, 1.f };
    c[ImGuiCol_TitleBg]              = { 0.08f, 0.08f, 0.08f, 1.f };
    c[ImGuiCol_TitleBgActive]        = { 0.13f, 0.13f, 0.13f, 1.f };
    c[ImGuiCol_Button]               = { 0.20f, 0.20f, 0.20f, 1.f };
    c[ImGuiCol_ButtonHovered]        = { 0.30f, 0.30f, 0.30f, 1.f };
    c[ImGuiCol_ButtonActive]         = { 0.14f, 0.14f, 0.14f, 1.f };
    c[ImGuiCol_SliderGrab]           = { 0.65f, 0.65f, 0.65f, 1.f };
    c[ImGuiCol_SliderGrabActive]     = { 0.92f, 0.92f, 0.92f, 1.f };
    c[ImGuiCol_CheckMark]            = { 0.80f, 0.80f, 0.80f, 1.f };
    c[ImGuiCol_Separator]            = { 0.25f, 0.25f, 0.25f, 1.f };
    c[ImGuiCol_ScrollbarBg]          = { 0.07f, 0.07f, 0.07f, 1.f };
    c[ImGuiCol_ScrollbarGrab]        = { 0.28f, 0.28f, 0.28f, 1.f };
    c[ImGuiCol_ScrollbarGrabHovered] = { 0.40f, 0.40f, 0.40f, 1.f };
    c[ImGuiCol_ScrollbarGrabActive]  = { 0.55f, 0.55f, 0.55f, 1.f };
    c[ImGuiCol_Text]                 = { 0.88f, 0.88f, 0.88f, 1.f };
    c[ImGuiCol_TextDisabled]         = { 0.40f, 0.40f, 0.40f, 1.f };
    c[ImGuiCol_Header]               = { 0.22f, 0.22f, 0.22f, 1.f };
    c[ImGuiCol_HeaderHovered]        = { 0.30f, 0.30f, 0.30f, 1.f };
    c[ImGuiCol_HeaderActive]         = { 0.16f, 0.16f, 0.16f, 1.f };
    c[ImGuiCol_ResizeGrip]           = { 0.26f, 0.26f, 0.26f, 1.f };
    c[ImGuiCol_ResizeGripHovered]    = { 0.45f, 0.45f, 0.45f, 1.f };
    c[ImGuiCol_ResizeGripActive]     = { 0.65f, 0.65f, 0.65f, 1.f };
}

// ================================================================
//  DX12 infrastructure
// ================================================================
static constexpr int  kWidth      = 1280;
static constexpr int  kHeight     = 720;
static constexpr UINT kFrameCount = 2;

static ComPtr<ID3D12Device>              g_device;
static ComPtr<ID3D12CommandQueue>        g_cmdQueue;
static ComPtr<IDXGISwapChain3>           g_swapChain;
static ComPtr<ID3D12CommandAllocator>    g_cmdAllocator[kFrameCount];
static ComPtr<ID3D12GraphicsCommandList> g_cmdList;
static ComPtr<ID3D12DescriptorHeap>      g_rtvHeap;
static ComPtr<ID3D12Resource>            g_renderTargets[kFrameCount];
static ComPtr<ID3D12DescriptorHeap>      g_srvHeap;
static ComPtr<ID3D12Fence>               g_fence;
static HANDLE                            g_fenceEvent    = nullptr;
static UINT64                            g_fenceValues[kFrameCount] = {};
static UINT64                            g_fenceSignaled = 0;
static UINT                              g_rtvDescSize   = 0;
static UINT                              g_frameIndex    = 0;
static HWND                              g_hwnd          = nullptr;

// ----------------------------------------------------------------
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ----------------------------------------------------------------
static bool InitDX12() {
    ComPtr<IDXGIFactory7> factory;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) return false;

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; SUCCEEDED(factory->EnumAdapterByGpuPreference(
            i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))); ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) break;
        adapter = nullptr;
    }

    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
    };
    for (auto level : levels)
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), level, IID_PPV_ARGS(&g_device)))) break;
    if (!g_device) return false;

    D3D12_COMMAND_QUEUE_DESC qd = {};
    g_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&g_cmdQueue));

    DXGI_SWAP_CHAIN_DESC1 scd   = {};
    scd.Width                   = kWidth;
    scd.Height                  = kHeight;
    scd.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc.Count        = 1;
    scd.BufferUsage             = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount             = kFrameCount;
    scd.SwapEffect              = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> sc1;
    factory->CreateSwapChainForHwnd(g_cmdQueue.Get(), g_hwnd, &scd, nullptr, nullptr, &sc1);
    sc1.As(&g_swapChain);
    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC rtvHd = {
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kFrameCount, D3D12_DESCRIPTOR_HEAP_FLAG_NONE
    };
    g_device->CreateDescriptorHeap(&rtvHd, IID_PPV_ARGS(&g_rtvHeap));
    g_rtvDescSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    auto rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kFrameCount; ++i) {
        g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_renderTargets[i]));
        g_device->CreateRenderTargetView(g_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += g_rtvDescSize;
    }

    for (UINT i = 0; i < kFrameCount; ++i)
        g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&g_cmdAllocator[i]));
    g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        g_cmdAllocator[0].Get(), nullptr, IID_PPV_ARGS(&g_cmdList));
    g_cmdList->Close();

    g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence));
    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_fenceEvent) return false;

    D3D12_DESCRIPTOR_HEAP_DESC srvHd = {
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
    };
    g_device->CreateDescriptorHeap(&srvHd, IID_PPV_ARGS(&g_srvHeap));

    return true;
}

// ----------------------------------------------------------------
static void WaitForGPU() {
    ++g_fenceSignaled;
    g_cmdQueue->Signal(g_fence.Get(), g_fenceSignaled);
    g_fence->SetEventOnCompletion(g_fenceSignaled, g_fenceEvent);
    WaitForSingleObject(g_fenceEvent, INFINITE);
}

// ----------------------------------------------------------------
static void RenderFrame(AudioDebugPanel& panel) {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    panel.Draw();

    ImGui::Render();

    g_cmdAllocator[g_frameIndex]->Reset();
    g_cmdList->Reset(g_cmdAllocator[g_frameIndex].Get(), nullptr);

    D3D12_RESOURCE_BARRIER barrier          = {};
    barrier.Type                            = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource            = g_renderTargets[g_frameIndex].Get();
    barrier.Transition.Subresource         = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore          = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter           = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_cmdList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += g_frameIndex * g_rtvDescSize;
    const float clear[] = { 0.08f, 0.08f, 0.08f, 1.0f };
    g_cmdList->ClearRenderTargetView(rtv, clear, 0, nullptr);
    g_cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    g_cmdList->SetDescriptorHeaps(1, g_srvHeap.GetAddressOf());
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_cmdList.Get());

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    g_cmdList->ResourceBarrier(1, &barrier);
    g_cmdList->Close();

    ID3D12CommandList* lists[] = { g_cmdList.Get() };
    g_cmdQueue->ExecuteCommandLists(1, lists);
    g_swapChain->Present(1, 0);

    ++g_fenceSignaled;
    g_cmdQueue->Signal(g_fence.Get(), g_fenceSignaled);
    g_fenceValues[g_frameIndex] = g_fenceSignaled;
    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();
    if (g_fence->GetCompletedValue() < g_fenceValues[g_frameIndex]) {
        g_fence->SetEventOnCompletion(g_fenceValues[g_frameIndex], g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

// ================================================================
//  Entry point
// ================================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"AudioMixer";
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, kWidth, kHeight };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowW(L"AudioMixer", L"Audio Mixer",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInst, nullptr);

    if (!InitDX12()) return 1;

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ApplyDawStyle();

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX12_Init(g_device.Get(), kFrameCount,
        DXGI_FORMAT_R8G8B8A8_UNORM, g_srvHeap.Get(),
        g_srvHeap->GetCPUDescriptorHandleForHeapStart(),
        g_srvHeap->GetGPUDescriptorHandleForHeapStart());

    Audio::Initialize();

    AudioDebugPanel panel;

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            RenderFrame(panel);
        }
    }

    WaitForGPU();
    Audio::Shutdown();
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_fenceEvent) CloseHandle(g_fenceEvent);
    DestroyWindow(g_hwnd);
    UnregisterClassW(L"AudioMixer", hInst);
    return 0;
}
