#pragma once
#include <string>
#include <windows.data.xml.dom.h>

class Notify {
public:
  static void Initialize(const std::wstring &appId);
  static void Toast(const std::wstring &title, const std::wstring &message);

private:
  static std::wstring s_appId;
};
