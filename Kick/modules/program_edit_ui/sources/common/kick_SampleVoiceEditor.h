// Shared, state-backed filter/envelope/LFO controls for drum pads and key zones.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
namespace kick_ui
{
    class SampleVoiceEditor : public ui::KickGrid, private juce::ValueTree::Listener, private juce::AsyncUpdater
    {
    public:
        SampleVoiceEditor(std::vector<int>& Selection, std::function<mt::MTCommonProgram*()> Program)
            : ui::KickGrid(5, 8), selection(Selection), program(std::move(Program))
        {
            setRowTitles({"FILTER", "AMPLITUDE", "FILTER ENV", "PITCH ENV", "LFO"});
            std::vector<juce::String> filters{"Off"};
            for(auto name : magic_enum::enum_names<mt::FilterType>()) filters.emplace_back(name.data(), (int)name.size());
            filterType.setListOptions(filters); filterType.setComponentID("sample-filter-type");
            cutoff.setRange(20, 20000, 1); resonance.setRange(0, 97, 1);
            cutoff.setComponentID("sample-filter-cutoff"); resonance.setComponentID("sample-filter-resonance");
            addComponent(filterType, 0, 0, 2); addComponent(cutoff, 0, 2, 3); addComponent(resonance, 0, 5, 2);
            zoneSelector.setComponentID("sample-zone-select"); addComponent(zoneSelector, 0, 7);
            zoneSelector.onSelectionChanged = [this](int index, const juce::String&) {
                if(!updatingZones && !selection.empty() && juce::isPositiveAndBelow(index, (int)zoneKeys.size())) { selection[0] = zoneKeys[(size_t)index]; refresh(); }
            };
            filterType.onSelectionChanged = [this](int i, const juce::String&) { edit([&](auto& p, int n) { p.setFilterType(n, i - 1); }); };
            cutoff.onValueChanged = [this](double v) { edit([&](auto& p, int n) { p.setFilterCutoff(n, v); }); };
            resonance.onValueChanged = [this](double v) { edit([&](auto& p, int n) { p.setFilterResonance(n, v); }); };
            const std::array<juce::String, 3> names{"amp", "filter", "pitch"};
            for(int row = 0; row < 3; ++row)
            {
                const auto type = (mt::EnvelopeType)row;
                enabled[row].setButtonText("ON"); enabled[row].setClickingTogglesState(true);
                enabled[row].setComponentID("sample-env-" + names[row] + "-on");
                env[row].setListOptions({"AD", "ADSR", "AHD", "AHDS", "AHDSR"});
                env[row].setComponentID("sample-env-" + names[row] + "-type");
                addComponent(enabled[row], row + 1, 0); addComponent(env[row], row + 1, 1);
                enabled[row].onClick = [this, row, type] { edit([&](auto& p, int n) { p.setEnvelopeEnabled(n, type, enabled[row].getToggleState()); }); };
                env[row].onSelectionChanged = [this, type](int i, const juce::String&) { edit([&](auto& p, int n) { p.setEnvType(n, (mt::ADSRType)i, type); }); };
                const std::array<juce::String, 6> titles{"ATTACK", "HOLD", "DECAY", "SUSTAIN", "RELEASE", "DEPTH"};
                for(int parameter = 0; parameter < 6; ++parameter)
                {
                    auto& control = values[row][parameter]; control.setTitle(titles[parameter]); control.setRange(0, 127, 1);
                    control.setComponentID("sample-env-" + names[row] + "-" + titles[parameter].toLowerCase());
                    addComponent(control, row + 1, parameter + 2);
                    control.onValueChanged = [this, type, parameter](double v) { edit([&](auto& p, int n) {
                        switch(parameter) {
                            case 0: p.setEnvelopeAttack(n, type, (int)v); break;
                            case 1: p.setEnvelopeHold(n, type, (int)v); break;
                            case 2: p.setEnvelopeDecay(n, type, (int)v); break;
                            case 3: p.setEnvelopeSustain(n, type, (int)v); break;
                            case 4: p.setEnvelopeRelease(n, type, (int)v); break;
                            case 5: p.setEnvelopeDepth(n, type, (int)v); break;
                        }
                    }); };
                }
            }
            lfoOn.setButtonText("ON"); lfoOn.setClickingTogglesState(true); lfoOn.setComponentID("sample-lfo-on");
            lfoType.setListOptions({"Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Noise", "Sample Hold"});
            rate.setRange(0.1, 200, 0.1); rate.setComponentID("sample-lfo-rate");
            addComponent(lfoOn, 4, 0); addComponent(lfoType, 4, 1); addComponent(rate, 4, 2, 2);
            lfoOn.onClick = [this] { edit([&](auto& p, int n) { p.setLFOEnabled(n, lfoOn.getToggleState()); }); };
            lfoType.onSelectionChanged = [this](int i, const juce::String&) { edit([&](auto& p, int n) { p.setLFOType(n, i); }); };
            rate.onValueChanged = [this](double v) { edit([&](auto& p, int n) { p.setLFOFrequency(n, v); }); };
            const std::array<juce::String, 4> destinations{"PITCH", "FILTER", "AMP", "PAN"};
            for(int i = 0; i < 4; ++i)
            {
                lfoDepth[i].setTitle(destinations[i]); lfoDepth[i].setRange(i == 3 ? -1 : 0, i == 3 ? 1 : 127, i == 3 ? 0.01 : 1);
                lfoDepth[i].setComponentID("sample-lfo-" + destinations[i].toLowerCase()); addComponent(lfoDepth[i], 4, i + 4);
                lfoDepth[i].onValueChanged = [this, i](double v) { edit([&](auto& p, int n) {
                    switch(i) { case 0: p.setLFOPitch(n, (int)v); break; case 1: p.setLFOFilter(n, (int)v); break; case 2: p.setLFOLevel(n, (int)v); break; case 3: p.setLFOPan(n, v); break; }
                }); };
            }
        }
        ~SampleVoiceEditor() override { cancelPendingUpdate(); observed.removeListener(this); }

        void refresh()
        {
            observed.removeListener(this); observed = {};
            auto* p = validProgram();
            if(p && !selection.empty())
            {
                juce::ScopedValueSetter<bool> guard(updatingZones, true);
                zoneKeys = p->getSampleKeys(); std::vector<juce::String> options;
                for(auto key : zoneKeys) options.push_back(juce::String(key));
                zoneSelector.setListOptions(options);
                if(!p->getCommonData(selection[0]) && !zoneKeys.empty()) selection[0] = zoneKeys.front();
                auto it = std::find(zoneKeys.begin(), zoneKeys.end(), selection[0]);
                zoneSelector.setSelected(it == zoneKeys.end() ? -1 : (int)(it - zoneKeys.begin()), juce::dontSendNotification);
            }
            const bool valid = p && !selection.empty() && p->getCommonData(selection[0]); setEnabled(valid);
            if(!valid) return;
            const int n = selection[0]; observed = p->getNoteVT(n); observed.addListener(this);
            filterType.setSelected(p->getFilterType(n) + 1, juce::dontSendNotification);
            cutoff.setValue(p->getFilterCutoff(n), juce::dontSendNotification); resonance.setValue(p->getFilterResonance(n), juce::dontSendNotification);
            for(int row = 0; row < 3; ++row)
            {
                const auto type = (mt::EnvelopeType)row;
                const auto shape = p->getEnvType(n, type);
                enabled[row].setToggleState(p->getEnvelopeEnabled(n, type), juce::dontSendNotification);
                env[row].setSelected(shape, juce::dontSendNotification);
                const std::array<int, 6> current{p->getEnvelopeAttack(n, type), p->getEnvelopeHold(n, type), p->getEnvelopeDecay(n, type), p->getEnvelopeSustain(n, type), p->getEnvelopeRelease(n, type), p->getEnvelopeDepth(n, type)};
                for(int i = 0; i < 6; ++i) values[row][i].setValue(current[i], juce::dontSendNotification);
                values[row][1].setEnabled(shape >= mt::Type_AHD);
                values[row][3].setEnabled(shape == mt::Type_ADSR || shape == mt::Type_AHDS || shape == mt::Type_AHDSR);
                values[row][4].setEnabled(shape == mt::Type_ADSR || shape == mt::Type_AHDSR);
            }
            lfoOn.setToggleState(p->getLFOEnabled(n), juce::dontSendNotification);
            lfoType.setSelected(p->getLFOType(n), juce::dontSendNotification); rate.setValue(p->getLFOFrequency(n), juce::dontSendNotification);
            const std::array<double, 4> modulation{(double)p->getLFOPitch(n), (double)p->getLFOFilter(n), (double)p->getLFOLevel(n), p->getLFOPan(n)};
            for(int i = 0; i < 4; ++i) lfoDepth[i].setValue(modulation[i], juce::dontSendNotification);
        }
        void handleCommand(CPktMgr::HWMsg_t& message)
        {
            if(contracts::DclContractsHelper::isPadButtonMessage(message) && !selection.empty() && contracts::DclContractsHelper::getPadState(message) == contracts::DeviceControlContracts::BTN_PRESSED)
            {
                int note = contracts::DclContractsHelper::getPadNumber(message) + mt::getAudioInterface().getBankOffset();
                if(auto* keys = dynamic_cast<mt::MTKeygroupProgram*>(validProgram())) note = keys->getZoneKeyForNote(note);
                if(note >= 0) { selection[0] = note; refresh(); }
            }
        }
    private:
        template<typename Action> void edit(Action&& Apply)
        {
            if(auto* p = validProgram()) for(auto n : selection) if(p->getCommonData(n)) Apply(*p, n);
        }
        mt::MTCommonProgram* validProgram()
        {
            auto& audio = mt::getAudioInterface(); if(!audio.isProjectLoaded()) return nullptr;
            auto* candidate = program();
            for(int i = 0; candidate && i < audio.getNumPrograms(); ++i) if(audio.getProgram(i) == candidate) return candidate;
            return nullptr;
        }
        void visibilityChanged() override { if(isVisible()) refresh(); }
        void handleAsyncUpdate() override { refresh(); }
        void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override { triggerAsyncUpdate(); }
        std::vector<int>& selection;
        std::function<mt::MTCommonProgram*()> program;
        juce::ValueTree observed;
        ui::KickListBox filterType{"TYPE"}; ui::KickRotary cutoff{"CUTOFF"}, resonance{"RESONANCE"};
        ui::KickListBox zoneSelector{"ZONE"}; std::vector<int> zoneKeys; bool updatingZones{false};
        std::array<ui::KickButton, 3> enabled;
        std::array<ui::KickListBox, 3> env;
        std::array<std::array<ui::KickRotary, 6>, 3> values;
        ui::KickButton lfoOn{"LFO"}; ui::KickListBox lfoType{"WAVE"}; ui::KickRotary rate{"RATE"};
        std::array<ui::KickRotary, 4> lfoDepth;
    };
}
