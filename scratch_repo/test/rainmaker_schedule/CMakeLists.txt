add_executable(RainmakerScheduleTest
  RainmakerScheduleTest.cpp
  ${REPO_ROOT}/src/rainmaker/RainmakerSchedule.cpp
)

target_include_directories(RainmakerScheduleTest PRIVATE
  ${REPO_ROOT}/src
  ${REPO_ROOT}/test/stubs
)

target_link_libraries(RainmakerScheduleTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(RainmakerScheduleTest)
