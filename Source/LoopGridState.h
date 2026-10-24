#pragma once

// Pure transport/state rules for the four-slot controller. No Bespoke objects or
// audio calls live here, so transitions can be checked without starting audio.
namespace loopgrid
{
enum State { Empty, Armed, Recording, Queued, Playing, Stopped };
enum Action { NoAction, StartCapture, FinishCapture, StartPlay, StopPlay, AbortCapture };

struct Transition
{
   State state;
   Action action;
   int target;
   bool clear{ false };
   bool hold{ false };
   bool commit{ false };
   bool launch{ false };
   bool stop{ false };
   bool displaceSibling{ false };
   bool releaseHold{ false };
};

inline Transition Click(State state, Action pending, bool occupied, bool siblingCapturing,
                        int nextBar, bool captureFits)
{
   Transition result{ state, pending, nextBar };
   if (state == Empty && !occupied && !siblingCapturing && captureFits)
      return { Armed, StartCapture, nextBar, true, true };
   if (state == Armed)
      return { Empty, NoAction, nextBar };
   if (state == Playing)
      return { Queued, StopPlay, nextBar };
   if (state == Stopped && occupied)
      return { Queued, StartPlay, nextBar };
   if (state == Queued)
   {
      // A queued stop has not muted the audio yet. Cancelling it restores
      // Playing; a queued launch is still silent, so it restores Stopped.
      if (pending == StopPlay)
         return { Playing, NoAction, nextBar };
      if (pending == StartPlay)
         return { occupied ? Stopped : Empty, NoAction, nextBar };
   }
   return result;
}

inline Transition OnBar(State state, Action pending, int measure, int bars, bool committing)
{
   if (pending == StartCapture && state == Armed)
      return { Recording, FinishCapture, measure + bars };
   if (pending == FinishCapture && state == Recording)
      return { Queued, StartPlay, measure + 1, false, false, true, false, false, true };
   if (pending == StartPlay && state == Queued)
   {
      if (committing)
         return { Queued, StartPlay, measure + 1 };
      return { Playing, NoAction, measure, false, false, false, true, false, true };
   }
   if (pending == StopPlay && state == Queued)
      return { Stopped, NoAction, measure, false, false, false, false, true };
   if (pending == AbortCapture && state == Queued)
      return { Empty, NoAction, measure, false, false, false, false, true, false, true };
   return { state, NoAction, measure };
}

inline Transition StopAll(State state, Action pending, bool occupied, int nextBar)
{
   if (state == Armed)
      return { Empty, NoAction, nextBar, false, false, false, false, false, false, true };
   if (state == Recording)
      return { Queued, AbortCapture, nextBar };
   if (state == Playing || (state == Queued && pending == StopPlay))
      return { Queued, StopPlay, nextBar };
   if (state == Queued && pending == StartPlay)
      return { occupied ? Stopped : Empty, NoAction, nextBar };
   return { state, pending, nextBar };
}

inline bool IsCapturing(State state) { return state == Armed || state == Recording; }
inline State OnStartup(bool occupied) { return occupied ? Stopped : Empty; }
inline State OnDisplaced(State state)
{
   return state == Playing || state == Queued ? Stopped : state;
}
inline float EffectiveTrackGain(float gain, bool muted, int soloTrack, int track)
{
   return muted || (soloTrack >= 0 && soloTrack != track) ? 0.0f : gain;
}
}
