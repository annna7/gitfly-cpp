cmake_minimum_required(VERSION 3.20)
project(gitfly LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Lint: enable clang-tidy if available
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if (CLANG_TIDY_EXE)
    message(STATUS "clang-tidy: ${CLANG_TIDY_EXE}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-p=${CMAKE_BINARY_DIR}")
endif ()


# Dependencies
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED Crypto)  # request Crypto component

# Library with core plumbing
add_library(gitfly_lib
        src/repo.cpp
        src/object_store.cpp
        src/diff.cpp
        src/remote.cpp
        src/tcp_remote.cpp
        src/index.cpp
        src/time.cpp
        src/refs.cpp
        src/status.cpp
        src/worktree.cpp
        src/config.cpp
        src/fs.cpp
        src/hash_sha1.cpp
        src/util.cpp
)
target_include_directories(gitfly_lib PUBLIC include)
target_link_libraries(gitfly_lib PUBLIC ZLIB::ZLIB OpenSSL::Crypto)

# CLI executable (porcelain)
add_executable(gitfly
        src/main.cpp
        src/cli/registry.cpp
        src/cli/register_commands.cpp
        src/cli/commands/init.cpp
        src/cli/commands/add.cpp
        src/cli/commands/commit.cpp
        src/cli/commands/status.cpp
        src/cli/commands/checkout.cpp
        src/cli/commands/branch.cpp
        src/cli/commands/log.cpp
        src/cli/commands/merge.cpp
        src/cli/commands/diff.cpp
        src/cli/commands/clone.cpp
        src/cli/commands/push.cpp
        src/cli/commands/fetch.cpp
        src/cli/commands/pull.cpp
        src/cli/commands/serve.cpp
)
target_include_directories(gitfly PRIVATE include src)
target_link_libraries(gitfly PRIVATE gitfly_lib)

# CTest
enable_testing()

add_executable(gitfly_repo_test tests/repository.cpp)
target_link_libraries(gitfly_repo_test PRIVATE gitfly_lib)
add_test(NAME gitfly_repo COMMAND gitfly_repo_test)


add_executable(gitfly_init_test tests/init.cpp)
target_link_libraries(gitfly_init_test PRIVATE gitfly_lib)
add_test(NAME gitfly_init COMMAND gitfly_init_test)

add_executable(gitfly_index_tree_test tests/index_write_tree.cpp)
target_link_libraries(gitfly_index_tree_test PRIVATE gitfly_lib)
add_test(NAME gitfly_index_tree COMMAND gitfly_index_tree_test)

add_executable(gitfly_commit_test tests/commit_from_index.cpp)
target_link_libraries(gitfly_commit_test PRIVATE gitfly_lib)
add_test(NAME gitfly_commit_from_index COMMAND gitfly_commit_test)

add_executable(gitfly_status_test tests/status.cpp)
target_link_libraries(gitfly_status_test PRIVATE gitfly_lib)
add_test(NAME gitfly_status COMMAND gitfly_status_test)

add_executable(gitfly_branch_log_test tests/branch_log.cpp)
target_link_libraries(gitfly_branch_log_test PRIVATE gitfly_lib)
add_test(NAME gitfly_branch_log COMMAND gitfly_branch_log_test)

add_executable(gitfly_merge_test tests/merge.cpp)
target_link_libraries(gitfly_merge_test PRIVATE gitfly_lib)
add_test(NAME gitfly_merge COMMAND gitfly_merge_test)

add_executable(gitfly_diff_algo_test tests/diff_algo.cpp)
target_link_libraries(gitfly_diff_algo_test PRIVATE gitfly_lib)
add_test(NAME gitfly_diff_algo COMMAND gitfly_diff_algo_test)

add_executable(gitfly_remote_fs_test tests/remote_fs.cpp)
target_link_libraries(gitfly_remote_fs_test PRIVATE gitfly_lib)
add_test(NAME gitfly_remote_fs COMMAND gitfly_remote_fs_test)

add_executable(gitfly_merge_ff_test tests/merge_fastforward.cpp)
target_link_libraries(gitfly_merge_ff_test PRIVATE gitfly_lib)
add_test(NAME gitfly_merge_fastforward COMMAND gitfly_merge_ff_test)

add_executable(gitfly_merge_conflict_test tests/merge_conflict.cpp)
target_link_libraries(gitfly_merge_conflict_test PRIVATE gitfly_lib)
add_test(NAME gitfly_merge_conflict COMMAND gitfly_merge_conflict_test)

add_executable(gitfly_remote_pull_test tests/remote_pull.cpp)
target_link_libraries(gitfly_remote_pull_test PRIVATE gitfly_lib)
add_test(NAME gitfly_remote_pull COMMAND gitfly_remote_pull_test)

# Collect sources for fix target
file(GLOB_RECURSE ALL_CXX_SRC CONFIGURE_DEPENDS
        src/*.cpp include/*.hpp src/**/*.cpp src/**/*.hpp)

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if (CLANG_TIDY_EXE)
    add_custom_target(tidy-fix
            COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} -fix -format-style=file
            $<JOIN:${ALL_CXX_SRC}, >
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-tidy with auto-fix on all sources"
    )
endif ()
