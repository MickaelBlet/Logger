set(library_project_name "${PROJECT_NAME}")

# get_target_property(library_include_dirs "${library_project_name}" INCLUDE_DIRECTORIES)

set(example_files
    "${CMAKE_CURRENT_SOURCE_DIR}/quickstart.cpp"
    # "${CMAKE_CURRENT_SOURCE_DIR}/parseData.cpp"
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
            COMPILE_FLAGS "-O2 -pedantic -pthread -Wall -Wextra"
            INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../single_include"
            LINK_LIBRARIES "pthread"
            COMPILE_DEFINITIONS "LOGGER_PERF_DEBUG=1;LOGGER_DROP_OVERFLOW=1"
    )
    add_dependencies("${filenamewe}.${library_project_name}.example" "${library_project_name}_single_include")
endforeach()