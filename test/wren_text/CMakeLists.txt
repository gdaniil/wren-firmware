# Use the same host renderer and real hyphenator as the dash regression suite.
get_target_property(wren_text_sources EmDashLineBreakingTest SOURCES)
list(FILTER wren_text_sources EXCLUDE REGEX "(EmDashLineBreakingTest|FocusReadingTest)\\.cpp$")
add_executable(WrenTextTest WrenTextTest.cpp ${wren_text_sources})
get_target_property(wren_text_includes EmDashLineBreakingTest INCLUDE_DIRECTORIES)
target_include_directories(WrenTextTest PRIVATE ${wren_text_includes})
target_link_libraries(WrenTextTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(WrenTextTest)
