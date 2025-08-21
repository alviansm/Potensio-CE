#pragma once
#include "resource.h" // Include your resource header
#include <string>
#include <wintoastlib.h>

class Notify : public WinToastLib::IWinToastHandler {
public:
  // Static method for easy usage with string filename
  static void show(const std::wstring &title, const std::wstring &message,
                   const std::wstring &customSoundFile = L"");

  // Static method for easy usage with resource ID
  static void show(const std::wstring &title, const std::wstring &message,
                   int resourceId);

  // IWinToastHandler overrides
  void toastActivated() const override;
  void toastActivated(int actionIndex) const override;
  void toastActivated(std::wstring response) const override;
  void toastDismissed(WinToastDismissalReason state) const override;
  void toastFailed() const override;

private:
  // Helper methods for sound handling
  static std::wstring getExecutablePath();
  static std::wstring getSoundPath(const std::wstring &soundFileName);
  static std::wstring extractResourceToTemp(int resourceId);
  static bool fileExists(const std::wstring &path);
};