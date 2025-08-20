#include "Application.h"

class MouseTracker {
public:
  static MouseTracker &Get() {
    static MouseTracker instance;
    return instance;
  }

  void OnDragStart(POINT start) {
    dragging = true;
    lastPoint = start;
    lastDir = 0;
    wiggleCount = 0;
  }

  void OnMouseMove(POINT pt) {
    if (!dragging)
      return;

    int dx = pt.x - lastPoint.x;
    int dy = pt.y - lastPoint.y;

    // Use horizontal wiggle (left/right)
    int dir = (dx > 10) ? 1 : (dx < -10 ? -1 : 0);

    if (dir != 0 && dir != lastDir) {
      wiggleCount++;
      lastDir = dir;
      if (wiggleCount >= 3) { // e.g., 3 direction changes = wiggle
        OnWiggleDetected();
        dragging = false; // reset
      }
    }

    lastPoint = pt;
  }

  void OnDragEnd() { dragging = false; }

private:
  bool dragging = false;
  POINT lastPoint{};
  int lastDir = 0;
  int wiggleCount = 0;

  void OnWiggleDetected() {
    // Wake the app window here
    if (Application::GetInstance())
      Application::GetInstance()->ShowMainWindow();
  }
};