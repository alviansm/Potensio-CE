#include "Notify.h"
#include <filesystem>
#include <fstream>
#include <shlobj.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

using namespace WinToastLib;

void Notify::show(const std::wstring &title, const std::wstring &message,
                  const std::wstring &customSoundFile) {
  if (!WinToast::isCompatible()) {
    return;
  }

  WinToast::instance()->setAppName(L"Potensio");
  const auto aumi =
      WinToast::configureAUMI(L"YourCompany", L"Potensio", L"App", L"2025");
  WinToast::instance()->setAppUserModelId(aumi);

  if (!WinToast::instance()->initialize()) {
    return;
  }

  WinToastTemplate templ(WinToastTemplate::Text02);
  templ.setTextField(title, WinToastTemplate::FirstLine);
  templ.setTextField(message, WinToastTemplate::SecondLine);

  // Handle sound - Priority: Custom file -> Default system sound
  if (!customSoundFile.empty()) {
    std::wstring soundPath = getSoundPath(customSoundFile);
    if (fileExists(soundPath)) {
      templ.setAudioPath(soundPath);
      templ.setAudioOption(WinToastTemplate::AudioOption::Default);
    } else {
      // Fall back to default system sound if custom sound not found
      templ.setAudioPath(WinToastTemplate::AudioSystemFile::DefaultSound);
    }
  } else {
    // Use default notification sound
    templ.setAudioPath(WinToastTemplate::AudioSystemFile::DefaultSound);
  }

  WinToast::instance()->showToast(templ, new Notify());
}

void Notify::show(const std::wstring &title, const std::wstring &message,
                  int resourceId) {
  if (!WinToast::isCompatible()) {
    return;
  }

  WinToast::instance()->setAppName(L"Potensio");
  const auto aumi =
      WinToast::configureAUMI(L"YourCompany", L"Potensio", L"App", L"2025");
  WinToast::instance()->setAppUserModelId(aumi);

  if (!WinToast::instance()->initialize()) {
    return;
  }

  WinToastTemplate templ(WinToastTemplate::Text02);
  templ.setTextField(title, WinToastTemplate::FirstLine);
  templ.setTextField(message, WinToastTemplate::SecondLine);

  // Handle sound - Priority: Custom file -> Resource -> Default system sound
  // First, try to find a custom sound file based on resource ID
  std::wstring customSoundFile;
  switch (resourceId) {
  case 139: // SOUND_POMODORO_END
    customSoundFile = L"pomodoro_end.mp3";
    break;
  case 140: // SOUND_BREAK_END
    customSoundFile = L"break_end.mp3";
    break;
  case 141: // SOUND_SUCCESS
    customSoundFile = L"success.mp3";
    break;
  case 142: // SOUND_NOTIFICATION
    customSoundFile = L"notification.mp3";
    break;
  }

  bool soundSet = false;

  // 1. Try custom sound file first
  if (!customSoundFile.empty()) {
    std::wstring soundPath = getSoundPath(customSoundFile);
    if (fileExists(soundPath)) {
      templ.setAudioPath(soundPath);
      templ.setAudioOption(WinToastTemplate::AudioOption::Default);
      soundSet = true;
    }
  }

  // 2. If no custom sound, try embedded resource
  if (!soundSet) {
    std::wstring resourcePath = extractResourceToTemp(resourceId);
    if (!resourcePath.empty() && fileExists(resourcePath)) {
      templ.setAudioPath(resourcePath);
      templ.setAudioOption(WinToastTemplate::AudioOption::Default);
      soundSet = true;
    }
  }

  // 3. Fall back to default system sound
  if (!soundSet) {
    templ.setAudioPath(WinToastTemplate::AudioSystemFile::DefaultSound);
  }

  WinToast::instance()->showToast(templ, new Notify());
}

std::wstring Notify::getExecutablePath() {
  wchar_t buffer[MAX_PATH];
  GetModuleFileNameW(NULL, buffer, MAX_PATH);

  // Remove the executable name to get just the directory
  PathRemoveFileSpecW(buffer);

  return std::wstring(buffer);
}

std::wstring Notify::getSoundPath(const std::wstring &soundFileName) {
  std::wstring exePath = getExecutablePath();
  std::wstring soundDir = exePath + L"\\sounds\\";
  return soundDir + soundFileName;
}

std::wstring Notify::extractResourceToTemp(int resourceId) {
  try {
    // Find the resource
    HRSRC hResource =
        FindResourceW(NULL, MAKEINTRESOURCEW(resourceId), L"RCDATA");
    if (!hResource)
      return L"";

    // Load the resource
    HGLOBAL hResourceData = LoadResource(NULL, hResource);
    if (!hResourceData)
      return L"";

    // Get resource data
    void *pResourceData = LockResource(hResourceData);
    DWORD resourceSize = SizeofResource(NULL, hResource);
    if (!pResourceData || resourceSize == 0)
      return L"";

    // Get temp directory
    wchar_t tempDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tempDir);

    // Create unique filename based on resource ID
    std::wstring tempFile = std::wstring(tempDir) + L"potensio_sound_" +
                            std::to_wstring(resourceId) + L".mp3";

    // Write resource to temp file
    std::ofstream file(tempFile, std::ios::binary);
    if (!file.is_open())
      return L"";

    file.write(static_cast<const char *>(pResourceData), resourceSize);
    file.close();

    return tempFile;
  } catch (...) {
    return L"";
  }
}

bool Notify::fileExists(const std::wstring &path) {
  return std::filesystem::exists(path);
}

void Notify::toastActivated() const {
  // Handle click
}

void Notify::toastActivated(int actionIndex) const {
  // Handle action button click
}

void Notify::toastActivated(std::wstring response) const {
  // Handle text input response (for toasts with input fields)
}

void Notify::toastDismissed(WinToastDismissalReason state) const {
  // Handle dismiss
}

void Notify::toastFailed() const {
  // Handle failure
}