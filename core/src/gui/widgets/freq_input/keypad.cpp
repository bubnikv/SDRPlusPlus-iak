#include <gui/widgets/freq_input.h>
#include <gui/widgets/popup_dialog.h>
#include <gui/style.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace freq_input {

    // Format a frequency in Hz with '.' group separators, matching the widget.
    static std::string groupHz(uint64_t hz) {
        std::string s = std::to_string(hz);
        for (int pos = (int)s.size() - 3; pos > 0; pos -= 3) {
            s.insert(pos, ".");
        }
        return s;
    }

    static int digitCount(uint64_t value) {
        int count = 1;
        while (value >= 10) {
            value /= 10;
            count++;
        }
        return count;
    }

    static int integerDigitLimit(const Context& ctx) {
        if (!ctx.limited) { return 6; } // dialog entry is MHz, capped at 999999 MHz
        return std::min(6, digitCount(ctx.rangeHi() / 1000000));
    }

    static uint64_t clampHz(double hz, const Context& ctx) {
        if (!std::isfinite(hz) || hz < 0.0) { hz = 0.0; }
        hz = std::min(hz, 999999999999.0);

        if (ctx.limited) {
            hz = std::clamp(hz, (double)ctx.rangeLo(), (double)ctx.rangeHi());
        }

        return (uint64_t)hz;
    }

    void Keypad::onOpen() {
        entry.clear();
    }

    void Keypad::key(char k, const Context& ctx) {
        if (k >= '0' && k <= '9') {
            size_t dot = entry.find('.');
            if (dot == std::string::npos) {
                if (entry == "0") { entry.clear(); }
                if ((int)entry.size() < integerDigitLimit(ctx)) { entry += k; }
            }
            else if (entry.size() - dot - 1 < 6) { // down to 1 Hz
                entry += k;
            }
        }
        else if (k == '.') {
            if (entry.empty()) {
                // IC-705 shorthand: [.] first re-enters the current MHz digits, so
                // retuning within the band is just [.] plus the kHz digits.
                uint64_t currentHz = clampHz((double)ctx.frequency, ctx);
                entry = std::to_string(currentHz / 1000000) + '.';
            }
            else if (entry.find('.') == std::string::npos) {
                entry += '.';
            }
        }
    }

    // [ENT]. The entry is a decimal number in MHz; digits left blank below the
    // last entered one become zeros, same as the IC-705. An empty entry commits
    // nothing -- the dialog just closes.
    void Keypad::commit(const Context& ctx, Outcome& out) const {
        if (entry.empty()) { return; }
        out.frequency = clampHz(round(atof(entry.c_str()) * 1e6), ctx);
        out.commit = true;
    }

    Outcome Keypad::draw(const Context& ctx, const Metrics& m) {
        Outcome out;
        ImGuiIO& kio = ImGui::GetIO();
        const uint64_t rangeLo = ctx.rangeLo();
        const uint64_t rangeHi = ctx.rangeHi();
        const ImVec4 errorCol(1.0f, 0.20f, 0.12f, 1.0f);
        double rawHz = 0.0;
        uint64_t targetHz = 0;
        bool belowRange = false;
        bool aboveRange = false;
        if (!entry.empty()) {
            rawHz = std::min(round(atof(entry.c_str()) * 1e6), 999999999999.0);
            targetHz = clampHz(rawHz, ctx);
            belowRange = ctx.limited && rawHz < (double)rangeLo;
            aboveRange = ctx.limited && rawHz > (double)rangeHi;
        }
        const bool outOfRange = belowRange || aboveRange;

        // Entered value in MHz; before any key, the current frequency dimmed.
        ImGui::PushFont(style::bigFont);
        if (entry.empty()) {
            char cur[32];
            snprintf(cur, sizeof(cur), "%.6f", (double)ctx.frequency / 1e6);
            char* end = cur + strlen(cur) - 1;
            while (*end == '0') { *end-- = 0; }
            if (*end == '.') { *end = 0; }
            ImGui::TextDisabled("%s", cur);
        }
        else {
            if (outOfRange) { ImGui::PushStyleColor(ImGuiCol_Text, errorCol); }
            ImGui::TextUnformatted(entry.c_str());
            if (outOfRange) { ImGui::PopStyleColor(); }
        }
        ImGui::PopFont();

        if (entry.empty()) {
            ImGui::TextDisabled("Enter frequency in MHz");
        }
        else {
            if (outOfRange) { ImGui::PushStyleColor(ImGuiCol_Text, errorCol); }
            if (ctx.limited && targetHz != (uint64_t)rawHz) {
                ImGui::Text("= %s Hz -> %s Hz", groupHz((uint64_t)rawHz).c_str(), groupHz(targetHz).c_str());
            }
            else {
                ImGui::Text("= %s Hz", groupHz(targetHz).c_str());
            }
            if (outOfRange) { ImGui::PopStyleColor(); }
            if (belowRange) {
                ImGui::TextColored(errorCol, "Below source range: minimum is %s Hz", groupHz(rangeLo).c_str());
            }
            else if (aboveRange) {
                ImGui::TextColored(errorCol, "Above source range: maximum is %s Hz", groupHz(rangeHi).c_str());
            }
        }
        if (ctx.limited) {
            ImGui::TextDisabled("Range: %s - %s Hz", groupHz(rangeLo).c_str(), groupHz(rangeHi).c_str());
        }
        ImGui::Spacing();

        // 4x4 grid, placed explicitly since the double-height ENT would otherwise
        // stretch its row: digit block with the backspace at its bottom-right
        // (Android PIN-pad convention), function column CE / Cancel / tall ENT.
        ImVec2 sp = ImGui::GetStyle().ItemSpacing;
        ImVec2 origin = ImGui::GetCursorPos();
        auto cellPos = [&](int r, int c) {
            return ImVec2(origin.x + c * (m.keySize.x + sp.x), origin.y + r * (m.keySize.y + sp.y));
        };

        const char* dig[4][3] = {
            { "7", "8", "9" },
            { "4", "5", "6" },
            { "1", "2", "3" },
            { ".", "0", NULL } // NULL = backspace, drawn below
        };
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 3; c++) {
                if (!dig[r][c]) { continue; }
                ImGui::SetCursorPos(cellPos(r, c));
                if (ImGui::Button(dig[r][c], m.keySize)) { key(dig[r][c][0], ctx); }
            }
        }

        // Backspace key: undoes the last keypress. Roboto-Medium ships no arrow or
        // erase glyph, so the icon is drawn by hand; GetColorU32 picks up the
        // disabled-state alpha automatically.
        ImGui::SetCursorPos(cellPos(3, 2));
        ImGui::BeginDisabled(entry.empty());
        bool bksp = ImGui::Button("##sdrpp_finp_bksp", m.keySize);
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 bmin = ImGui::GetItemRectMin();
            ImVec2 bmax = ImGui::GetItemRectMax();
            ImVec2 ctr((bmin.x + bmax.x) / 2.0f, (bmin.y + bmax.y) / 2.0f);
            float iw = style::dp(11.0f);    // icon half-width
            float ih = style::dp(7.0f);     // icon half-height
            float notch = style::dp(7.0f);  // depth of the pointed tip
            float th = style::dp(1.5f);
            ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
            ImVec2 pts[5] = {
                ImVec2(ctr.x - iw, ctr.y),
                ImVec2(ctr.x - iw + notch, ctr.y - ih),
                ImVec2(ctr.x + iw, ctr.y - ih),
                ImVec2(ctr.x + iw, ctr.y + ih),
                ImVec2(ctr.x - iw + notch, ctr.y + ih)
            };
            dl->AddPolyline(pts, 5, col, ImDrawFlags_Closed, th);
            float bx = ctr.x + notch / 2.0f; // center of the icon body
            float xr = style::dp(3.0f);
            dl->AddLine(ImVec2(bx - xr, ctr.y - xr), ImVec2(bx + xr, ctr.y + xr), col, th);
            dl->AddLine(ImVec2(bx - xr, ctr.y + xr), ImVec2(bx + xr, ctr.y - xr), col, th);
        }
        ImGui::EndDisabled();
        if (bksp) { entry.pop_back(); }

        ImGui::SetCursorPos(cellPos(0, 3));
        if (ImGui::Button("CE", ImVec2(m.funcWidth, m.keySize.y))) { entry.clear(); }
        ImGui::SetCursorPos(cellPos(1, 3));
        if (ImGui::Button("Cancel##sdrpp_finp", ImVec2(m.funcWidth, m.keySize.y))) { out.close = true; }
        ImGui::SetCursorPos(cellPos(2, 3));
        if (ImGui::Button("ENT##sdrpp_finp", ImVec2(m.funcWidth, 2.0f * m.keySize.y + sp.y))) {
            commit(ctx, out);
            out.close = true;
        }

        // Hardware keyboard: digits, '.', Backspace, Enter (Escape is handled
        // page-independently by Dialog).
        for (int j = 0; j < kio.InputQueueCharacters.Size; j++) {
            char c = (char)kio.InputQueueCharacters[j];
            if ((c >= '0' && c <= '9') || c == '.') { key(c, ctx); }
            else if (c == ',') { key('.', ctx); }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !entry.empty()) { entry.pop_back(); }
        if (PopupDialog::confirmKeyPressed()) {
            commit(ctx, out);
            out.close = true;
        }

        return out;
    }

}
