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
#include <cmath>

namespace
{
   const char* kSlotKey[4] = { "rhythm_a", "rhythm_b", "lead_a", "lead_b" };
   const char* kRecorderKey[2] = { "rhythm_recorder", "lead_recorder" };
   const char* kAmpKey[2] = { "rhythm_amp", "lead_amp" };
   const char* kStateName[6] = { "empty", "armed", "recording", "queued", "playing", "stopped" };
   uint64_t PackCommand(int action, int target) { return (uint64_t(uint32_t(target)) << 8) | uint64_t(action); }
}

LoopGrid::LoopGrid() = default;

LoopGrid::~LoopGrid()
{
   TheTransport->RemoveListener(this);
}

void LoopGrid::CreateUIControls()
{
   IDrawableModule::CreateUIControls();
   mLengthSelector = new DropdownList(this, "capture bars", 179, 3, &mCaptureBars);
   mLengthSelector->AddLabel("4 bars", 4);
   mLengthSelector->AddLabel("8 bars", 8);
   for (int track = 0; track < 2; ++track)
   {
      int y = 28 + track * 59;
      mMuteButtons[track] = new ClickButton(this, track == 0 ? "Rhythm mute" : "Lead mute", 3, y);
      mMuteButtons[track]->SetOverrideDisplayName("mute");
      mSoloButtons[track] = new ClickButton(this, track == 0 ? "Rhythm solo" : "Lead solo", 3, y + 19);
      mSoloButtons[track]->SetOverrideDisplayName("solo");
      mGainSliders[track] = new FloatSlider(this, track == 0 ? "Rhythm gain" : "Lead gain",
                                            3, y + 38, 103, 15, &mGain[track], 0, 2);
      for (int col = 0; col < 2; ++col)
      {
         int i = track * 2 + col;
         const char* name = track == 0 ? (col == 0 ? "Rhythm A" : "Rhythm B")
                                       : (col == 0 ? "Lead A" : "Lead B");
         mSlots[i].button = new ClickButton(this, name, 122 + col * 78, y);
         mSlots[i].button->SetOverrideDisplayName(col == 0 ? "A" : "B");
         mSlots[i].button->SetDimensions(72, 45);
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

void LoopGrid::ButtonClicked(ClickButton* button, double time)
{
   for (int track = 0; track < 2; ++track)
   {
      if (button == mMuteButtons[track])
      {
         mMuted[track] = !mMuted[track];
         ApplyTrackGains(time);
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
      slot.state.store(next.state);
      if (next.action != NoAction)
         slot.command.store(PackCommand(next.action, next.target));
   }
}

void LoopGrid::FloatSliderUpdated(FloatSlider* slider, float oldVal, double time)
{
   for (int track = 0; track < 2; ++track)
      if (slider == mGainSliders[track])
         ApplyTrackGains(time);
}

void LoopGrid::DropdownUpdated(DropdownList* list, int oldVal, double time) {}

void LoopGrid::DrawModule()
{
   if (Minimized() || !IsVisible())
      return;
   mLengthSelector->Draw();
   for (int track = 0; track < 2; ++track)
   {
      mMuteButtons[track]->Draw();
      mSoloButtons[track]->Draw();
      mGainSliders[track]->Draw();
      if (mMuted[track])
      {
         ofSetColor(255, 100, 100);
         DrawTextNormal("muted", 76, 30 + track * 59);
      }
      if (mSoloTrack == track)
      {
         ofSetColor(255, 225, 90);
         DrawTextNormal("solo", 76, 49 + track * 59);
      }
   }
   for (int i = 0; i < 4; ++i)
   {
      const State state = (State)mSlots[i].state.load();
      int x = 122 + (i % 2) * 78;
      int y = 28 + (i / 2) * 59;
      mSlots[i].button->Draw();
      switch (state)
      {
         case Empty: ofSetColor(65, 65, 65); break;
         case Armed: ofSetColor(230, 150, 20); break;
         case Recording: ofSetColor(220, 45, 35); break;
         case Queued: ofSetColor(230, 210, 40); break;
         case Playing: ofSetColor(55, 190, 85); break;
         case Stopped: ofSetColor(70, 105, 135); break;
      }
      ofRect(x + 55, y + 4, 12, 37);
      ofSetColor(255);
      DrawTextNormal(Ready(i) ? kStateName[state] : "unmapped", x + 5, y + 54);
   }
   if (gTime < mStatusUntil)
   {
      ofSetColor(255, 210, 105);
      DrawTextNormal(mStatus, 3, 161);
   }
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
      mModuleSaveData.LoadBool(std::string(kAmpKey[track]) + "_muted", moduleInfo, false);
   }
   mModuleSaveData.LoadInt("solo_track", moduleInfo, -1, -1, 1);
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
      moduleInfo[std::string(kAmpKey[track]) + "_muted"] = mMuted[track];
   }
   moduleInfo["solo_track"] = mSoloTrack;
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
      mMuted[track] = mModuleSaveData.GetBool(std::string(kAmpKey[track]) + "_muted");
   }
   mSoloTrack = mModuleSaveData.GetInt("solo_track");
   if (mSoloTrack < -1 || mSoloTrack > 1)
      mSoloTrack = -1;
   if (IsInitialized())
      BindRecorders();
   ApplyTrackGains(gTime);
}

void LoopGrid::PostLoadState()
{
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
}
