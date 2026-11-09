#include "platform/windows/private/printWindowCompatibility.h"
#include <iostream>
int main() {
 for (int scenario = 0; scenario < 6; ++scenario) {
  int calls = 0, clears = 0; bool flagsOk = true; std::string error;
  const bool result = desktopctl::printWindowCompatible([&](UINT flags) {
   ++calls; flagsOk &= flags == (calls == 1 ? PW_RENDERFULLCONTENT : 0);
   if (scenario == 0 || (calls == 2 && scenario == 1)) return true;
   SetLastError(calls == 1 ? (scenario == 2 ? ERROR_ACCESS_DENIED : scenario == 3 ? 0 : ERROR_INVALID_PARAMETER) : ERROR_INVALID_WINDOW_HANDLE);
   return false;
  }, [&] { ++clears; return scenario != 5; }, error);
  const int expectedCalls = (scenario == 1 || scenario == 4) ? 2 : 1;
  if (!flagsOk || calls != expectedCalls || result != (scenario <= 1)
      || clears != ((scenario == 1 || scenario >= 4) ? 1 : 0)
      || (!result && error.empty()) || (scenario == 4 && error.find("1400") == std::string::npos)) return 1;
 }
 std::cout << "Six PrintWindow fallback scenarios passed.\n";
}
