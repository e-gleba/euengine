find_program(
    clang_doc_exe
    NAMES clang-doc
    DOC
        "clang-doc: generates C/C++ code documentation from source. Install: 'sudo dnf install clang-tools-extra', 'sudo apt install clang-tools-extra', 'brew install llvm', or 'choco install llvm'. Required for 'clang_doc' target."
)

if(clang_doc_exe)
    file(
        GLOB_RECURSE clang_doc_sources
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.cxx"
        "${PROJECT_SOURCE_DIR}/src/*.hpp"
        "${PROJECT_SOURCE_DIR}/src/*.hxx"
    )

    add_custom_target(
        clang_doc
        COMMAND
            "${clang_doc_exe}" ${clang_doc_sources} --format=html
            "--output=${CMAKE_BINARY_DIR}/clang-doc"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        VERBATIM
        COMMENT
            "running clang-doc to generate HTML documentation for all sources"
        USES_TERMINAL
    )
else()
    message(
        NOTICE
        "clang-doc not found. 'clang_doc' target will not be available.\n"
        "install: sudo dnf install clang-tools-extra | sudo apt install clang-tools-extra | brew install llvm | choco install llvm"
    )
endif()
