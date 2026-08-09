#include <gui/widgets/freq_input.h>
#include <gui/widgets/popup_dialog.h>
#include <gui/style.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace freq_input {

    // Format a frequency in Hz with '.' group separators, matching the widget.
    static std::string groupHz(uint64_t frequencyHz) {
        std::string s = std::to_string(frequencyHz);
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

    static uint64_t clampHz(double frequencyHz, const Context& ctx) {
        if (!std::isfinite(frequencyHz) || frequencyHz < 0.0) {
            frequencyHz = 0.0;
        }
        frequencyHz = std::min(frequencyHz, 999999999999.0);

        if (ctx.limited) {
            frequencyHz = std::clamp(
                frequencyHz,
                (double)ctx.rangeLo(),
                (double)ctx.rangeHi());
        }

        return (uint64_t)frequencyHz;
    }

    // Height to reserve for the readout: every line it is able to show in any
    // state, measured wrapped to `width`. Laying the readout out line by line
    // instead made the key grid slide down by up to a full key height as the
    // echo, the out-of-range warning and the source range came and went --
    // under the finger, mid-entry, because the window is auto-sized and pinned
    // by its top-left corner.
    float Keypad::readoutHeight(const Context& ctx, float width) {
        const float sp = ImGui::GetStyle().ItemSpacing.y;
        const auto lineH = [&](const std::string& text) {
            return ImGui::CalcTextSize(text.c_str(), NULL, false, width).y;
        };

        float h = style::bigFont->FontSize; // the entered value
        int lines = 1;

        // The echo, at its widest: both the raw and the clamped value.
        h += ctx.limited
            ? lineH("= " + groupHz(ctx.rangeHi()) + " Hz -> " + groupHz(ctx.rangeHi()) + " Hz")
            : lineH("= " + groupHz(999999999999ull) + " Hz");
        lines++;

        if (ctx.limited) {
            h += std::max(
                lineH("Below source range: minimum is " + groupHz(ctx.rangeLo()) + " Hz"),
                lineH("Above source range: maximum is " + groupHz(ctx.rangeHi()) + " Hz"));
            h += lineH("Range: " + groupHz(ctx.rangeLo()) + " - " + groupHz(ctx.rangeHi()) + " Hz");
            lines += 2;
        }
        return h + (float)(lines - 1) * sp;
    }

    // The entered value, left-aligned in bigFont, shrunk to fit `width` and
    // occupying exactly one bigFont line whatever it says. ImGui::Text would
    // either widen the dialog or, with a wrap position set, break the number at
    // an arbitrary digit -- bigFont's glyph range is '.' through '9', so it has
    // no space to break at.
    static void drawValue(const char* text, float width, ImU32 col) {
        const float lineH = style::bigFont->FontSize;
        float size = lineH;
        ImVec2 ts = style::bigFont->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
        if (ts.x > width && ts.x > 0.0f) {
            size *= width / ts.x;
            ts = style::bigFont->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
        }
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddText(
            style::bigFont, size, ImVec2(pos.x, pos.y + (lineH - ts.y) * 0.5f), col, text);
        ImGui::Dummy(ImVec2(width, lineH));
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

        // Readout block: beside the keys or above them, per Metrics, which
        // weighs the column it would need against the height stacking would
        // cost. Its height is reserved for the worst case and its text wraps,
        // so neither what is typed nor how long the source's range prints can
        // move the keys or widen the dialog.
        const ImVec2 sp = ImGui::GetStyle().ItemSpacing;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::BeginChild("##sdrpp_freq_input_readout",
                          ImVec2(m.readoutWidth, m.readoutHeight),
                          false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PushTextWrapPos(m.readoutWidth);

        // Entered value in MHz; before any key, the current frequency dimmed.
        if (entry.empty()) {
            char cur[32];
            snprintf(cur, sizeof(cur), "%.6f", (double)ctx.frequency / 1e6);
            char* end = cur + strlen(cur) - 1;
            while (*end == '0') { *end-- = 0; }
            if (*end == '.') { *end = 0; }
            drawValue(cur, m.readoutWidth, ImGui::GetColorU32(ImGuiCol_TextDisabled));
        }
        else {
            drawValue(entry.c_str(), m.readoutWidth,
                      ImGui::GetColorU32(outOfRange ? errorCol : ImGui::GetStyleColorVec4(ImGuiCol_Text)));
        }

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

        ImGui::PopTextWrapPos();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        if (m.wideKeypad) { ImGui::SameLine(); }
        else { ImGui::Spacing(); }

        // 4x4 grid, placed explicitly since the double-height ENT would otherwise
        // stretch its row: digit block with the backspace at its bottom-right
        // (Android PIN-pad convention), function column CE / Cancel / tall ENT.
        // Centred when the page is wider than the block, which the band grid
        // can make it.
        ImVec2 origin = ImGui::GetCursorPos();
        if (!m.wideKeypad) {
            origin.x += std::max(0.0f, (m.totalWidth - m.keypadWidth) * 0.5f);
        }
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
        bool bksp = ImGui::Button("##sdrpp_freq_input_bksp", m.keySize);
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
        if (ImGui::Button("Cancel##sdrpp_freq_input", ImVec2(m.funcWidth, m.keySize.y))) { out.close = true; }
        ImGui::SetCursorPos(cellPos(2, 3));
        if (ImGui::Button("ENT##sdrpp_freq_input", ImVec2(m.funcWidth, 2.0f * m.keySize.y + sp.y))) {
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
