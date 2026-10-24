#include "../../Source/LoopGridState.h"
#include <cassert>

using namespace loopgrid;

int main()
{
   // A fresh slot is inert until the requested boundary, then captures the
   // chosen number of complete bars. A gradual stock commit defers launch.
   State rhythm = OnStartup(false);
   assert(rhythm == Empty);
   auto r = Click(rhythm, NoAction, false, false, 3, true);
   assert(r.state == Armed && r.action == StartCapture && r.target == 3 && r.hold);
   rhythm = r.state;
   assert(2 < r.target); // the transport must not dispatch this command in bar 2
   r = OnBar(rhythm, r.action, 3, 4, false);
   assert(r.state == Recording && r.action == FinishCapture && r.target == 7);
   rhythm = r.state;
   r = OnBar(rhythm, r.action, 7, 4, false);
   assert(r.state == Queued && r.action == StartPlay && r.target == 8 && r.commit);
   rhythm = r.state;
   r = OnBar(rhythm, r.action, 8, 4, true);
   assert(r.state == Queued && r.action == StartPlay && r.target == 9);
   r = OnBar(r.state, r.action, 9, 4, false);
   assert(r.state == Playing && r.launch && r.displaceSibling);
   rhythm = r.state;

   // Cancel a stop before its bar: audio remains playing and the UI says so.
   r = Click(rhythm, NoAction, true, false, 10, true);
   assert(r.state == Queued && r.action == StopPlay);
   r = Click(r.state, r.action, true, false, 10, true);
   assert(r.state == Playing && r.action == NoAction);
   r = Click(r.state, NoAction, true, false, 11, true);
   r = OnBar(r.state, r.action, 11, 4, false);
   assert(r.state == Stopped && r.stop);

   // A queued launch is silent until its bar; cancelling restores stopped.
   r = Click(Stopped, NoAction, true, false, 12, true);
   assert(r.state == Queued && r.action == StartPlay);
   r = Click(r.state, r.action, true, false, 12, true);
   assert(r.state == Stopped && r.action == NoAction);
   // This also applies to an auto-launch queued while a captured buffer is
   // finishing: cancelling cannot abort the commit, but leaves it silent.
   r = Click(Queued, StartPlay, true, false, 12, true);
   assert(r.state == Stopped && r.action == NoAction);

   // A second slot on the same track cannot arm while capture is underway.
   r = Click(Empty, NoAction, false, true, 4, true);
   assert(r.state == Empty && r.action == NoAction);
   r = Click(Empty, NoAction, false, false, 4, false); // 60-second limit
   assert(r.state == Empty && r.action == NoAction);
   r = Click(Armed, StartCapture, false, false, 4, true);
   assert(r.state == Empty && r.action == NoAction); // cancel arm

   // Launching B displaces A on the launch bar, independently of Lead.
   State lead = Playing;
   r = Click(Stopped, NoAction, true, false, 13, true);
   assert(r.state == Queued && r.action == StartPlay);
   r = OnBar(r.state, r.action, 13, 8, false);
   assert(r.state == Playing && r.displaceSibling);
   assert(OnDisplaced(Playing) == Stopped);
   assert(lead == Playing);

   // Reload never resumes a recording, a queue, or audible playback.
   assert(OnStartup(true) == Stopped);
   assert(OnStartup(false) == Empty);

   // One solo at a time; mute survives solo toggles and gain changes.
   assert(EffectiveTrackGain(.7f, false, -1, 0) == .7f);
   assert(EffectiveTrackGain(.7f, false, 0, 0) == .7f);
   assert(EffectiveTrackGain(.8f, false, 0, 1) == 0);
   assert(EffectiveTrackGain(.7f, true, 0, 0) == 0);
   assert(EffectiveTrackGain(.8f, false, -1, 1) == .8f);

   // Stop All preserves the audio until the boundary, cancels an arm, and
   // aborts an in-progress capture without marking the slot occupied.
   r = StopAll(Playing, NoAction, true, 21);
   assert(r.state == Queued && r.action == StopPlay && r.target == 21);
   r = OnBar(r.state, r.action, 21, 4, false);
   assert(r.state == Stopped && r.stop);
   r = StopAll(Armed, StartCapture, false, 21);
   assert(r.state == Empty && r.action == NoAction && r.releaseHold);
   r = StopAll(Recording, FinishCapture, false, 21);
   assert(r.state == Queued && r.action == AbortCapture && r.target == 21);
   r = OnBar(r.state, r.action, 21, 4, false);
   assert(r.state == Empty && r.releaseHold && r.stop);
   r = StopAll(Queued, StartPlay, true, 21);
   assert(r.state == Stopped && r.action == NoAction);
   assert(EffectiveTrackGain(.7f, true, -1, 0) == 0);
   assert(EffectiveTrackGain(.8f, true, -1, 1) == 0);
   return 0;
}
