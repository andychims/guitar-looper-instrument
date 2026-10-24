#pragma once

// Commit writes may begin before sample zero to crossfade the loop boundary.
// C++ remainder is negative for negative input, so normalize before indexing.
inline int WrapLooperCommitIndex(int index, int length)
{
   int wrapped = index % length;
   return wrapped < 0 ? wrapped + length : wrapped;
}
