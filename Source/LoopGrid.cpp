#include "LoopGrid.h"
#include "Looper.h"
#include "LooperRecorder.h"
#include "Amplifier.h"
#include "ModularSynth.h"
#include "SynthGlobals.h"
#include "FillSaveDropdown.h"
#include "OpenFrameworksPort.h"
#include "ofxJSONElement.h"
#include "PatchCableSource.h"
#include "EffectChain.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
   const char* kSlotKey[4] = { "rhythm_a", "rhythm_b", "lead_a", "lead_b" };
   const char* kRecorderKey[2] = { "rhythm_recorder", "lead_recorder" };
   const char* kAmpKey[2] = { "rhythm_amp", "lead_amp" };
   const char* kFxKey[2] = { "rhythm_fx", "lead_fx" };
   const char* kMonitorKey[2] = { "rhythm_monitor", "lead_monitor" };
   const char* kStateName[6] = { "empty", "armed", "recording", "queued", "playing", "stopped" };
   const char* kActionName[PerformanceBindings::kActions] = {
      "Rhythm A", "Rhythm B", "Lead A", "Lead B", "Stop all", "Panic", "120 BPM",
      "Rhythm mute", "Lead mute", "Rhythm solo", "Lead solo", "Rhythm FX", "Lead FX",
      "Rhythm monitor", "Lead monitor", "Rhythm gain +", "Lead gain +"
   };
   uint64_t PackCommand(int action, int target) { return (uint64_t(uint32_t(target)) << 8) | uint64_t(action); }
}

LoopGrid::LoopGrid() = default;

void LoopGrid::GetModuleDimensions(float& width, float& height)
{
   width = 650;
   height = mMapMode == MapMode::Off ? 340 : 520;
}

std::atomic<LoopGrid*> LoopGrid::sMidiLearnGrid{ nullptr };
std::atomic<uint64_t> LoopGrid::sMidiLearnMessage{ 0 };
std::atomic<MidiController*> LoopGrid::sMidiLearnController{ nullptr };
std::mutex LoopGrid::sMidiLearnMutex;

LoopGrid::~LoopGrid()
{
   CancelMapping();
   TheTransport->RemoveListener(this);
}

void LoopGrid::CreateUIControls()
{
   IDrawableModule::CreateUIControls();
   mLengthSelector = new DropdownList(this, "capture bars", 198, 9, &mCaptureBars);
   mLengthSelector->AddLabel("4 bars", 4);
   mLengthSelector->AddLabel("8 bars", 8);
   mStopAllButton = new ClickButton(this, "Stop all", 340, 8);
   mStopAllButton->SetDimensions(96, 27);
   mPanicButton = new ClickButton(this, "Panic", 445, 8);
   mPanicButton->SetDimensions(76, 27);
   mTempo120Button = new ClickButton(this, "Set 120 BPM", 530, 8);
   mTempo120Button->SetOverrideDisplayName("120 BPM");
   mTempo120Button->SetDimensions(107, 27);
   mKeyboardCheckbox = new Checkbox(this, "Performance keys", 12, 37, &mKeyboardEnabled);
   mMapKeyButton = new ClickButton(this, "Map key", 180, 37);
   mMapKeyButton->SetDimensions(68, 19);
   mMapMidiButton = new ClickButton(this, "Map MIDI", 255, 37);
   mMapMidiButton->SetDimensions(75, 19);
   for (int track = 0; track < 2; ++track)
   {
      int y = 58 + track * 125;
      mMuteButtons[track] = new ClickButton(this, track == 0 ? "Rhythm mute" : "Lead mute", 12, y + 64);
      mMuteButtons[track]->SetOverrideDisplayName("mute");
      mMuteButtons[track]->SetDimensions(48, 24);
      mSoloButtons[track] = new ClickButton(this, track == 0 ? "Rhythm solo" : "Lead solo", 66, y + 64);
      mSoloButtons[track]->SetOverrideDisplayName("solo");
      mSoloButtons[track]->SetDimensions(43, 24);
      mFxButtons[track] = new ClickButton(this, track == 0 ? "Rhythm FX" : "Lead FX", 115, y + 64);
      mFxButtons[track]->SetOverrideDisplayName("FX");
      mFxButtons[track]->SetDimensions(41, 24);
      mMonitorButtons[track] = new ClickButton(this, track == 0 ? "Rhythm monitor" : "Lead monitor", 162, y + 64);
      mMonitorButtons[track]->SetOverrideDisplayName("mon");
      mMonitorButtons[track]->SetDimensions(48, 24);
      mGainSliders[track] = new FloatSlider(this, track == 0 ? "Rhythm gain" : "Lead gain",
                                            12, y + 37, 198, 19, &mGain[track], 0, 2);
      for (int col = 0; col < 2; ++col)
      {
         int i = track * 2 + col;
         const char* name = track == 0 ? (col == 0 ? "Rhythm A" : "Rhythm B")
                                       : (col == 0 ? "Lead A" : "Lead B");
         mSlots[i].button = new ClickButton(this, name, 240 + col * 202, y);
         mSlots[i].button->SetOverrideDisplayName("");
         mSlots[i].button->SetDimensions(190, 100);
      }
   }
}

void LoopGrid::Init()
{
   IDrawableModule::Init();
   BindRecorders();
   TheTransport->AddListener(this, kInterval_1n, OffsetInfo(0, true), false);
}

void LoopGrid::BindRecorders()
{
   // LooperRecorder owns a normal audio output at 0 and eight special looper
   // cables at 1..8. The stock PostRepatch hook sets Looper::mRecorder.
   // Run only during setup/load or a settings edit, never on the audio thread.
   for (int track = 0; track < 2; ++track)
   {
      auto* recorder = mRecorders[track];
      if (!recorder || recorder->IsDeleted())
         continue;
      for (int col = 0; col < 2; ++col)
      {
         auto* cable = recorder->GetPatchCableSource(1 + col);
         auto* looper = mSlots[track * 2 + col].looper;
         if (!cable)
            continue;
         if (looper && !looper->IsDeleted())
         {
            if (cable->GetTarget() != looper)
               cable->SetTarget(looper);
         }
         else if (cable->GetTarget())
            cable->Clear();
      }
   }
}

bool LoopGrid::Ready(int index) const
{
   int track = index / 2;
   return mSlots[index].looper && !mSlots[index].looper->IsDeleted() &&
          mRecorders[track] && !mRecorders[track]->IsDeleted() &&
          mTrackAmps[track] && !mTrackAmps[track]->IsDeleted() &&
          mRecorders[track]->GetPatchCableSource(1 + index % 2) &&
          mRecorders[track]->GetPatchCableSource(1 + index % 2)->GetTarget() == mSlots[index].looper &&
          mSlots[index].looper->GetRecorder() == mRecorders[track];
}

void LoopGrid::Poll()
{
   // Key learning must yield to typing. MIDI learning can stay armed while a
   // native keyboard MIDI controller has focus and emits its incoming notes.
   if (mMapMode == MapMode::Key && IKeyboardFocusListener::GetActiveKeyboardFocus())
      CancelMapping();
   CompleteMidiLearn();
   for (int i = 0; i < 4; ++i)
   {
      if (mSlots[i].looper && mSlots[i].looper->IsDeleted())
      {
         mSlots[i].looper = nullptr;
         mSlots[i].command.store(0);
      }
      else if (mSlots[i].looper && mSlots[i].state.load() == Stopped &&
               !mSlots[i].looper->IsCommitting())
         mSlots[i].looper->SetHoldMuteAfterCommit(false);
   }
   for (int track = 0; track < 2; ++track)
   {
      if (mRecorders[track] && mRecorders[track]->IsDeleted())
         mRecorders[track] = nullptr;
      if (mTrackAmps[track] && mTrackAmps[track]->IsDeleted())
         mTrackAmps[track] = nullptr;
      if (mTrackFx[track] && mTrackFx[track]->IsDeleted())
         mTrackFx[track] = nullptr;
      if (mTrackMonitors[track] && mTrackMonitors[track]->IsDeleted())
         mTrackMonitors[track] = nullptr;
   }
}

void LoopGrid::Queue(int index, Action action, int target, State state)
{
   auto& slot = mSlots[index];
   slot.state.store(state);
   slot.command.store(PackCommand(action, target));
}

void LoopGrid::ApplyTrackGains(double time)
{
   for (int track = 0; track < 2; ++track)
      if (mTrackAmps[track] && !mTrackAmps[track]->IsDeleted())
         mTrackAmps[track]->GetGainSlider()->SetValue(
            loopgrid::EffectiveTrackGain(mGain[track], mMuted[track], mSoloTrack, track), time);
}

void LoopGrid::ShowStatus(const char* message, double time)
{
   mStatus = message;
   mStatusUntil = time + 3000;
}

void LoopGrid::StopAll(double time)
{
   const int nextBar = (int)std::floor(TheTransport->GetMeasureTime(time)) + 1;
   for (int i = 0; i < 4; ++i)
   {
      auto& slot = mSlots[i];
      const State state = (State)slot.state.load();
      const Action pending = (Action)(slot.command.load() & 0xff);
      if (state == Queued && pending == StopPlay)
         continue;
      const auto next = loopgrid::StopAll(state, pending, slot.occupied.load(), nextBar);
      if (next.releaseHold && slot.looper)
         slot.looper->SetHoldMuteAfterCommit(false);
      if (next.action == NoAction)
      {
         slot.command.store(0);
         slot.state.store(next.state);
      }
      else if (next.action != pending || next.state != state)
         Queue(i, next.action, next.target, next.state);
   }
   ShowStatus("stop queued for next bar", time);
}

void LoopGrid::Panic(double time)
{
   for (int track = 0; track < 2; ++track)
      mMuted[track] = true;
   ApplyTrackGains(time);
   for (auto& slot : mSlots)
   {
      slot.command.store(0);
      slot.state.store(loopgrid::OnStartup(slot.occupied.load()));
      if (slot.looper && !slot.looper->IsDeleted())
      {
         slot.looper->SetMute(time, true);
         if (!slot.looper->IsCommitting())
            slot.looper->SetHoldMuteAfterCommit(false);
      }
   }
   ShowStatus("panic: both tracks muted", time);
}

void LoopGrid::ButtonClicked(ClickButton* button, double time)
{
   if (button == mMapKeyButton || button == mMapMidiButton)
   {
      const MapMode wanted = button == mMapKeyButton ? MapMode::Key : MapMode::Midi;
      if (mMapMode == wanted)
         CancelMapping();
      else
      {
         CancelMapping();
         mMapMode = wanted;
         ShowStatus(wanted == MapMode::Key ? "map key: click a panel control" : "map MIDI: click a panel control", time);
      }
      return;
   }
   if (mMapMode != MapMode::Off)
   {
      const int action = ActionForControl(button);
      if (action >= 0)
      {
         SelectMapAction(action, time);
         return;
      }
   }
   if (button == mStopAllButton)
   {
      StopAll(time);
      return;
   }
   if (button == mPanicButton)
   {
      Panic(time);
      return;
   }
   if (button == mTempo120Button)
   {
      for (const auto& slot : mSlots)
         if (slot.state.load() != Empty && slot.state.load() != Stopped)
         {
            ShowStatus("stop clips before tempo change", time);
            return;
         }
      TheTransport->SetTempo(120);
      ShowStatus("transport set to 120 BPM", time);
      return;
   }
   for (int track = 0; track < 2; ++track)
   {
      if (button == mMuteButtons[track])
      {
         mMuted[track] = !mMuted[track];
         ApplyTrackGains(time);
         return;
      }
      if (button == mFxButtons[track])
      {
         auto* fx = mTrackFx[track];
         if (!fx || fx->IsDeleted())
            ShowStatus("map a track FX chain first", time);
         else
            fx->SetEnabled(!fx->IsEnabled());
         return;
      }
      if (button == mMonitorButtons[track])
      {
         auto* monitor = mTrackMonitors[track];
         if (!monitor || monitor->IsDeleted() || monitor == mTrackAmps[track])
         {
            ShowStatus("map a separate monitor gain", time);
            return;
         }
         if (!mMonitorOff[track])
         {
            mMonitorRestoreGain[track] = monitor->GetGainSlider()->GetValue();
            mMonitorOff[track] = true;
            monitor->GetGainSlider()->SetValue(0, time);
         }
         else
         {
            mMonitorOff[track] = false;
            monitor->GetGainSlider()->SetValue(mMonitorRestoreGain[track], time);
         }
         return;
      }
      if (button == mSoloButtons[track])
      {
         mSoloTrack = mSoloTrack == track ? -1 : track;
         ApplyTrackGains(time);
         return;
      }
   }

   for (int i = 0; i < 4; ++i)
   {
      auto& slot = mSlots[i];
      if (button != slot.button)
         continue;
      if (!Ready(i))
      {
         ShowStatus("map looper, recorder and gain", time);
         return;
      }
      const int nextBar = (int)std::floor(TheTransport->GetMeasureTime(time)) + 1;
      const State state = (State)slot.state.load();
      const Action pending = (Action)(slot.command.load() & 0xff);
      const int sibling = i ^ 1;
      const State other = (State)mSlots[sibling].state.load();
      const bool captureFits = mCaptureBars * TheTransport->MsPerBar() < 59000;
      if (state == Empty && !captureFits)
      {
         ShowStatus("capture exceeds 59 seconds", time);
         return;
      }
      if (state == Empty && loopgrid::IsCapturing(other))
      {
         ShowStatus("track is already capturing", time);
         return;
      }
      const auto next = loopgrid::Click(state, pending, slot.occupied.load(),
                                        loopgrid::IsCapturing(other), nextBar, captureFits);
      if (next.state == state && next.action == pending)
      {
         if (state == Recording)
            ShowStatus("capture in progress", time);
         return;
      }
      if (next.clear)
      {
         slot.bars.store(mCaptureBars);
         slot.looper->SetMute(time, true);
         slot.looper->SetHoldMuteAfterCommit(true);
      }
      if (state == Armed && next.state == Empty)
         slot.looper->SetHoldMuteAfterCommit(false);
      if (state == Queued && next.action == NoAction &&
          pending == StartPlay && !slot.looper->IsCommitting())
         slot.looper->SetHoldMuteAfterCommit(false);
      if (next.action == NoAction)
      {
         slot.command.store(0);
         slot.state.store(next.state);
      }
      else
         Queue(i, next.action, next.target, next.state);
      if (next.action == StartPlay && other == Queued &&
          (mSlots[sibling].command.load() & 0xff) == StartPlay)
      {
         mSlots[sibling].command.store(0);
         mSlots[sibling].state.store(Stopped);
         if (mSlots[sibling].looper && !mSlots[sibling].looper->IsCommitting())
            mSlots[sibling].looper->SetHoldMuteAfterCommit(false);
      }
      return;
   }
}

void LoopGrid::OnTimeEvent(double time)
{
   const int measure = TheTransport->GetMeasure(time);
   for (int i = 0; i < 4; ++i)
   {
      auto& slot = mSlots[i];
      uint64_t pending = slot.command.load();
      const Action action = (Action)(pending & 0xff);
      const int target = int(pending >> 8);
      if (action == NoAction || measure < target)
         continue;
      if (!slot.command.compare_exchange_strong(pending, 0))
         continue;
      if (!Ready(i))
      {
         if (action == AbortCapture && slot.looper)
            slot.looper->SetHoldMuteAfterCommit(false);
         slot.state.store(slot.occupied.load() ? Stopped : Empty);
         continue;
      }
      const State state = (State)slot.state.load();
      const auto next = loopgrid::OnBar(state, action, measure, slot.bars.load(), slot.looper->IsCommitting());
      if (next.commit)
      {
         auto* recorder = mRecorders[i / 2];
         recorder->SetNumBars(slot.bars.load());
         recorder->Commit(slot.looper, true); // replace the designated slot, preserving it if arming is cancelled
         slot.occupied.store(true);
      }
      if (next.displaceSibling)
      {
         const int sibling = i ^ 1;
         auto& otherSlot = mSlots[sibling];
         const State otherState = (State)otherSlot.state.load();
         if (otherState == Playing || otherState == Queued)
         {
            if (auto* other = otherSlot.looper)
               other->SetMute(time, true);
            otherSlot.command.store(0);
            otherSlot.state.store(loopgrid::OnDisplaced(otherState));
         }
      }
      if (next.launch)
      {
         slot.looper->SetMute(time, false);
         slot.looper->SetHoldMuteAfterCommit(false);
      }
      if (next.stop)
         slot.looper->SetMute(time, true);
      if (next.releaseHold)
         slot.looper->SetHoldMuteAfterCommit(false);
      slot.state.store(next.state);
      if (next.action != NoAction)
         slot.command.store(PackCommand(next.action, next.target));
   }
}

void LoopGrid::FloatSliderUpdated(FloatSlider* slider, float oldVal, double time)
{
   if (mMapMode != MapMode::Off && !mRestoringMapSlider)
   {
      const int action = ActionForControl(slider);
      if (action >= 0)
      {
         mRestoringMapSlider = true;
         slider->SetValue(oldVal, time);
         mRestoringMapSlider = false;
         SelectMapAction(action, time);
         return;
      }
   }
   for (int track = 0; track < 2; ++track)
      if (slider == mGainSliders[track])
         ApplyTrackGains(time);
}

void LoopGrid::DropdownUpdated(DropdownList* list, int oldVal, double time) {}

void LoopGrid::DrawModule()
{
   if (Minimized() || !IsVisible())
      return;
   ofPushStyle();
   ofSetColor(230);
   DrawTextNormal("LOOP GRID", 12, 24, 18);
   DrawTextNormal("tempo " + ofToString(TheTransport->GetTempo(), 0), 112, 25, 12);
   mLengthSelector->Draw();
   mStopAllButton->Draw();
   mPanicButton->Draw();
   mTempo120Button->Draw();
   mKeyboardCheckbox->Draw();
   mMapKeyButton->Draw();
   mMapMidiButton->Draw();
   for (int track = 0; track < 2; ++track)
   {
      const int y = 58 + track * 125;
      ofSetColor(220, 220, 230);
      DrawTextNormal(track == 0 ? "RHYTHM" : "LEAD", 12, y + 22, 20);
      mMuteButtons[track]->Draw();
      mSoloButtons[track]->Draw();
      mFxButtons[track]->Draw();
      mMonitorButtons[track]->Draw();
      mGainSliders[track]->Draw();
      ofSetColor(mMuted[track] ? ofColor(255, 105, 105) : ofColor(120, 220, 150));
      DrawTextNormal(mMuted[track] ? "MUTED" : "live", 12, y + 106, 11);
      if (mSoloTrack == track)
      {
         ofSetColor(255, 225, 90);
         DrawTextNormal("SOLO", 65, y + 106, 11);
      }
      const bool hasFx = mTrackFx[track] && !mTrackFx[track]->IsDeleted();
      ofSetColor(hasFx && mTrackFx[track]->IsEnabled() ? ofColor(120, 220, 150) : ofColor(160, 160, 160));
      DrawTextNormal(hasFx ? (mTrackFx[track]->IsEnabled() ? "FX on" : "FX off") : "no FX", 114, y + 106, 11);
      const bool hasMonitor = mTrackMonitors[track] && !mTrackMonitors[track]->IsDeleted() &&
                              mTrackMonitors[track] != mTrackAmps[track];
      ofSetColor(hasMonitor && !mMonitorOff[track] ? ofColor(120, 220, 150) : ofColor(160, 160, 160));
      DrawTextNormal(hasMonitor ? (mMonitorOff[track] ? "mon off" : "mon on") : "no mon", 161, y + 106, 11);
   }
   for (int i = 0; i < 4; ++i)
   {
      const State state = (State)mSlots[i].state.load();
      const int x = 240 + (i % 2) * 202;
      const int y = 58 + (i / 2) * 125;
      mSlots[i].button->Draw();
      ofColor stateColor;
      switch (state)
      {
         case Empty: stateColor = ofColor(65, 65, 65); break;
         case Armed: stateColor = ofColor(230, 150, 20); break;
         case Recording: stateColor = ofColor(220, 45, 35); break;
         case Queued: stateColor = ofColor(230, 210, 40); break;
         case Playing: stateColor = ofColor(55, 190, 85); break;
         case Stopped: stateColor = ofColor(70, 105, 135); break;
      }
      ofSetColor(stateColor.r, stateColor.g, stateColor.b, 155);
      ofRect(x + 3, y + 3, 184, 94);
      ofSetColor(255);
      DrawTextNormal(i % 2 == 0 ? "A" : "B", x + 12, y + 37, 29);
      DrawTextNormal(Ready(i) ? kStateName[state] : "unmapped", x + 12, y + 70, 14);
      if (state == Empty && Ready(i))
         DrawTextNormal("click to record", x + 12, y + 86, 10);
   }
   if (gTime < mStatusUntil)
   {
      ofSetColor(255, 210, 105);
      DrawTextNormal(mStatus, 12, 329, 12);
   }
   if (mMapMode != MapMode::Off)
   {
      for (int action = 0; action < PerformanceBindings::kActions; ++action)
      {
         auto* control = ControlForAction(action);
         if (!control)
            continue;
         ofRectangle rect = control->GetRect(K(local));
         ofNoFill();
         ofSetColor(action == mPendingMapAction ? ofColor(255, 220, 55) : ofColor(100, 210, 255));
         ofRect(rect);
         ofFill();
         const int boundKey = mBindings.KeyFor(action);
         const std::string badge = std::string(boundKey ? 1 : 0, char(boundKey)) +
                                   (mMidiLabels[action].empty() ? "" : " M");
         ofSetColor(255, 230, 130);
         DrawTextNormal(badge, rect.getMaxX() - (mMidiLabels[action].empty() ? 14 : 27),
                        rect.y + 12, 10);
      }
      ofSetColor(255, 225, 130);
      DrawTextNormal("Select a control above, then press a chord or send MIDI. Esc cancels.", 12, 348, 11);
      for (int action = 0; action < PerformanceBindings::kActions; ++action)
      {
         const int column = action / 9;
         const int row = action % 9;
         const int x = 12 + column * 320;
         const int y = 366 + row * 17;
         const bool selected = action == mPendingMapAction;
         ofSetColor(selected ? ofColor(255, 220, 55) : ofColor(235, 235, 235));
         DrawTextNormal(std::string(kActionName[action]) + "  " +
                        PerformanceBindings::Label(mBindings.KeyFor(action)), x, y, 10);
         ofSetColor(selected ? ofColor(255, 225, 145) : ofColor(165, 205, 235));
         DrawTextNormal(mMidiLabels[action].empty() ? "MIDI: --" : "MIDI: " + mMidiLabels[action],
                        x, y + 9, 8);
      }
   }
   ofPopStyle();
}

void LoopGrid::LoadLayout(const ofxJSONElement& moduleInfo)
{
   for (int i = 0; i < 4; ++i)
   {
      mModuleSaveData.LoadString(kSlotKey[i], moduleInfo, "", FillDropdown<Looper*>);
      mModuleSaveData.LoadBool(std::string(kSlotKey[i]) + "_occupied", moduleInfo, false);
   }
   for (int track = 0; track < 2; ++track)
   {
      mModuleSaveData.LoadString(kRecorderKey[track], moduleInfo, "", FillDropdown<LooperRecorder*>);
      mModuleSaveData.LoadString(kAmpKey[track], moduleInfo, "", FillDropdown<Amplifier*>);
      mModuleSaveData.LoadString(kFxKey[track], moduleInfo, "", FillDropdown<EffectChain*>);
      mModuleSaveData.LoadString(kMonitorKey[track], moduleInfo, "", FillDropdown<Amplifier*>);
      mModuleSaveData.LoadBool(std::string(kAmpKey[track]) + "_muted", moduleInfo, false);
      mModuleSaveData.LoadBool(std::string(kMonitorKey[track]) + "_off", moduleInfo, false);
      mModuleSaveData.LoadFloat(std::string(kMonitorKey[track]) + "_restore_gain", moduleInfo, 1.0f, 0.0f, 2.0f);
   }
   mModuleSaveData.LoadInt("solo_track", moduleInfo, -1, -1, 1);
   if (moduleInfo.isMember("performance_keys"))
   {
      auto keys = mBindings.ExportKeys();
      for (int action = 0; action < PerformanceBindings::kActions; ++action)
      {
         const std::string index = std::to_string(action);
         if (moduleInfo["performance_keys"].isMember(index))
            keys[action] = moduleInfo["performance_keys"][index].asInt();
      }
      mBindings.ImportKeys(keys); // invalid/reserved/duplicate saved map leaves defaults
   }
   if (moduleInfo.isMember("performance_midi_labels"))
      for (int action = 0; action < PerformanceBindings::kActions; ++action)
      {
         const std::string index = std::to_string(action);
         if (moduleInfo["performance_midi_labels"].isMember(index))
            mMidiLabels[action] = moduleInfo["performance_midi_labels"][index].asString();
      }
   if (moduleInfo.isMember("performance_midi_bindings"))
   {
      std::lock_guard<std::mutex> lock(sMidiLearnMutex);
      for (int action = 0; action < PerformanceBindings::kActions; ++action)
      {
         const std::string index = std::to_string(action);
         if (!moduleInfo["performance_midi_bindings"].isMember(index))
            continue;
         const auto& saved = moduleInfo["performance_midi_bindings"][index];
         MidiBinding binding;
         if (saved.isMember("controller")) binding.controllerPath = saved["controller"].asString();
         if (saved.isMember("type")) binding.type = saved["type"].asInt();
         if (saved.isMember("control")) binding.control = saved["control"].asInt();
         if (saved.isMember("channel")) binding.channel = saved["channel"].asInt();
         if (saved.isMember("page")) binding.page = saved["page"].asInt();
         if (binding.Valid() && binding.type <= kMidiMessage_Control &&
             binding.control <= 127 && binding.channel >= 0 && binding.channel <= 16 &&
             binding.page < MAX_MIDI_PAGES)
            mMidiBindings[action] = binding;
      }
   }
   SetUpFromSaveData();
}

void LoopGrid::SaveLayout(ofxJSONElement& moduleInfo)
{
   for (int i = 0; i < 4; ++i)
   {
      moduleInfo[kSlotKey[i]] = mSlots[i].looper ? mSlots[i].looper->Path() : "";
      moduleInfo[std::string(kSlotKey[i]) + "_occupied"] = mSlots[i].occupied.load();
   }
   for (int track = 0; track < 2; ++track)
   {
      moduleInfo[kRecorderKey[track]] = mRecorders[track] ? mRecorders[track]->Path() : "";
      moduleInfo[kAmpKey[track]] = mTrackAmps[track] ? mTrackAmps[track]->Path() : "";
      moduleInfo[kFxKey[track]] = mTrackFx[track] ? mTrackFx[track]->Path() : "";
      moduleInfo[kMonitorKey[track]] = mTrackMonitors[track] ? mTrackMonitors[track]->Path() : "";
      moduleInfo[std::string(kAmpKey[track]) + "_muted"] = mMuted[track];
      moduleInfo[std::string(kMonitorKey[track]) + "_off"] = mMonitorOff[track];
      moduleInfo[std::string(kMonitorKey[track]) + "_restore_gain"] = mMonitorRestoreGain[track];
   }
   moduleInfo["solo_track"] = mSoloTrack;
   for (int action = 0; action < PerformanceBindings::kActions; ++action)
   {
      const std::string index = std::to_string(action);
      moduleInfo["performance_keys"][index] = mBindings.KeyFor(action);
      if (!mMidiLabels[action].empty())
         moduleInfo["performance_midi_labels"][index] = mMidiLabels[action];
   }
   {
      std::lock_guard<std::mutex> lock(sMidiLearnMutex);
      for (int action = 0; action < PerformanceBindings::kActions; ++action)
      {
         const auto& binding = mMidiBindings[action];
         if (!binding.Valid())
            continue;
         const std::string index = std::to_string(action);
         auto& saved = moduleInfo["performance_midi_bindings"][index];
         saved["controller"] = binding.controllerPath;
         saved["type"] = binding.type;
         saved["control"] = binding.control;
         saved["channel"] = binding.channel;
         saved["page"] = binding.page;
      }
   }
}

void LoopGrid::SetUpFromSaveData()
{
   for (int i = 0; i < 4; ++i)
   {
      mSlots[i].looper = dynamic_cast<Looper*>(TheSynth->FindModule(mModuleSaveData.GetString(kSlotKey[i]), false));
      bool occupied = mModuleSaveData.GetBool(std::string(kSlotKey[i]) + "_occupied");
      mSlots[i].occupied.store(occupied);
      mSlots[i].state.store(loopgrid::OnStartup(occupied));
      mSlots[i].command.store(0);
      if (mSlots[i].looper)
         mSlots[i].looper->SetMute(gTime, true);
   }
   for (int track = 0; track < 2; ++track)
   {
      auto* nextRecorder = dynamic_cast<LooperRecorder*>(TheSynth->FindModule(mModuleSaveData.GetString(kRecorderKey[track]), false));
      if (mRecorders[track] && mRecorders[track] != nextRecorder && !mRecorders[track]->IsDeleted())
      {
         for (int col = 0; col < 2; ++col)
            if (auto* cable = mRecorders[track]->GetPatchCableSource(1 + col))
               cable->Clear();
      }
      mRecorders[track] = nextRecorder;
      mTrackAmps[track] = dynamic_cast<Amplifier*>(TheSynth->FindModule(mModuleSaveData.GetString(kAmpKey[track]), false));
      mTrackFx[track] = dynamic_cast<EffectChain*>(TheSynth->FindModule(mModuleSaveData.GetString(kFxKey[track]), false));
      mTrackMonitors[track] = dynamic_cast<Amplifier*>(TheSynth->FindModule(mModuleSaveData.GetString(kMonitorKey[track]), false));
      mMuted[track] = mModuleSaveData.GetBool(std::string(kAmpKey[track]) + "_muted");
      mMonitorOff[track] = mModuleSaveData.GetBool(std::string(kMonitorKey[track]) + "_off");
      mMonitorRestoreGain[track] = mModuleSaveData.GetFloat(std::string(kMonitorKey[track]) + "_restore_gain");
   }
   mSoloTrack = mModuleSaveData.GetInt("solo_track");
   mKeyboardEnabled = false; // explicit opt-in each session
   if (mSoloTrack < -1 || mSoloTrack > 1)
      mSoloTrack = -1;
   if (IsInitialized())
      BindRecorders();
   ApplyTrackGains(gTime);
   for (int track = 0; track < 2; ++track)
      if (mMonitorOff[track] && mTrackMonitors[track] && mTrackMonitors[track] != mTrackAmps[track])
         mTrackMonitors[track]->GetGainSlider()->SetValue(0, gTime);
}

void LoopGrid::PostLoadState()
{
   mKeyboardEnabled = false; // do not arm performance keys on reopen
   CancelMapping();
   BindRecorders();
   // Waveforms and UI controls are restored after layout. Saved clips start
   // stopped so session reopen never causes surprise playback or recording.
   for (auto& slot : mSlots)
   {
      if (slot.looper)
         slot.looper->SetMute(gTime, true);
      slot.command.store(0);
      slot.state.store(loopgrid::OnStartup(slot.occupied.load()));
   }
   ApplyTrackGains(gTime);
   for (int track = 0; track < 2; ++track)
      if (mMonitorOff[track] && mTrackMonitors[track] && mTrackMonitors[track] != mTrackAmps[track])
         mTrackMonitors[track]->GetGainSlider()->SetValue(0, gTime);
}

int LoopGrid::ActionForControl(IUIControl* control) const
{
   for (int i = 0; i < 4; ++i)
      if (control == mSlots[i].button)
         return i;
   if (control == mStopAllButton) return 4;
   if (control == mPanicButton) return 5;
   if (control == mTempo120Button) return 6;
   for (int track = 0; track < 2; ++track)
   {
      if (control == mMuteButtons[track]) return 7 + track;
      if (control == mSoloButtons[track]) return 9 + track;
      if (control == mFxButtons[track]) return 11 + track;
      if (control == mMonitorButtons[track]) return 13 + track;
      if (control == mGainSliders[track]) return 15 + track;
   }
   return -1;
}

IUIControl* LoopGrid::ControlForAction(int action) const
{
   if (action >= 0 && action < 4) return mSlots[action].button;
   if (action == 4) return mStopAllButton;
   if (action == 5) return mPanicButton;
   if (action == 6) return mTempo120Button;
   if (action >= 7 && action < 9) return mMuteButtons[action - 7];
   if (action >= 9 && action < 11) return mSoloButtons[action - 9];
   if (action >= 11 && action < 13) return mFxButtons[action - 11];
   if (action >= 13 && action < 15) return mMonitorButtons[action - 13];
   if (action >= 15 && action < 17) return mGainSliders[action - 15];
   return nullptr;
}

void LoopGrid::PerformAction(int action, double time)
{
   if (action >= 15 && action < 17)
   {
      const int track = action - 15;
      mGainSliders[track]->SetValue(std::min(2.0f, mGain[track] + 0.1f), time);
      return;
   }
   auto* control = dynamic_cast<ClickButton*>(ControlForAction(action));
   if (control)
      ButtonClicked(control, time);
}

bool LoopGrid::HandlePerformanceKey(int key, int modifiers, bool isRepeat)
{
   if (isRepeat)
      return false;
   if (mMapMode != MapMode::Off)
   {
      if (key == OF_KEY_ESC)
      {
         CancelMapping();
         ShowStatus("mapping cancelled", gTime);
         return true;
      }
      if (mMapMode == MapMode::Key && mPendingMapAction >= 0)
      {
         const auto result = mBindings.Assign(mPendingMapAction, key, modifiers);
         if (result == PerformanceBindings::Result::Assigned)
         {
            CancelMapping();
            ShowStatus("key assigned; save session to persist", gTime);
         }
         else
            ShowStatus(result == PerformanceBindings::Result::Duplicate ? "key already mapped" :
                       "reserved: use Ctrl+Option+letter/digit", gTime);
         return true;
      }
      return false; // mapping UI is open; do not launch clips via old bindings
   }
   if (!mKeyboardEnabled)
      return false;
   const int action = mBindings.ActionFor(key, modifiers);
   if (action < 0)
      return false;
   PerformAction(action, gTime);
   return true;
}

void LoopGrid::CancelMapping()
{
   std::lock_guard<std::mutex> lock(sMidiLearnMutex);
   if (sMidiLearnGrid.load() == this)
   {
      sMidiLearnGrid.store(nullptr);
      sMidiLearnMessage.store(0);
      sMidiLearnController.store(nullptr);
   }
   mPendingMapAction = -1;
   mMapMode = MapMode::Off;
}

void LoopGrid::SelectMapAction(int action, double time)
{
   if (mMapMode == MapMode::Key)
   {
      mPendingMapAction = action;
      ShowStatus("press Ctrl+Option+letter/digit; Esc cancels", time);
      return;
   }
   // Replace the selected MIDI target under the same lock used by the MIDI
   // callback, so it never reads a concurrently changed action index.
   {
      std::lock_guard<std::mutex> lock(sMidiLearnMutex);
      if (sMidiLearnGrid.load() == this)
         sMidiLearnGrid.store(nullptr);
      mPendingMapAction = -1;
   }
   // Learn only from an already connected native MIDI controller.
   std::vector<IDrawableModule*> modules;
   TheSynth->GetAllModules(modules);
   bool connected = false;
   for (auto* item : modules)
      if (auto* midi = dynamic_cast<MidiController*>(item))
         if (!midi->IsDeleted() && midi->IsInputConnected(false))
            connected = true;
   if (!connected)
   {
      CancelMapping();
      ShowStatus("no connected MIDI input; configure a midicontroller", time);
      return;
   }
   {
      std::lock_guard<std::mutex> lock(sMidiLearnMutex);
      mPendingMapAction = action;
      sMidiLearnMessage.store(0);
      sMidiLearnController.store(nullptr);
      sMidiLearnGrid.store(this);
   }
   ShowStatus("send a MIDI note or CC on your connected controller", time);
}

bool LoopGrid::CaptureMidiLearn(MidiController* controller, MidiMessageType type, int control, int channel, float value)
{
   if (value <= 0 ||
       (type != kMidiMessage_Note && type != kMidiMessage_Control) ||
       control < 0 || control > 127 || channel < 0 || channel > 16)
      return false;
   std::lock_guard<std::mutex> lock(sMidiLearnMutex);
   LoopGrid* grid = sMidiLearnGrid.load();
   if (!grid)
      return false;
   if (sMidiLearnMessage.load() == 0)
   {
      sMidiLearnController.store(controller);
      const int action = grid->mPendingMapAction;
      const MidiBinding previous = action >= 0 && action < PerformanceBindings::kActions
                                     ? grid->mMidiBindings[action] : MidiBinding{};
      auto* target = grid->ControlForAction(action);
      uint64_t error = 0;
      if (action < 0 || !target)
         error = 1;
      else if (action >= 15 && type != kMidiMessage_Control)
         error = 2;
      else if (previous.Valid() && previous.controllerPath != controller->Path())
         error = 4;
      else if (auto* existing = controller->GetConnectionForControl(type, control))
      {
         const bool isPrevious = previous.Valid() && previous.type == type &&
            previous.control == control && previous.channel == channel &&
            previous.page == controller->GetPage() &&
            (existing->mUIControl == target || existing->mShouldRetryForUIControlAt == target->Path());
         if (!isPrevious)
            error = 3;
      }
      if (!error)
      {
         // Match MidiController's own bind-mode behavior: mutate its connection
         // vector in the same MIDI callback, before normal dispatch iterates it.
         if (previous.Valid())
            controller->RemoveConnectionForUIControl(MidiMessageType(previous.type), previous.control,
                                                     previous.channel, previous.page, target);
         auto* connection = controller->AddControlConnection(type, control, channel, target);
         if (connection && action < 15)
         {
            // ClickButton::SetFromMidiCC clicks on positive values and treats
            // zero as release. Disable per-device incremental/scaled defaults.
            connection->mType = kControlType_Slider;
            connection->mIncrementAmount = 0;
            connection->mScaleOutput = false;
            connection->m14BitMode = false;
         }
         if (connection)
            grid->mMidiBindings[action] = { controller->Path(), int(type), control, channel, controller->GetPage() };
      }
      const uint64_t packed = 1 | (uint64_t(type) << 8) | (uint64_t(control) << 16) |
                              (uint64_t(channel) << 32) | (error << 48);
      sMidiLearnMessage.store(packed);
   }
   return true; // consume the learned event, so it cannot also trigger a clip
}

void LoopGrid::CompleteMidiLearn()
{
   uint64_t packed = 0;
   MidiController* controller = nullptr;
   {
      std::lock_guard<std::mutex> lock(sMidiLearnMutex);
      if (sMidiLearnGrid.load() != this || mMapMode != MapMode::Midi || mPendingMapAction < 0)
         return;
      packed = sMidiLearnMessage.exchange(0);
      if (packed)
      {
         sMidiLearnGrid.store(nullptr);
         controller = sMidiLearnController.exchange(nullptr);
      }
   }
   if (!packed)
      return;
   std::vector<IDrawableModule*> modules;
   TheSynth->GetAllModules(modules);
   bool stillPresent = false;
   for (auto* item : modules)
      if (item == controller && !item->IsDeleted())
         stillPresent = true;
   if (!stillPresent)
   {
      CancelMapping();
      ShowStatus("MIDI controller disappeared", gTime);
      return;
   }
   const auto type = MidiMessageType((packed >> 8) & 0xff);
   const int control = int((packed >> 16) & 0xff);
   const int channel = int((packed >> 32) & 0xff);
   const int error = int((packed >> 48) & 0xff);
   if (error == 1)
   {
      CancelMapping();
      ShowStatus("MIDI target unavailable", gTime);
      return;
   }
   if (error == 2)
   {
      CancelMapping();
      ShowStatus("gain needs MIDI CC; buttons take note or momentary CC", gTime);
      return;
   }
   if (error == 3)
   {
      CancelMapping();
      ShowStatus("MIDI control already mapped", gTime);
      return;
   }
   if (error == 4)
   {
      CancelMapping();
      ShowStatus("other controller owns this map; edit it in the MIDI controller", gTime);
      return;
   }
   mMidiLabels[mPendingMapAction] = controller->GetDeviceIn() + " " +
      (type == kMidiMessage_Note ? "note " : "CC ") + std::to_string(control) +
      " ch" + std::to_string(channel);
   CancelMapping();
   ShowStatus("MIDI mapped; save session to persist", gTime);
}
