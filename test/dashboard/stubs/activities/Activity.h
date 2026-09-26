#pragma once
#include <GfxRenderer.h>
#include <Logging.h>
class MappedInputManager {
 public:
  enum class Button { Back };
  bool wasPressed(Button) const { return false; }
  struct Labels {
    const char* btn1;
    const char* btn2;
    const char* btn3;
    const char* btn4;
  };
  Labels mapLabels(const char* a, const char* b, const char* c, const char* d) { return {a, b, c, d}; }
};
struct RenderLock {
  ~RenderLock() {}
};
class Activity {
 protected:
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;

 public:
  inline static bool finished = false;
  Activity(const char*, GfxRenderer& r, MappedInputManager& m) : renderer(r), mappedInput(m) {}
  virtual ~Activity() = default;
  virtual void onEnter() { finished = false; }
  virtual void onExit() {}
  virtual void loop() {}
  virtual void render(RenderLock&&) {}
  virtual bool preventAutoSleep() { return false; }
  void requestUpdate() {}
  static void finish() { finished = true; }
};
