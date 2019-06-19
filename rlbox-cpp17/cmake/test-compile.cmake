function(target_compile_test TARGET SHOULD_PASS)
    set_target_properties(${TARGET} PROPERTIES
                      EXCLUDE_FROM_ALL TRUE
                      EXCLUDE_FROM_DEFAULT_BUILD TRUE)

    set(test_name "TestCompile${TARGET}")
    add_test(NAME ${test_name}
         COMMAND ${CMAKE_COMMAND} --build . --target ${TARGET} --config $<CONFIGURATION> --verbose
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

     if (NOT SHOULD_PASS)
         set_tests_properties(${test_name} PROPERTIES WILL_FAIL TRUE)
         set_property(TEST ${test_name} PROPERTY TIMEOUT_AFTER_MATCH "6" ": error:")
     endif()
endfunction()
