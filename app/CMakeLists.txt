set(BASE "${CMAKE_CURRENT_SOURCE_DIR}")

set(
    SOURCES
    "${BASE}/build.cpp"
    "${BASE}/entwine.cpp"
    "${BASE}/info.cpp"
    "${BASE}/merge.cpp"
    # "${BASE}/scan.cpp"
)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

add_executable(app ${SOURCES})
compiler_options(app)
add_dependencies(app entwine)

target_link_libraries(app
    PRIVATE
        entwine
        ${PDAL_LIBRARIES}
        ${CMAKE_DL_LIBS}
        ${CMAKE_THREAD_LIBS_INIT}
        pdalcpp
        OpenSSL::applink
        OpenSSL::Crypto
)
if (CURL_FOUND)
    target_link_libraries(app
        PRIVATE
            CURL::libcurl
    )
endif()



if (DEFINED BACKTRACE_DEFS)
  target_link_libraries(app PRIVATE ${Backtrace_LIBRARIES})
endif (DEFINED BACKTRACE_DEFS)

# Named for the fork so it can live beside a conda-installed upstream entwine
# without shadowing it. Every upstream subcommand still works.
set_target_properties(app PROPERTIES OUTPUT_NAME entwine2tiles)

install(TARGETS app DESTINATION bin)
