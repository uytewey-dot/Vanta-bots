#pragma once

#include "../../ImGui/imgui.h"
#include "../Support/Public/PlayerBotVersionSelection.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// Presentation only: no game objects, server actions or persistent state.
namespace MagnesiumUI
{
    inline bool Animations = true;
    inline ImVec4 Accent(float Alpha = 1.f) { return ImVec4(.32f, .86f, .74f, Alpha); }
    inline ImVec4 Background() { return ImVec4(.043f, .063f, .090f, 1.f); }
    inline ImVec4 Surface() { return ImVec4(.067f, .094f, .129f, 1.f); }
    inline ImVec4 Border() { return ImVec4(.15f, .20f, .25f, 1.f); }
    inline ImVec4 Muted() { return ImVec4(.57f, .65f, .72f, 1.f); }
    inline ImVec4 Text() { return ImVec4(.91f, .95f, .98f, 1.f); }
    inline ImVec4 Warning() { return ImVec4(.98f, .72f, .36f, 1.f); }
    inline float S(float Value) { return Value * ImGui::GetStyle().FontScaleDpi; }

    inline ImVec4 Mix(ImVec4 A, ImVec4 B, float T)
    {
        return ImVec4(A.x + (B.x - A.x) * T, A.y + (B.y - A.y) * T,
            A.z + (B.z - A.z) * T, A.w + (B.w - A.w) * T);
    }

    inline float Approach(float Value, float Target, float Speed = 18.f)
    {
        if (!Animations)
            return Target;
        const float Dt = (std::clamp)(ImGui::GetIO().DeltaTime, 0.f, .1f);
        return Value + (Target - Value) * (1.f - std::exp(-Speed * Dt));
    }

    inline float Animate(const char* Id, float Target, float Speed = 18.f)
    {
        auto* State = ImGui::GetStateStorage();
        const auto Key = ImGui::GetID(Id);
        const float Value = Approach(State->GetFloat(Key, Target), Target, Speed);
        State->SetFloat(Key, Value);
        return Value;
    }

    inline void ApplyStyle(float Dpi)
    {
        ImGui::StyleColorsDark();
        auto& Style = ImGui::GetStyle();
        Style.WindowRounding = 0.f;
        Style.WindowBorderSize = 0.f;
        Style.ChildRounding = 12.f;
        Style.PopupRounding = 10.f;
        Style.FrameRounding = 7.f;
        Style.FramePadding = ImVec2(10.f, 6.f);
        Style.ItemSpacing = ImVec2(12.f, 9.f);
        Style.ItemInnerSpacing = ImVec2(8.f, 5.f);
        Style.GrabMinSize = 12.f;
        Style.GrabRounding = 8.f;
        Style.ScrollbarSize = 10.f;
        Style.ScrollbarRounding = 8.f;
        Style.TabRounding = 6.f;
        Style.DisabledAlpha = .45f;
        auto* C = Style.Colors;
        C[ImGuiCol_WindowBg] = Background();
        C[ImGuiCol_ChildBg] = Background();
        C[ImGuiCol_PopupBg] = Surface();
        C[ImGuiCol_Border] = Border();
        C[ImGuiCol_BorderShadow] = ImVec4(0.f, 0.f, 0.f, 0.f);
        C[ImGuiCol_Text] = Text();
        C[ImGuiCol_TextDisabled] = Muted();
        C[ImGuiCol_TextSelectedBg] = Accent(.28f);
        C[ImGuiCol_FrameBg] = Surface();
        C[ImGuiCol_FrameBgHovered] = Mix(Surface(), Accent(), .12f);
        C[ImGuiCol_FrameBgActive] = Mix(Surface(), Accent(), .20f);
        C[ImGuiCol_TitleBg] = Background();
        C[ImGuiCol_TitleBgActive] = Surface();
        C[ImGuiCol_MenuBarBg] = Surface();
        C[ImGuiCol_CheckMark] = Accent();
        C[ImGuiCol_SliderGrab] = Accent();
        C[ImGuiCol_SliderGrabActive] = Mix(Accent(), Text(), .3f);
        C[ImGuiCol_Button] = Mix(Surface(), Border(), .45f);
        C[ImGuiCol_ButtonHovered] = Mix(Surface(), Accent(), .22f);
        C[ImGuiCol_ButtonActive] = Mix(Surface(), Accent(), .35f);
        C[ImGuiCol_Header] = Accent(.12f);
        C[ImGuiCol_HeaderHovered] = Accent(.20f);
        C[ImGuiCol_HeaderActive] = Accent(.28f);
        C[ImGuiCol_Separator] = Border();
        C[ImGuiCol_SeparatorHovered] = Accent(.5f);
        C[ImGuiCol_SeparatorActive] = Accent();
        C[ImGuiCol_ScrollbarBg] = Background();
        C[ImGuiCol_ScrollbarGrab] = Border();
        C[ImGuiCol_ScrollbarGrabHovered] = Muted();
        C[ImGuiCol_ScrollbarGrabActive] = Accent();
        C[ImGuiCol_Tab] = Surface();
        C[ImGuiCol_TabHovered] = Accent(.2f);
        C[ImGuiCol_TabSelected] = Accent(.12f);
        C[ImGuiCol_NavCursor] = Accent();
        C[ImGuiCol_PlotLines] = Accent();
        C[ImGuiCol_PlotHistogram] = Accent();
        Style.ScaleAllSizes(Dpi);
        Style.FontScaleDpi = Dpi;
    }

    inline void TextAt(ImVec2 Pos, const char* Value, ImVec4 Color, float Size = 17.f)
    {
        ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), S(Size), Pos,
            ImGui::GetColorU32(Color), Value);
    }

    inline float Pill(ImVec2 Pos, const char* Label, ImVec4 Color)
    {
        const auto Size = ImGui::CalcTextSize(Label);
        const float Width = Size.x + S(28.f);
        auto* Draw = ImGui::GetWindowDrawList();
        Draw->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + S(28.f)),
            ImGui::GetColorU32(ImVec4(Color.x, Color.y, Color.z, .10f)), S(14.f));
        Draw->AddCircleFilled(ImVec2(Pos.x + S(11.f), Pos.y + S(14.f)), S(3.f),
            ImGui::GetColorU32(Color));
        Draw->AddText(ImVec2(Pos.x + S(20.f), Pos.y + (S(28.f) - Size.y) * .5f),
            ImGui::GetColorU32(Color), Label);
        return Width;
    }

    inline void Header(float Width, double Version, const char* Status, ImVec4 StatusColor)
    {
        const auto P = ImGui::GetWindowPos();
        auto* Draw = ImGui::GetWindowDrawList();
        Draw->AddRectFilled(P, ImVec2(P.x + Width, P.y + S(72.f)),
            ImGui::GetColorU32(Surface()));
        Draw->AddLine(ImVec2(P.x, P.y + S(71.f)), ImVec2(P.x + Width, P.y + S(71.f)),
            ImGui::GetColorU32(Border()));
        const ImVec2 Mark(P.x + S(20.f), P.y + S(16.f));
        Draw->AddRectFilled(Mark, ImVec2(Mark.x + S(40.f), Mark.y + S(40.f)),
            ImGui::GetColorU32(Accent()), S(12.f));
        TextAt(ImVec2(Mark.x + S(13.f), Mark.y + S(8.f)), "V", Background(), 22.f);
        TextAt(ImVec2(P.x + S(73.f), P.y + S(16.f)), "VANTA BOTS", Text(), 22.f);
        TextAt(ImVec2(P.x + S(74.f), P.y + S(42.f)), "SERVER CONTROL", Muted(), 11.f);
        char Release[32];
        std::snprintf(Release, sizeof(Release), "Fortnite %.2f", Version);
        const float ReleaseW = ImGui::CalcTextSize(Release).x + S(28.f);
        const float StatusW = ImGui::CalcTextSize(Status).x + S(28.f);
        const float Right = P.x + Width - S(22.f);
        Pill(ImVec2(Right - ReleaseW, P.y + S(22.f)), Release, Accent());
        if (Width > S(640.f))
            Pill(ImVec2(Right - ReleaseW - S(10.f) - StatusW, P.y + S(22.f)), Status, StatusColor);
    }

    inline void Icon(ImVec2 P, int Kind, ImU32 Color)
    {
        auto* D = ImGui::GetWindowDrawList();
        auto V = [P](float X, float Y) { return ImVec2(P.x + S(X), P.y + S(Y)); };
        auto Line = [&](float X, float Y, float X2, float Y2) {
            D->AddLine(V(X, Y), V(X2, Y2), Color, S(1.5f));
        };
        if (Kind == 4) // bot
        {
            D->AddRect(V(2, 5), V(18, 17), Color, S(4.f), 0, S(1.5f));
            Line(10, 2, 10, 5);
            D->AddCircleFilled(V(7, 10), S(1.5f), Color);
            D->AddCircleFilled(V(13, 10), S(1.5f), Color);
            Line(7, 14, 13, 14);
        }
        else if (Kind == 2) // players
        {
            D->AddCircle(V(8, 6), S(3.5f), Color, 16, S(1.5f));
            D->AddBezierCubic(V(1, 18), V(1, 8), V(15, 8), V(15, 18), Color, S(1.5f));
            D->AddCircle(V(16, 8), S(2.5f), Color, 16, S(1.5f));
        }
        else if (Kind == 0 || Kind == 5 || Kind == 6)
        {
            for (int Y = 0; Y < 2; ++Y)
                for (int X = 0; X < 2; ++X)
                    D->AddRect(V(2.f + X * 10, 2.f + Y * 10), V(8.f + X * 10, 8.f + Y * 10),
                        Color, S(1.5f), 0, S(1.4f));
        }
        else if (Kind == 1 || Kind == 9)
        {
            for (int I = 0; I < 3; ++I)
            {
                D->AddCircleFilled(V(3, 4.f + I * 6), S(1.4f), Color);
                Line(8, 4.f + I * 6, 18, 4.f + I * 6);
            }
        }
        else
        {
            D->AddCircle(V(10, 10), S(8.f), Color, 24, S(1.5f));
            if (Kind == 8) { Line(10, 9, 10, 15); D->AddCircleFilled(V(10, 5), S(1.f), Color); }
            else { Line(10, 2, 10, 18); Line(2, 10, 18, 10); }
        }
    }

    inline bool Navigation(const char* Label, int Id, bool Active, float Width, float Height)
    {
        ImGui::PushID(Id);
        const auto P = ImGui::GetCursorScreenPos();
        const bool Pressed = ImGui::InvisibleButton("##nav", ImVec2(Width, Height), ImGuiButtonFlags_EnableNav);
        const float Hover = Animate("hover", ImGui::IsItemHovered() ? 1.f : 0.f);
        const float Selected = Animate("selected", Active || Pressed ? 1.f : 0.f);
        auto* D = ImGui::GetWindowDrawList();
        const auto Fill = Mix(Surface(), Accent(), Selected * .12f + Hover * .055f);
        D->AddRectFilled(P, ImVec2(P.x + Width, P.y + Height), ImGui::GetColorU32(Fill), S(8.f));
        if (ImGui::IsItemFocused())
            D->AddRect(P, ImVec2(P.x + Width, P.y + Height), ImGui::GetColorU32(Accent(.7f)), S(8.f));
        const auto Color = Mix(Mix(Muted(), Text(), Hover), Accent(), Selected);
        Icon(ImVec2(P.x + S(12.f), P.y + (Height - S(20.f)) * .5f), Id, ImGui::GetColorU32(Color));
        D->AddText(ImVec2(P.x + S(44.f), P.y + (Height - ImGui::GetTextLineHeight()) * .5f),
            ImGui::GetColorU32(Color), Label);
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::PopID();
        return Pressed;
    }

    inline bool Toggle(const char* Label, bool* Value)
    {
        ImGui::PushID(Label);
        const char* End = std::strstr(Label, "##");
        if (!End) End = Label + std::strlen(Label);
        const auto LabelSize = ImGui::CalcTextSize(Label, End);
        const ImVec2 P = ImGui::GetCursorScreenPos();
        const float Height = (std::max)(S(24.f), LabelSize.y);
        const float Width = S(38.f) + (LabelSize.x > 0.f ? S(10.f) + LabelSize.x : 0.f);
        const bool Pressed = ImGui::InvisibleButton("##toggle", ImVec2(Width, Height), ImGuiButtonFlags_EnableNav);
        if (Pressed) *Value = !*Value;
        const float On = Animate("on", *Value ? 1.f : 0.f);
        const float Hover = Animate("hover", ImGui::IsItemHovered() ? 1.f : 0.f);
        auto* D = ImGui::GetWindowDrawList();
        const ImVec2 Min(P.x, P.y + (Height - S(22.f)) * .5f);
        const ImVec2 Max(Min.x + S(38.f), Min.y + S(22.f));
        D->AddRectFilled(Min, Max, ImGui::GetColorU32(Mix(Mix(Border(), Muted(), Hover * .15f), Accent(), On)), S(11.f));
        D->AddCircleFilled(ImVec2(Min.x + S(11.f) + S(16.f) * On, Min.y + S(11.f)), S(7.f),
            ImGui::GetColorU32(Mix(Text(), Background(), On)));
        if (ImGui::IsItemFocused())
            D->AddRect(ImVec2(Min.x - S(2.f), Min.y - S(2.f)), ImVec2(Max.x + S(2.f), Max.y + S(2.f)),
                ImGui::GetColorU32(Accent()), S(13.f));
        D->AddText(ImVec2(P.x + S(48.f), P.y + (Height - LabelSize.y) * .5f),
            ImGui::GetColorU32(Text()), Label, End);
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::PopID();
        return Pressed;
    }

    inline const char* PageName(int Id)
    {
        static const char* Names[] = { "Match overview", "Playlists", "Players", "Lategame",
            "Player bots", "Creative", "Custom map", "Trickshot", "Credits", "Calendar" };
        return Id >= 0 && Id < 10 ? Names[Id] : "Vanta Bots";
    }

    inline void PageHeading(int Id)
    {
        ImGui::PushFont(nullptr, 26.f);
        ImGui::TextUnformatted(PageName(Id));
        ImGui::PopFont();
        ImGui::TextColored(Muted(), "%s", Id == 4
            ? "Your lobby. Your rules. Set up the perfect opponents."
            : "Configure your session and make it your own.");
        ImGui::Dummy(ImVec2(0.f, S(8.f)));
    }

    inline bool VersionTile(const char* Label, const char* Subtitle, bool Selected, float Width)
    {
        const auto P = ImGui::GetCursorScreenPos();
        const float Height = S(Subtitle ? 65.f : 40.f);
        const bool Pressed = ImGui::InvisibleButton("##version", ImVec2(Width, Height), ImGuiButtonFlags_EnableNav);
        const float Hover = Animate("hover", ImGui::IsItemHovered() ? 1.f : 0.f);
        const float On = Animate("on", Selected || Pressed ? 1.f : 0.f);
        auto* D = ImGui::GetWindowDrawList();
        const ImVec2 Max(P.x + Width, P.y + Height);
        D->AddRectFilled(P, Max, ImGui::GetColorU32(Mix(Surface(), Accent(), On * .09f + Hover * .04f)), S(9.f));
        D->AddRect(P, Max, ImGui::GetColorU32(Mix(Border(), Accent(), (std::max)(On, Hover * .5f))), S(9.f));
        if (ImGui::IsItemFocused())
            D->AddRect(ImVec2(P.x + S(3.f), P.y + S(3.f)), ImVec2(Max.x - S(3.f), Max.y - S(3.f)),
                ImGui::GetColorU32(Accent(.65f)), S(7.f));
        TextAt(ImVec2(P.x + S(14.f), P.y + S(10.f)), Label, Mix(Text(), Accent(), On), Subtitle ? 22.f : 17.f);
        if (Subtitle)
            TextAt(ImVec2(P.x + S(14.f), P.y + S(39.f)), Subtitle, Muted(), 12.f);
        const ImVec2 Check(Max.x - S(18.f), P.y + S(20.f));
        D->AddCircle(Check, S(6.f), ImGui::GetColorU32(Mix(Border(), Accent(), On)), 20, S(1.5f));
        if (On > .01f)
            D->AddCircleFilled(Check, S(3.f) * On, ImGui::GetColorU32(Accent(On)));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        return Pressed;
    }

    inline bool VersionPicker(int& Selection, float Width)
    {
        namespace P = PlayerBotVersionSelection;
        ImGui::PushID("bot-version-picker");
        bool Changed = false;
        ImGui::PushID(P::Automatic);
        if (VersionTile("Automatic (current game)", nullptr, Selection == P::Automatic, Width))
        {
            Changed = Selection != P::Automatic;
            Selection = P::Automatic;
        }
        ImGui::PopID();
        const int Columns = Width >= S(510.f) ? 3 : (Width >= S(320.f) ? 2 : 1);
        const float Gap = S(10.f);
        const float TileWidth = (Width - Gap * (Columns - 1)) / Columns;
        const char* Subtitles[P::Count] = { "Current game", "Chapter 5 / Season 1", "Chapter 5 / Season 4",
            "Chapter 2 / Remix", "Chapter 3 / Season 1", "Chapter 4 / Season 2", "Chapter 4 / Season 4",
            "Chapter 2 / Season 2", "Chapter 2 / Season 4" };
        for (int I = 1; I < P::Count; ++I)
        {
            const int Id = P::DisplayOrder[I];
            if ((I - 1) % Columns != 0) ImGui::SameLine(0.f, Gap);
            ImGui::PushID(Id);
            if (VersionTile(P::Options[Id].PreferenceValue, Subtitles[Id], Selection == Id, TileWidth))
            {
                Changed = Selection != Id;
                Selection = Id;
            }
            ImGui::PopID();
        }
        ImGui::PopID();
        return Changed;
    }

    inline void Notice(const char* Title, const char* Detail, float Width, bool Good)
    {
        const auto P = ImGui::GetCursorScreenPos();
        const auto Color = Good ? Accent() : Warning();
        const float Wrap = (std::max)(S(40.f), Width - S(28.f));
        const auto DetailSize = ImGui::CalcTextSize(Detail, nullptr, false, Wrap);
        const float Height = S(28.f) + ImGui::GetTextLineHeight() + DetailSize.y;
        auto* D = ImGui::GetWindowDrawList();
        D->AddRectFilled(P, ImVec2(P.x + Width, P.y + Height),
            ImGui::GetColorU32(ImVec4(Color.x, Color.y, Color.z, .06f)), S(8.f));
        D->AddText(ImVec2(P.x + S(14.f), P.y + S(10.f)), ImGui::GetColorU32(Color), Title);
        D->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(P.x + S(14.f), P.y + S(17.f) + ImGui::GetTextLineHeight()),
            ImGui::GetColorU32(Muted()), Detail, nullptr, Wrap);
        ImGui::Dummy(ImVec2(Width, Height));
    }
}
