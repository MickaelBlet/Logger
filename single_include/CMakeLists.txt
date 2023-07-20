set(library_project_name "${PROJECT_NAME}")

find_program(PYTHON3 "python3")
find_program(CLANG_FORMAT "clang-format")
if(PYTHON3 AND CLANG_FORMAT)
    get_target_property(library_source_files "${library_project_name}" SOURCES)
    add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/../single_include/blet/logger.h"
        COMMAND ${PYTHON3} ./third/amalgamate/amalgamate.py -c ./single_include/amalgamate_config.json -s . > /dev/null
        COMMAND ${CLANG_FORMAT} -i ./single_include/blet/logger.h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
        DEPENDS
            ${library_source_files}
            "${CMAKE_CURRENT_SOURCE_DIR}/../include/blet/logger.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/../single_include/amalgamate_config.json"
        COMMENT "Generating single_include/blet/logger.h"
    )
    add_custom_target("${library_project_name}_single_include"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/../single_include/blet/logger.h"
    )
    if(BUILD_TESTING)
        add_subdirectory(test)
    endif()
    add_dependencies("${library_project_name}" "${library_project_name}_single_include")
endif()