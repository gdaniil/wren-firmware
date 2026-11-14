#include <gtest/gtest.h>

#include <cmath>

#include "src/wren/typography/ReaderTypography.h"

using namespace wren;

TEST(ReaderTypography, ConversionAndDefault) {
  EXPECT_EQ(mmToPx(25.4f, X3_PPI), 259);
  auto result = resolve(X3_PPI, Mode::Standard, modeSpec(Mode::Standard).defaultStep, Font::Literata, "ru");
  EXPECT_EQ(result.xHeightPx, 14);
  EXPECT_EQ(result.linePitchPx, 38);
  EXPECT_EQ(result.sideMarginPx, 16);
  EXPECT_EQ(result.regularWeight, 385);
  EXPECT_EQ(result.boldWeight, 450);
}

TEST(ReaderTypography, AllStepsAndLanguages) {
  for (int ppi : {X3_PPI, X4_PPI}) {
    for (int m = 0; m < 3; ++m) {
      auto mode = static_cast<Mode>(m);
      for (uint8_t step = 0; step < 5; ++step) {
        auto value = resolve(ppi, mode, step, Font::Bitter, "en-US");
        EXPECT_EQ(value.xHeightPx, mmToPx(modeSpec(mode).xHeightMm[step], ppi));
        EXPECT_EQ(value.linePitchPx, std::lround(modeSpec(mode).lineStepXh * value.xHeightPx));
        EXPECT_EQ(value.hyphenation.minLeft, 2);
        EXPECT_EQ(value.hyphenation.minRight, 3);
        EXPECT_EQ(resolve(ppi, mode, step, Font::Bitter, "ru").hyphenation.minRight, 2);
      }
    }
  }
}

TEST(ReaderTypography, Navigation) {
  uint8_t step = 3;
  EXPECT_EQ(nextStep(Mode::Standard, step), StepResult::Moved);
  EXPECT_EQ(step, 4);
  EXPECT_EQ(nextStep(Mode::Standard, step), StepResult::OfferNeighbour);
  EXPECT_EQ(step, 4);
  EXPECT_EQ(nextStep(Mode::Large, step), StepResult::AtBoundary);
  step = 0;
  EXPECT_EQ(previousStep(Mode::Standard, step), StepResult::OfferNeighbour);
  EXPECT_EQ(previousStep(Mode::Small, step), StepResult::AtBoundary);
  step = 1;
  EXPECT_EQ(previousStep(Mode::Small, step), StepResult::Moved);
  EXPECT_EQ(step, 0);
}

TEST(ReaderTypography, SpecFigures) {
  constexpr int expectedXh[2][3][5] = {
      {{11, 12, 13, 14, 15}, {14, 15, 17, 19, 22}, {24, 28, 32, 36, 41}},
      {{9, 10, 11, 12, 13}, {12, 13, 15, 16, 18}, {21, 23, 26, 30, 34}},
  };
  for (int device = 0; device < 2; ++device) {
    for (int m = 0; m < 3; ++m) {
      for (int step = 0; step < 5; ++step) {
        EXPECT_EQ(resolve(device == 0 ? X3_PPI : X4_PPI, static_cast<Mode>(m), step, Font::Literata, "en").xHeightPx,
                  expectedXh[device][m][step]);
      }
    }
  }
  EXPECT_STREQ(fontFamily(Font::GolosText), "WrenGolos");
  const auto x4Standard = resolve(X4_PPI, Mode::Standard, 0, Font::Literata, "ru");
  EXPECT_EQ(x4Standard.xHeightPx, 12);
  EXPECT_EQ(x4Standard.regularWeight, 360);
  EXPECT_EQ(x4Standard.boldWeight, 665);
}
