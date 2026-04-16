cmake_minimum_required(VERSION 3.20)
project(linuxcomplete VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Fcitx5
find_package(Fcitx5Core REQUIRED)
find_package(Fcitx5Module COMPONENTS Notifications QUIET)
find_package(PkgConfig REQUIRED)

# Find nlohmann/json
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)
endif()

# Include Fcitx5 macros
include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake" OPTIONAL)

# Dictionary install path
set(DICT_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/linuxcomplete/dict")
set(FREQ_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/linuxcomplete/frequency")
set(CONFIG_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/linuxcomplete/config")
set(RULES_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/linuxcomplete/rules")
set(EMOJI_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/linuxcomplete/emoji")

add_definitions(
    -DDICT_INSTALL_DIR="${DICT_INSTALL_DIR}"
    -DFREQ_INSTALL_DIR="${FREQ_INSTALL_DIR}"
    -DCONFIG_INSTALL_DIR="${CONFIG_INSTALL_DIR}"
    -DRULES_INSTALL_DIR="${RULES_INSTALL_DIR}"
    -DLINUXCOMPLETE_VERSION="${PROJECT_VERSION}"
)

# Predictor library
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(linuxcomplete_predictor STATIC
    src/predictor/trie.cpp
    src/predictor/predictor.cpp
    src/predictor/ai_reranker.cpp
    src/predictor/dictionary.cpp
    src/predictor/ngram.cpp
    src/predictor/grammar_rules.cpp
    src/predictor/phrase.cpp
    src/predictor/emoji.cpp
    src/predictor/text_utils.cpp
)
target_include_directories(linuxcomplete_predictor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(nlohmann_json_FOUND)
    target_link_libraries(linuxcomplete_predictor PUBLIC nlohmann_json::nlohmann_json)
else()
    target_include_directories(linuxcomplete_predictor PUBLIC ${NLOHMANN_JSON_INCLUDE_DIRS})
endif()

# Fcitx5 addon
add_library(linuxcomplete MODULE
    src/engine/engine.cpp
    src/engine/engine_config.cpp
)
target_link_libraries(linuxcomplete PRIVATE
    Fcitx5::Core
    Fcitx5::Module::Notifications
    linuxcomplete_predictor
)
target_include_directories(linuxcomplete PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Install targets
set_target_properties(linuxcomplete PROPERTIES PREFIX "")

# Install the addon library
install(TARGETS linuxcomplete
    LIBRARY DESTINATION "${FCITX_INSTALL_ADDONDIR}"
)

# Install addon config
install(FILES config/linuxcomplete-addon.conf
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon"
    RENAME linuxcomplete.conf
)

# Install input method config
install(FILES config/linuxcomplete-im.conf
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod"
    RENAME linuxcomplete.conf
)

# Install dictionaries
install(DIRECTORY data/dict/
    DESTINATION "${DICT_INSTALL_DIR}"
    FILES_MATCHING PATTERN "*.txt"
)
install(DIRECTORY data/frequency/
    DESTINATION "${FREQ_INSTALL_DIR}"
    FILES_MATCHING PATTERN "*.txt"
)

# Install ngram and phrase data
set(NGRAM_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/linuxcomplete/ngram")
install(DIRECTORY data/ngram/
    DESTINATION "${NGRAM_INSTALL_DIR}"
    FILES_MATCHING PATTERN "*.txt"
)

install(DIRECTORY data/rules/
    DESTINATION "${RULES_INSTALL_DIR}"
    FILES_MATCHING PATTERN "*.txt"
)

# Install emoji data
install(DIRECTORY data/emoji/
    DESTINATION "${EMOJI_INSTALL_DIR}"
    FILES_MATCHING PATTERN "*.tsv"
)

# Install default config
install(FILES config/linuxcomplete.conf
    DESTINATION "${CONFIG_INSTALL_DIR}"
)

message(STATUS "LinuxComplete v${PROJECT_VERSION}")
message(STATUS "  Addon dir: ${FCITX_INSTALL_ADDONDIR}")
message(STATUS "  Dict dir:  ${DICT_INSTALL_DIR}")

enable_testing()

# ── Tests: link against the predictor library instead of re-compiling sources ──

add_executable(linuxcomplete_english_grammar_test tests/english_grammar_sanity.cpp)
target_link_libraries(linuxcomplete_english_grammar_test PRIVATE linuxcomplete_predictor)
target_compile_definitions(linuxcomplete_english_grammar_test PRIVATE
    RULES_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/rules"
)
add_test(NAME linuxcomplete_english_grammar_sanity COMMAND linuxcomplete_english_grammar_test)

add_executable(linuxcomplete_predictor_learning_test tests/predictor_learning_regression.cpp)
target_link_libraries(linuxcomplete_predictor_learning_test PRIVATE linuxcomplete_predictor)
add_test(NAME linuxcomplete_predictor_learning_regression COMMAND linuxcomplete_predictor_learning_test)

add_executable(linuxcomplete_predictor_quality_test tests/predictor_quality_regression.cpp)
target_link_libraries(linuxcomplete_predictor_quality_test PRIVATE linuxcomplete_predictor)
add_test(NAME linuxcomplete_predictor_quality_regression COMMAND linuxcomplete_predictor_quality_test)

add_executable(linuxcomplete_ai_reranker_test tests/ai_reranker_regression.cpp)
target_link_libraries(linuxcomplete_ai_reranker_test PRIVATE linuxcomplete_predictor)
add_test(NAME linuxcomplete_ai_reranker_regression COMMAND linuxcomplete_ai_reranker_test)

add_executable(linuxcomplete_data_quality_test tests/data_quality_regression.cpp)
target_link_libraries(linuxcomplete_data_quality_test PRIVATE linuxcomplete_predictor)
add_test(NAME linuxcomplete_data_quality_regression COMMAND linuxcomplete_data_quality_test)

# Unit tests for extracted modules (text_utils, grammar_rules index, emoji index).
add_executable(linuxcomplete_text_utils_test tests/text_utils_test.cpp)
target_link_libraries(linuxcomplete_text_utils_test PRIVATE linuxcomplete_predictor)
add_test(NAME linuxcomplete_text_utils COMMAND linuxcomplete_text_utils_test)

add_executable(linuxcomplete_grammar_index_test tests/grammar_index_test.cpp)
target_link_libraries(linuxcomplete_grammar_index_test PRIVATE linuxcomplete_predictor)
target_compile_definitions(linuxcomplete_grammar_index_test PRIVATE
    RULES_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/rules"
)
add_test(NAME linuxcomplete_grammar_index COMMAND linuxcomplete_grammar_index_test)

add_executable(linuxcomplete_emoji_index_test tests/emoji_index_test.cpp)
target_link_libraries(linuxcomplete_emoji_index_test PRIVATE linuxcomplete_predictor)
target_compile_definitions(linuxcomplete_emoji_index_test PRIVATE
    EMOJI_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/emoji"
)
add_test(NAME linuxcomplete_emoji_index COMMAND linuxcomplete_emoji_index_test)

add_executable(linuxcomplete_blocklist_test tests/blocklist_test.cpp)
target_link_libraries(linuxcomplete_blocklist_test PRIVATE linuxcomplete_predictor)
add_test(NAME linuxcomplete_blocklist COMMAND linuxcomplete_blocklist_test)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME linuxcomplete_rules_validation
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/rules_tool.py
            --rules-dir ${CMAKE_CURRENT_SOURCE_DIR}/data/rules
            validate
)
add_test(NAME linuxcomplete_rules_tool_regression
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/rules_tool_regression.py
)
