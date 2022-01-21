if(BUILD_TESTING)
    add_executable(bbget-test
        main.test.cpp
    )
    target_link_libraries(bbget-test gmock gmock_main bbget-obj)

    target_include_directories(bbget-test PRIVATE
        $<TARGET_PROPERTY:bbget-obj,SOURCE_DIR>
        $<TARGET_PROPERTY:bbget-obj,INCLUDE_DIRECTORIES>
    )

    gtest_discover_tests(bbget-test)
endif()
