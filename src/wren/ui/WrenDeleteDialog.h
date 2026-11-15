#pragma once
#include "activities/Activity.h"

class WrenDeleteDialog final : public Activity {
 public:
  WrenDeleteDialog(GfxRenderer& r, MappedInputManager& i, const std::string& path, const std::string& title,
                   void (*paintBackground)(void*), void* backgroundContext);
  // Confirm-only variant for non-file actions: no path, no size row, nothing is deleted. Confirm
  // finishes with a non-cancelled result and the caller does the work. Text pointers must outlive
  // the dialog (tr() strings do).
  WrenDeleteDialog(GfxRenderer& r, MappedInputManager& i, const char* heading, const char* body,
                   const char* confirmLabel, void (*paintBackground)(void*), void* backgroundContext);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void keep();
  void remove();
  std::string path;
  std::string title;
  uint64_t bytes = 0;
  bool swallowBack = false;
  bool swallowConfirm = false;
  void (*paintBackground)(void*);
  void* backgroundContext;
  bool failed = false;
  // Set only by the confirm-only constructor.
  const char* customHeading = nullptr;
  const char* customBody = nullptr;
  const char* customConfirm = nullptr;
};
