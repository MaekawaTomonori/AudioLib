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

    static constexpr float kChannelWidth = 90.f;
    static constexpr float kMasterWidth  = 104.f;
    static constexpr float kFaderHeight  = 130.f;
    static constexpr float kVUWidth      = 10.f;
    static constexpr float kVUHeight     = 90.f;
    static constexpr float kHeaderHeight = 26.f;

    static const ImVec4 kTextDim     = { 0.50f, 0.50f, 0.50f, 1.f };
    static const ImVec4 kMuteActive  = { 0.85f, 0.25f, 0.25f, 1.f };
    static const ImVec4 kLoopActive  = { 0.25f, 0.55f, 0.85f, 1.f };
    static const ImVec4 kPlayActive  = { 0.22f, 0.72f, 0.38f, 1.f };
    static const ImVec4 kPauseActive = { 0.72f, 0.60f, 0.10f, 1.f };

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
        dl->AddRectFilled({ x, y + h - fillH }, { x + w, y + h },
            IM_COL32(40, 200, 60, 255), 2.f);
    } else if (fillH <= greenH + yellowH) {
        float yf = fillH - greenH;
        dl->AddRectFilled({ x, greenTop },       { x + w, y + h },
            IM_COL32(40, 200, 60,  255), 2.f);
        dl->AddRectFilled({ x, greenTop - yf },  { x + w, greenTop },
            IM_COL32(220, 200, 30, 255), 2.f);
    } else {
        float rf = fillH - greenH - yellowH;
        dl->AddRectFilled({ x, greenTop },       { x + w, y + h },
            IM_COL32(40, 200, 60,  255), 2.f);
        dl->AddRectFilled({ x, yellowTop },      { x + w, greenTop },
            IM_COL32(220, 200, 30, 255), 2.f);
        dl->AddRectFilled({ x, yellowTop - rf }, { x + w, yellowTop },
            IM_COL32(220, 60, 40,  255), 2.f);
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
    // --- Loaded audio asset (Audio::Handle) ---
    struct LoadedSound {
        std::string   path;
        Audio::Handle handle;
        int           colorIdx = 0;
    };

    // --- Active playback instance (Audio::PlaybackHandle) in Mixer ---
    struct PlayingTrack {
        std::string           name;
        Audio::PlaybackHandle playback;
        float  volume   = 1.0f;
        float  pitch    = 1.0f;
        float  pan      = 0.0f;
        bool   muted    = false;
        bool   loop     = false;
        bool   paused   = false;
        int    colorIdx = 0;
    };

    // --- Windows ---
    void DrawLoaderWindow();   // Load audio files, select for Player
    void DrawPlayerWindow();   // Pre-play parameter adjustment + send to Mixer
    void DrawMixerWindow();    // Real-time control of playing tracks

    // --- Strip helpers ---
    void DrawMasterChannel(float groupX);
    void DrawTrackStrip(PlayingTrack& t, int idx, float groupX, bool& remove);
    void DrawDualVU(int seed, float volume, bool playing, float stripWidth);

    // --- State ---
    char  pathBuf_[256] = "fx.wav";
    float masterVolume_ = 1.0f;
    int   nextColorIdx_ = 0;
    int   selectedSound_ = -1;

    // Player pre-play parameters
    float playerVolume_ = 1.0f;
    float playerPitch_  = 1.0f;
    float playerPan_    = 0.0f;
    bool  playerLoop_   = false;

    std::vector<LoadedSound>  loadedSounds_;
    std::vector<PlayingTrack> playingTracks_;
};

// ----------------------------------------------------------------
void AudioDebugPanel::Draw() {
    DrawLoaderWindow();
    DrawPlayerWindow();
    DrawMixerWindow();
}

// ================================================================
//  Track Loader window
//  - Load audio files and manage the asset list
//  - Click a row to select it for the Player
// ================================================================
void AudioDebugPanel::DrawLoaderWindow() {
    ImGui::SetNextWindowSize({ 400.f, 210.f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({ 10.f, 10.f },    ImGuiCond_FirstUseEver);
    ImGui::Begin("Track Loader");

    // --- Path input + Load button ---
    ImGui::SetNextItemWidth(220.f);
    ImGui::InputText("##path", pathBuf_, sizeof(pathBuf_));
    ImGui::SameLine(0, 6.f);

    ImGui::PushStyleColor(ImGuiCol_Button,        { 0.18f, 0.42f, 0.18f, 1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.24f, 0.55f, 0.24f, 1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.12f, 0.30f, 0.12f, 1.f });
    if (ImGui::Button("Load") && pathBuf_[0] != '\0') {
        loadedSounds_.push_back({ pathBuf_, Audio::Handle(pathBuf_), nextColorIdx_ });
        nextColorIdx_ = (nextColorIdx_ + 1) % DawStyle::kColorCount;
        selectedSound_ = static_cast<int>(loadedSounds_.size()) - 1;
    }
    ImGui::PopStyleColor(3);

    ImGui::Separator();

    // --- Loaded sound list ---
    ImGui::BeginChild("##SoundList", { 0.f, 0.f }, false);
    int removeIdx = -1;
    for (int i = 0; i < static_cast<int>(loadedSounds_.size()); ++i) {
        ImGui::PushID(i);

        const ImVec4& col = DawStyle::kChannelColors[loadedSounds_[i].colorIdx];
        ImGui::PushStyleColor(ImGuiCol_Header,        col);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(col.x + 0.1f, col.y + 0.1f, col.z + 0.1f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(col.x - 0.1f, col.y - 0.1f, col.z - 0.1f, 1.f));

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
        if (selectedSound_ == removeIdx)        selectedSound_ = -1;
        else if (selectedSound_ > removeIdx)    --selectedSound_;
    }
    ImGui::EndChild();

    ImGui::End();
}

// ================================================================
//  Player window
//  - Shows the selected sound from Loader
//  - Adjust volume/pitch/pan/loop before playing
//  - PLAY sends a new PlaybackHandle to the Mixer
// ================================================================
void AudioDebugPanel::DrawPlayerWindow() {
    ImGui::SetNextWindowSize({ 450.f, 210.f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({ 420.f, 10.f },   ImGuiCond_FirstUseEver);
    ImGui::Begin("Player");

    const bool hasSelection = (selectedSound_ >= 0 &&
                               selectedSound_ < static_cast<int>(loadedSounds_.size()));

    // --- Selected sound display ---
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

    // --- Pre-play parameters ---
    ImGui::BeginDisabled(!hasSelection);

    ImGui::SetNextItemWidth(260.f);
    ImGui::SliderFloat("Volume##p", &playerVolume_, 0.f, 1.f, "%.2f");

    ImGui::SetNextItemWidth(260.f);
    ImGui::SliderFloat("Pitch##p",  &playerPitch_,  0.1f, 3.f, "%.2fx");

    ImGui::SetNextItemWidth(260.f);
    ImGui::SliderFloat("Pan##p",    &playerPan_,    -1.f, 1.f, "%.2f");

    // Loop toggle
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

    // PLAY button — sends to Mixer
    ImGui::PushStyleColor(ImGuiCol_Button,        DawStyle::kPlayActive);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.35f, 0.90f, 0.50f, 1.f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.12f, 0.50f, 0.22f, 1.f });
    if (ImGui::Button("  >  PLAY  ") && hasSelection) {
        auto& sound = loadedSounds_[selectedSound_];

        PlayingTrack t;
        t.name     = sound.path;
        t.playback = sound.handle.Play(playerVolume_, playerPitch_, playerLoop_);
        t.playback.SetPan(playerPan_);
        t.volume   = playerVolume_;
        t.pitch    = playerPitch_;
        t.pan      = playerPan_;
        t.loop     = playerLoop_;
        t.colorIdx = sound.colorIdx;
        playingTracks_.push_back(std::move(t));
    }
    ImGui::PopStyleColor(3);

    ImGui::EndDisabled();

    ImGui::End();
}

// ================================================================
//  Mixer window
//  - Master (leftmost) + active PlaybackHandle strips
//  - Real-time volume/pitch/pan/mute/loop/pause/stop control
// ================================================================
void AudioDebugPanel::DrawMixerWindow() {
    ImGui::SetNextWindowSize({ 1260.f, 450.f }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({ 10.f, 230.f },    ImGuiCond_FirstUseEver);
    ImGui::Begin("Mixer");

    ImGui::BeginChild("##MixerScroll", { 0.f, 0.f }, false,
        ImGuiWindowFlags_HorizontalScrollbar);

    // Master is the leftmost strip
    float gx = ImGui::GetCursorPosX();
    DrawMasterChannel(gx);
    ImGui::SameLine(0, 0.f);

    ImGui::PushStyleColor(ImGuiCol_Separator, { 0.32f, 0.32f, 0.32f, 1.f });
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 8.f);

    // Playing tracks
    int removeIdx = -1;
    for (int i = 0; i < static_cast<int>(playingTracks_.size()); ++i) {
        bool remove = false;
        float trackGx = ImGui::GetCursorPosX();
        DrawTrackStrip(playingTracks_[i], i, trackGx, remove);
        if (remove) removeIdx = i;
        ImGui::SameLine(0, 8.f);
    }
    if (removeIdx >= 0)
        playingTracks_.erase(playingTracks_.begin() + removeIdx);

    ImGui::EndChild();
    ImGui::End();
}

// ----------------------------------------------------------------
void AudioDebugPanel::DrawDualVU(int seed, float volume, bool playing, float stripWidth) {
    float time   = static_cast<float>(ImGui::GetTime());
    float levelL = playing ? SimulateVU(seed,     time) * volume : 0.f;
    float levelR = playing ? SimulateVU(seed + 7, time) * volume : 0.f;

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    ImVec2      pos   = ImGui::GetCursorScreenPos();
    float       gap   = 3.f;
    float       totW  = DawStyle::kVUWidth * 2.f + gap;
    float       offX  = (stripWidth - totW) * 0.5f;

    DrawVUBar(dl, pos.x + offX,                         pos.y,
        DawStyle::kVUWidth, DawStyle::kVUHeight, levelL);
    DrawVUBar(dl, pos.x + offX + DawStyle::kVUWidth + gap, pos.y,
        DawStyle::kVUWidth, DawStyle::kVUHeight, levelR);

    ImGui::Dummy({ stripWidth, DawStyle::kVUHeight });
}

// ----------------------------------------------------------------
void AudioDebugPanel::DrawMasterChannel(float groupX) {
    bool anyPlaying = false;
    for (auto& t : playingTracks_)
        if (t.playback.IsPlaying() && !t.paused) { anyPlaying = true; break; }

    ImGui::BeginGroup();

    // Header
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetCursorScreenPos();
        float       w   = DawStyle::kMasterWidth;
        float       h   = DawStyle::kHeaderHeight;
        dl->AddRectFilled(pos, { pos.x + w, pos.y + h },
            IM_COL32(70, 70, 70, 255), 4.f, ImDrawFlags_RoundCornersTop);
        const char* label = "MASTER";
        ImVec2 tsz = ImGui::CalcTextSize(label);
        dl->AddText({ pos.x + (w - tsz.x) * 0.5f, pos.y + (h - tsz.y) * 0.5f },
            IM_COL32(230, 230, 230, 255), label);
        ImGui::Dummy({ w, h });
    }

    // LED
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetCursorScreenPos();
        float       r   = 5.f;
        ImVec2 ctr = { pos.x + DawStyle::kMasterWidth * 0.5f, pos.y + r + 3.f };
        ImU32  col = anyPlaying ? IM_COL32(50, 255, 80, 255) : IM_COL32(28, 60, 28, 255);
        dl->AddCircleFilled(ctr, r, col);
        dl->AddCircle(ctr, r, IM_COL32(0, 0, 0, 160), 16, 1.2f);
        ImGui::Dummy({ DawStyle::kMasterWidth, r * 2.f + 6.f });
    }

    // VU
    DrawDualVU(99, masterVolume_, anyPlaying, DawStyle::kMasterWidth);

    // Fader
    {
        float fw = 40.f;
        ImGui::SetCursorPosX(groupX + (DawStyle::kMasterWidth - fw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg,          { 0.06f, 0.06f, 0.06f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_SliderGrab,        { 0.82f, 0.82f, 0.82f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,  { 1.00f, 1.00f, 1.00f, 1.f });
        if (ImGui::VSliderFloat("##master", { fw, DawStyle::kFaderHeight }, &masterVolume_, 0.f, 1.f, ""))
            Audio::SetMasterVolume(masterVolume_);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Master: %.2f", masterVolume_);
        ImGui::PopStyleColor(3);
    }

    // Readout
    {
        char buf[12];
        snprintf(buf, sizeof(buf), "%.2f", masterVolume_);
        float tx = (DawStyle::kMasterWidth - ImGui::CalcTextSize(buf).x) * 0.5f;
        ImGui::SetCursorPosX(groupX + tx);
        ImGui::PushStyleColor(ImGuiCol_Text, DawStyle::kTextDim);
        ImGui::TextUnformatted(buf);
        ImGui::PopStyleColor();
    }

    ImGui::EndGroup();
}

// ----------------------------------------------------------------
void AudioDebugPanel::DrawTrackStrip(PlayingTrack& t, int idx, float groupX, bool& remove) {
    // Filename label
    const size_t slash = t.name.find_last_of("/\\");
    std::string  label = (slash != std::string::npos) ? t.name.substr(slash + 1) : t.name;
    if (label.size() > 11) label = label.substr(0, 10) + "~";

    const ImVec4& hdrColor = DawStyle::kChannelColors[t.colorIdx];
    const bool    playing  = t.playback.IsPlaying();

    ImGui::PushID(idx);
    ImGui::BeginGroup();

    // Colored header
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetCursorScreenPos();
        float       w   = DawStyle::kChannelWidth;
        float       h   = DawStyle::kHeaderHeight;
        dl->AddRectFilled(pos, { pos.x + w, pos.y + h },
            ImGui::ColorConvertFloat4ToU32(hdrColor), 4.f, ImDrawFlags_RoundCornersTop);
        ImVec2 tsz = ImGui::CalcTextSize(label.c_str());
        dl->AddText({ pos.x + (w - tsz.x) * 0.5f, pos.y + (h - tsz.y) * 0.5f },
            IM_COL32(230, 230, 230, 255), label.c_str());
        ImGui::Dummy({ w, h });
    }

    // LED
    {
        ImDrawList* dl  = ImGui::GetWindowDrawList();
        ImVec2      pos = ImGui::GetCursorScreenPos();
        float       r   = 5.f;
        ImVec2 ctr = { pos.x + DawStyle::kChannelWidth * 0.5f, pos.y + r + 3.f };
        ImU32  col = (playing && !t.paused) ? IM_COL32(50, 255, 80,  255)
                   : t.paused               ? IM_COL32(220, 190, 30, 255)
                                            : IM_COL32(28, 60, 28,   255);
        dl->AddCircleFilled(ctr, r, col);
        dl->AddCircle(ctr, r, IM_COL32(0, 0, 0, 160), 16, 1.2f);
        ImGui::Dummy({ DawStyle::kChannelWidth, r * 2.f + 6.f });
    }

    // VU
    DrawDualVU(idx, t.volume, playing && !t.paused, DawStyle::kChannelWidth);

    // Volume fader
    {
        float fw = 34.f;
        ImGui::SetCursorPosX(groupX + (DawStyle::kChannelWidth - fw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg,         { 0.06f, 0.06f, 0.06f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_SliderGrab,       { 0.72f, 0.72f, 0.72f, 1.f });
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, { 1.00f, 1.00f, 1.00f, 1.f });
        if (ImGui::VSliderFloat("##vol", { fw, DawStyle::kFaderHeight }, &t.volume, 0.f, 1.f, ""))
            t.playback.SetVolume(t.volume);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Volume: %.2f", t.volume);
        ImGui::PopStyleColor(3);
    }

    // Volume readout
    {
        char buf[12];
        snprintf(buf, sizeof(buf), "%.2f", t.volume);
        float tx = (DawStyle::kChannelWidth - ImGui::CalcTextSize(buf).x) * 0.5f;
        ImGui::SetCursorPosX(groupX + tx);
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
        t.playback.SetPan(t.pan);

    // Pitch
    ImGui::PushStyleColor(ImGuiCol_Text, DawStyle::kTextDim);
    ImGui::SetCursorPosX(groupX);
    ImGui::TextUnformatted("PITCH");
    ImGui::PopStyleColor();
    ImGui::SetCursorPosX(groupX);
    ImGui::SetNextItemWidth(DawStyle::kChannelWidth);
    if (ImGui::SliderFloat("##pit", &t.pitch, 0.1f, 3.f, "%.2fx"))
        t.playback.SetPitch(t.pitch);

    // Mute / Loop
    // FIX: capture state BEFORE the button to keep Push/Pop balanced.
    //      Toggling inside the button changes the state mid-frame,
    //      causing Push/Pop count mismatch if the condition is re-evaluated.
    ImGui::Spacing();
    ImGui::SetCursorPosX(groupX);
    {
        float bw = (DawStyle::kChannelWidth - 4.f) * 0.5f;

        const bool wasMuted = t.muted;
        if (wasMuted) {
            ImGui::PushStyleColor(ImGuiCol_Button,        DawStyle::kMuteActive);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 1.f, 0.4f, 0.4f, 1.f });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.6f, 0.1f, 0.1f, 1.f });
        }
        if (ImGui::Button("M", { bw, 0.f })) {
            t.muted = !t.muted;
            t.playback.Mute(t.muted);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Mute");
        if (wasMuted) ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 4.f);

        const bool wasLoop = t.loop;
        if (wasLoop) {
            ImGui::PushStyleColor(ImGuiCol_Button,        DawStyle::kLoopActive);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.4f, 0.7f, 1.f, 1.f });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.1f, 0.35f, 0.7f, 1.f });
        }
        if (ImGui::Button("L", { bw, 0.f })) {
            t.loop = !t.loop;
            t.playback.SetLoop(t.loop);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Loop");
        if (wasLoop) ImGui::PopStyleColor(3);
    }

    // Transport: Play / Pause / Stop
    // Note: Unicode symbols (▶ ⏸ ■) require a font with those glyphs.
    //       Default ImGui font (ProggyClean) does not include them.
    ImGui::Spacing();
    ImGui::SetCursorPosX(groupX);
    {
        float bw = (DawStyle::kChannelWidth - 8.f) / 3.f;

        // Capture states before any button can change them
        const bool wasPlayingActive = playing && !t.paused;
        const bool wasPaused        = t.paused;

        // Play ">"
        if (wasPlayingActive) {
            ImGui::PushStyleColor(ImGuiCol_Button,        DawStyle::kPlayActive);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.35f, 0.9f, 0.5f, 1.f });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.12f, 0.5f, 0.22f, 1.f });
        }
        if (ImGui::Button(" > ", { bw, 0.f })) {
            if (t.paused) {
                t.playback.Resume();
                t.paused = false;
            } else {
                t.playback.Stop();
                t.playback = Audio::Handle(t.name).Play(t.volume, t.pitch, t.loop);
                t.playback.SetPan(t.pan);
                if (t.muted) t.playback.Mute(true);
                t.paused = false;
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play");
        if (wasPlayingActive) ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 4.f);

        // Pause "||"
        if (wasPaused) {
            ImGui::PushStyleColor(ImGuiCol_Button,        DawStyle::kPauseActive);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.9f, 0.78f, 0.18f, 1.f });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  { 0.5f, 0.40f, 0.05f, 1.f });
        }
        if (ImGui::Button("||", { bw, 0.f })) {
            if (playing && !t.paused) {
                t.playback.Pause();
                t.paused = true;
            } else if (t.paused) {
                t.playback.Resume();
                t.paused = false;
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause / Resume");
        if (wasPaused) ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 4.f);

        // Stop "[]"
        if (ImGui::Button("[]", { bw, 0.f })) {
            t.playback.Stop();
            t.paused = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop");
    }

    // Remove
    ImGui::Spacing();
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
