#pragma once
#include <random>
#include <string>
#include <vector>

namespace Pomodoro {

class Message {
public:
  static std::wstring getPomodoroStarted() {
    return randomPick(pomodoroStarted);
  }

  static std::wstring getPomodoroPaused() { return randomPick(pomodoroPaused); }

  static std::wstring getPomodoroStopped() {
    return randomPick(pomodoroStopped);
  }

  static std::wstring getSingleSessionCompleted() {
    return randomPick(singleSessionCompleted);
  }

  static std::wstring getProgress10() { return randomPick(progress10); }

  static std::wstring getProgress50() { return randomPick(progress50); }

  static std::wstring getProgress90() { return randomPick(progress90); }

  static std::wstring getSessionTimeUp() { return randomPick(sessionTimeUp); }

  static std::wstring getCycleCompleted() { return randomPick(cycleCompleted); }

private:
  inline static const std::vector<std::wstring> pomodoroStarted{
      L"Focus mode on, you’ve got this!", L"Time to shine, let’s begin!",
      L"Pomodoro started, stay sharp!", L"Deep work begins now!",
      L"Good luck! Make it count."};

  inline static const std::vector<std::wstring> pomodoroPaused{
      L"Taking a breather, nice work so far.", L"Paused. Stretch a little!",
      L"Break time, recharge your brain.", L"Paused—don’t lose momentum!",
      L"A short pause to come back stronger."};

  inline static const std::vector<std::wstring> pomodoroStopped{
      L"Session stopped. Reset and refocus.", L"Pomodoro ended early.",
      L"Stopped—sometimes it’s needed.",
      L"Reset complete, ready when you are."};

  inline static const std::vector<std::wstring> singleSessionCompleted{
      L"One session down—well done!", L"Great focus! That’s a wrap.",
      L"Pomodoro complete. Nice job!", L"You crushed that session!"};

  inline static const std::vector<std::wstring> progress10{
      L"You’ve just begun, steady pace!", L"10% in, keep rolling.",
      L"Nice start—stay focused!"};

  inline static const std::vector<std::wstring> progress50{
      L"Halfway there—keep up the momentum!", L"50% complete, you’re on track!",
      L"Half done, half to go!"};

  inline static const std::vector<std::wstring> progress90{
      L"Almost done, final push!", L"90% complete, don’t quit now!",
      L"Last stretch, give it your best!"};

  inline static const std::vector<std::wstring> sessionTimeUp{
      L"Time’s up—session complete!", L"Ding! That’s the end of this Pomodoro.",
      L"Well done, time to pause.", L"Session finished—great job!"};

  inline static const std::vector<std::wstring> cycleCompleted{
      L"Full cycle complete! Take a long break.",
      L"Great discipline—cycle finished!", L"You’ve earned a real rest.",
      L"Cycle wrapped up, fantastic work!"};

  static std::wstring randomPick(const std::vector<std::wstring> &list) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, list.size() - 1);
    return list[dist(gen)];
  }
};

} // namespace Pomodoro
