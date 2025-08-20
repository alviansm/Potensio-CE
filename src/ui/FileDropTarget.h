#pragma once
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <windows.h>

class UIManager;

class FileDropTarget : public IDropTarget {
public:
  FileDropTarget(UIManager *ui) : m_refCount(1), m_ui(ui) {}

  // IUnknown
  HRESULT __stdcall QueryInterface(REFIID riid, void **ppvObject) override {
    if (riid == IID_IUnknown || riid == IID_IDropTarget) {
      *ppvObject = this;
      AddRef();
      return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
  }

  ULONG __stdcall AddRef() override {
    return InterlockedIncrement(&m_refCount);
  }

  ULONG __stdcall Release() override {
    LONG count = InterlockedDecrement(&m_refCount);
    if (count == 0)
      delete this;
    return count;
  }

  // IDropTarget
  HRESULT __stdcall DragEnter(IDataObject *pDataObj, DWORD, POINTL,
                              DWORD *pdwEffect) override {
    *pdwEffect = DROPEFFECT_COPY;
    StartShakeTracking();
    return S_OK;
  }

  HRESULT __stdcall DragOver(DWORD, POINTL pt, DWORD *pdwEffect) override {
    *pdwEffect = DROPEFFECT_COPY;
    CheckShakeGesture(pt);
    return S_OK;
  }

  HRESULT __stdcall DragLeave() override {
    StopShakeTracking();
    return S_OK;
  }

  HRESULT __stdcall Drop(IDataObject *pDataObj, DWORD, POINTL,
                         DWORD *pdwEffect) override {
    *pdwEffect = DROPEFFECT_COPY;
    HandleDroppedFiles(pDataObj);
    return S_OK;
  }

private:
  LONG m_refCount;
  UIManager *m_ui;

  // --- Wiggle detection state ---
  POINT m_lastPos = {};
  int m_lastDirection = 0;
  int m_shakeCount = 0;
  DWORD m_lastShakeTime = 0;

  void StartShakeTracking() {
    m_lastDirection = 0;
    m_shakeCount = 0;
    m_lastShakeTime = GetTickCount();
  }

  void StopShakeTracking() {
    m_lastDirection = 0;
    m_shakeCount = 0;
  }

  void CheckShakeGesture(const POINTL &pt) {
    POINT p = {pt.x, pt.y};
    int dx = p.x - m_lastPos.x;

    if (abs(dx) > 20) { // threshold
      int dir = (dx > 0) ? 1 : -1;
      DWORD now = GetTickCount();

      if (now - m_lastShakeTime > 1000) {
        m_shakeCount = 0; // reset if too slow
      }

      if (m_lastDirection != 0 && dir != m_lastDirection) {
        m_shakeCount++;
        m_lastShakeTime = now;
      }

      m_lastDirection = dir;
      m_lastPos = p;

      if (m_shakeCount >= 4) {
        m_ui->ShowWindow(); // wake the app
        StopShakeTracking();
      }
    }
  }

  void HandleDroppedFiles(IDataObject *pDataObj) {
    if (!pDataObj)
      return;

    FORMATETC fmt = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stg;
    if (SUCCEEDED(pDataObj->GetData(&fmt, &stg))) {
      HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
      if (hDrop) {
        UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; i++) {
          char filePath[MAX_PATH];
          DragQueryFileA(hDrop, i, filePath, MAX_PATH);
          if (m_ui && m_ui->GetMainWindow()) {
            m_ui->GetMainWindow()->AddStagedFile(filePath);
          }
        }
        GlobalUnlock(stg.hGlobal);
      }
      ReleaseStgMedium(&stg);
    }
  }
};
