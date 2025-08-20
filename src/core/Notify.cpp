#include "Notify.h"
#include <shobjidl.h>
#include <windows.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.UI.Notifications.h>

using namespace winrt;
using namespace Windows::UI::Notifications;
using namespace Windows::Data::Xml::Dom;

std::wstring Notify::s_appId;

void Notify::Initialize(const std::wstring &appId) {
  s_appId = appId;
  SetCurrentProcessExplicitAppUserModelID(s_appId.c_str());
}

void Notify::Toast(const std::wstring &title, const std::wstring &message) {
  std::wstring xml = LR"(<toast>
              <visual>
                <binding template="ToastGeneric">
                  <text>)" +
                     title + LR"(</text>
                  <text>)" +
                     message + LR"(</text>
                </binding>
              </visual>
            </toast>)";

  XmlDocument doc;
  doc.LoadXml(xml);

  ToastNotification toast{doc};
  ToastNotificationManager::CreateToastNotifier(s_appId).Show(toast);
}