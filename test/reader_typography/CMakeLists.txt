add_executable(ReaderTypographyTest ReaderTypographyTest.cpp ../../src/wren/typography/ReaderTypography.cpp)
target_link_libraries(ReaderTypographyTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(ReaderTypographyTest)
