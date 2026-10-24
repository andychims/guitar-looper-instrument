#include "PerformanceBindings.h"
#include <cctype>

PerformanceBindings::PerformanceBindings()
{
   for (int i = 0; i < 4; ++i)
      mKeys[i] = '1' + i;
   mKeys[4] = 'S'; // stop all
   mKeys[5] = 'P'; // panic
   mKeys[6] = 'T'; // set 120 BPM
   mKeys[7] = 'Q'; // rhythm mute
   mKeys[8] = 'W'; // lead mute
   mKeys[9] = 'A'; // rhythm solo
   mKeys[10] = 'D'; // lead solo
   mKeys[11] = 'E'; // rhythm FX
   mKeys[12] = 'R'; // lead FX
   mKeys[13] = 'Z'; // rhythm monitor
   mKeys[14] = 'X'; // lead monitor
   mKeys[15] = 'U'; // rhythm gain +0.1
   mKeys[16] = 'I'; // lead gain +0.1
}

int PerformanceBindings::Normalize(int key)
{
   if (key >= 'a' && key <= 'z')
      return key - 'a' + 'A';
   return key;
}

bool PerformanceBindings::IsAllowed(int key, int modifiers)
{
   key = Normalize(key);
   // Exact Ctrl+Option leaves stock QWERTY, Cmd, Shift and Option shortcuts
   // available. No navigation, punctuation, space or function key is stolen.
   return modifiers == kRequiredModifiers &&
          ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'));
}

PerformanceBindings::Result PerformanceBindings::Assign(int action, int key, int modifiers)
{
   if (action < 0 || action >= kActions)
      return Result::InvalidAction;
   if (!IsAllowed(key, modifiers))
      return Result::Reserved;
   key = Normalize(key);
   for (int i = 0; i < kActions; ++i)
      if (i != action && mKeys[i] == key)
         return Result::Duplicate;
   mKeys[action] = key;
   return Result::Assigned;
}

int PerformanceBindings::ActionFor(int key, int modifiers) const
{
   if (!IsAllowed(key, modifiers))
      return -1;
   key = Normalize(key);
   for (int i = 0; i < kActions; ++i)
      if (mKeys[i] == key)
         return i;
   return -1;
}

int PerformanceBindings::KeyFor(int action) const
{
   return action >= 0 && action < kActions ? mKeys[action] : 0;
}

std::string PerformanceBindings::Label(int key)
{
   if (!key)
      return "unmapped";
   return std::string("Ctrl+Opt+") + char(Normalize(key));
}

bool PerformanceBindings::ImportKeys(const std::array<int, kActions>& keys)
{
   PerformanceBindings candidate;
   candidate.mKeys.fill(0);
   for (int action = 0; action < kActions; ++action)
      if (keys[action] != 0 &&
          candidate.Assign(action, keys[action], kRequiredModifiers) != Result::Assigned)
         return false;
   mKeys = candidate.mKeys;
   return true;
}
