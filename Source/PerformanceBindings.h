#pragma once

#include <array>
#include <string>

// This deliberately owns only policy and key/action pairs. The performer must
// explicitly enable routing in LoopGrid; this class never hooks global keys.
class PerformanceBindings
{
public:
   static constexpr int kActions = 17;
   static constexpr int kRequiredModifiers = 2 | 4; // Bespoke Alt | Control
   enum class Result { Assigned, Reserved, Duplicate, InvalidAction };

   PerformanceBindings();
   static bool IsAllowed(int key, int modifiers);
   static int Normalize(int key);
   Result Assign(int action, int key, int modifiers);
   int ActionFor(int key, int modifiers) const;
   int KeyFor(int action) const;
   std::array<int, kActions> ExportKeys() const { return mKeys; }
   bool ImportKeys(const std::array<int, kActions>& keys);
   static std::string Label(int key);

private:
   std::array<int, kActions> mKeys{};
};
