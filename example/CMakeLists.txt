set(library_project_name "${PROJECT_NAME}")

get_target_property(library_include_dirs "${library_project_name}" INCLUDE_DIRECTORIES)

set(example_files
    # "${CMAKE_CURRENT_SOURCE_DIR}/dump.cpp"
    # "${CMAKE_CURRENT_SOURCE_DIR}/parseData.cpp"
    # "${CMAKE_CURRENT_SOURCE_DIR}/parseFile.cpp"
    # "${CMAKE_CURRENT_SOURCE_DIR}/parseStream.cpp"
    # "${CMAKE_CURRENT_SOURCE_DIR}/parseString.cpp"
    # "${CMAKE_CURRENT_SOURCE_DIR}/quickstart.cpp"
)

foreach(file ${example_files})
    get_filename_component(filenamewe "${file}" NAME_WE)
    add_executable("${filenamewe}.${library_project_name}.example" "${file}")
    set_target_properties("${filenamewe}.${library_project_name}.example"
        PROPERTIES
            CXX_STANDARD "${CMAKE_CXX_STANDARD}"
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
            NO_SYSTEM_FROM_IMPORTED ON
            COMPILE_FLAGS "-pedantic -Wall -Wextra -Werror"
            INCLUDE_DIRECTORIES "${library_include_dirs}"
            LINK_LIBRARIES "${library_project_name}"
    )
endforeach()