#include "../../Source/PerformanceBindings.h"
#include <cassert>

int main()
{
   PerformanceBindings map;
   const int chord = PerformanceBindings::kRequiredModifiers;
   assert(map.ActionFor('1', chord) == 0);
   assert(map.ActionFor('4', chord) == 3);
   assert(map.ActionFor('S', chord) == 4);
   assert(map.ActionFor('P', chord) == 5);
   assert(map.ActionFor('Q', chord) == 7);
   assert(map.ActionFor('W', chord) == 8);
   assert(map.ActionFor('A', chord) == 9);
   assert(map.ActionFor('D', chord) == 10);
   assert(map.ActionFor('E', chord) == 11);
   assert(map.ActionFor('R', chord) == 12);
   assert(map.ActionFor('Z', chord) == 13);
   assert(map.ActionFor('X', chord) == 14);
   assert(map.ActionFor('U', chord) == 15);
   assert(map.ActionFor('I', chord) == 16);
   assert(map.ActionFor('1', 0) == -1);
   assert(map.ActionFor('1', 2) == -1);
   assert(map.ActionFor('1', 8) == -1);
   assert(map.ActionFor('1', chord | 1) == -1);
   assert(map.Assign(6, '9', 0) == PerformanceBindings::Result::Reserved);
   assert(map.Assign(6, '9', chord) == PerformanceBindings::Result::Assigned);
   assert(map.Assign(7, '9', chord) == PerformanceBindings::Result::Duplicate);
   assert(map.ActionFor('9', chord) == 6);
   const auto saved = map.ExportKeys();
   PerformanceBindings restored;
   assert(restored.ImportKeys(saved));
   assert(restored.ActionFor('9', chord) == 6);
   auto invalid = saved;
   invalid[8] = '9';
   assert(!restored.ImportKeys(invalid));
   assert(restored.ExportKeys() == saved);
   return 0;
}
