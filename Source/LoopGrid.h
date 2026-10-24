#pragma once

#include "IDrawableModule.h"
#include "Transport.h"
#include "ClickButton.h"
#include "Checkbox.h"
#include "Slider.h"
#include "DropdownList.h"
#include "LoopGridState.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <string>

class Looper;
class LooperRecorder;
class Amplifier;

// A small control surface for four stock loopers. Audio and waveform persistence
// remain with Looper/LooperRecorder; this module never owns audio buffers.
class LoopGrid : public IDrawableModule, public ITimeListener, public IButtonListener,
                 public IFloatSliderListener, public IDropdownListener
{
public:
   LoopGrid();
   ~LoopGrid() override;
   static IDrawableModule* Create() { return new LoopGrid(); }
   void CreateUIControls() override;
   void Init() override;
   void Poll() override;
   void OnTimeEvent(double time) override;
   void ButtonClicked(ClickButton* button, double time) override;
   void FloatSliderUpdated(FloatSlider* slider, float oldVal, double time) override;
   void DropdownUpdated(DropdownList* list, int oldVal, double time) override;
   void LoadLayout(const ofxJSONElement& moduleInfo) override;
   void SaveLayout(ofxJSONElement& moduleInfo) override;
   void SetUpFromSaveData() override;
   void PostLoadState() override;
   bool CheckNeedsDraw() override { return true; }

private:
   using State = loopgrid::State;
   using Action = loopgrid::Action;
   static constexpr State Empty = loopgrid::Empty;
   static constexpr State Armed = loopgrid::Armed;
   static constexpr State Recording = loopgrid::Recording;
   static constexpr State Queued = loopgrid::Queued;
   static constexpr State Playing = loopgrid::Playing;
   static constexpr State Stopped = loopgrid::Stopped;
   static constexpr Action NoAction = loopgrid::NoAction;
   static constexpr Action StartCapture = loopgrid::StartCapture;
   static constexpr Action FinishCapture = loopgrid::FinishCapture;
   static constexpr Action StartPlay = loopgrid::StartPlay;
   static constexpr Action StopPlay = loopgrid::StopPlay;
   struct Slot
   {
      Looper* looper{ nullptr };
      ClickButton* button{ nullptr };
      std::atomic<int> state{ Empty };
      std::atomic<uint64_t> command{ 0 };
      std::atomic<int> bars{ 4 };
      std::atomic<bool> occupied{ false };
   };
   void Queue(int index, Action action, int target, State state);
   void BindRecorders();
   void ApplyTrackGains(double time);
   void ShowStatus(const char* text, double time);
   bool Ready(int index) const;
   void DrawModule() override;
   void GetModuleDimensions(float& width, float& height) override { width = 282; height = 171; }

   std::array<Slot, 4> mSlots;
   std::array<LooperRecorder*, 2> mRecorders{ nullptr, nullptr };
   std::array<Amplifier*, 2> mTrackAmps{ nullptr, nullptr };
   std::array<ClickButton*, 2> mMuteButtons{ nullptr, nullptr };
   std::array<ClickButton*, 2> mSoloButtons{ nullptr, nullptr };
   std::array<FloatSlider*, 2> mGainSliders{ nullptr, nullptr };
   std::array<float, 2> mGain{ 1, 1 };
   std::array<bool, 2> mMuted{ false, false };
   int mSoloTrack{ -1 };
   std::string mStatus;
   double mStatusUntil{ -1 };
   DropdownList* mLengthSelector{ nullptr };
   int mCaptureBars{ 4 };
};
