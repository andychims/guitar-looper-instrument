#include "../../Source/LooperCommitMath.h"
#include <cassert>

int main()
{
   constexpr int loopLength = 384000; // four bars at 120 BPM / 48 kHz
   assert(WrapLooperCommitIndex(-200, loopLength) == loopLength - 200);
   assert(WrapLooperCommitIndex(-1, loopLength) == loopLength - 1);
   assert(WrapLooperCommitIndex(0, loopLength) == 0);
   assert(WrapLooperCommitIndex(loopLength - 1, loopLength) == loopLength - 1);
   assert(WrapLooperCommitIndex(loopLength, loopLength) == 0);
   assert(WrapLooperCommitIndex(loopLength + 199, loopLength) == 199);
   return 0;
}
