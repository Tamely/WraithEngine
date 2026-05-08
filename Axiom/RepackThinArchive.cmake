if(NOT DEFINED INPUT_ARCHIVE OR NOT DEFINED OUTPUT_ARCHIVE)
  message(FATAL_ERROR
    "INPUT_ARCHIVE and OUTPUT_ARCHIVE must both be provided.")
endif()

if(NOT EXISTS "${INPUT_ARCHIVE}")
  message(FATAL_ERROR
    "Thin archive does not exist: ${INPUT_ARCHIVE}")
endif()

get_filename_component(INPUT_ARCHIVE_DIR "${INPUT_ARCHIVE}" DIRECTORY)
get_filename_component(OUTPUT_ARCHIVE_DIR "${OUTPUT_ARCHIVE}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_ARCHIVE_DIR}")

execute_process(
  COMMAND /usr/bin/nm "${INPUT_ARCHIVE}"
  OUTPUT_VARIABLE NM_OUTPUT
  ERROR_VARIABLE NM_ERROR
  RESULT_VARIABLE NM_RESULT
)

if(NOT NM_RESULT EQUAL 0)
  message(FATAL_ERROR
    "Failed to inspect thin archive ${INPUT_ARCHIVE}: ${NM_ERROR}")
endif()

string(REPLACE "\r\n" "\n" NM_OUTPUT "${NM_OUTPUT}")
string(REPLACE "\r" "\n" NM_OUTPUT "${NM_OUTPUT}")
string(REPLACE "\n" ";" NM_LINES "${NM_OUTPUT}")

set(OBJECT_FILES)
foreach(LINE IN LISTS NM_LINES)
  if(LINE MATCHES "^([^ ].+):$")
    set(MEMBER_PATH "${CMAKE_MATCH_1}")
    if(IS_ABSOLUTE "${MEMBER_PATH}")
      set(OBJECT_PATH "${MEMBER_PATH}")
    else()
      set(OBJECT_PATH "${INPUT_ARCHIVE_DIR}/${MEMBER_PATH}")
    endif()
    if(NOT EXISTS "${OBJECT_PATH}")
      message(FATAL_ERROR
        "Thin archive member was not found on disk: ${OBJECT_PATH}")
    endif()
    list(APPEND OBJECT_FILES "${OBJECT_PATH}")
  endif()
endforeach()

list(REMOVE_DUPLICATES OBJECT_FILES)

if(NOT OBJECT_FILES)
  message(FATAL_ERROR
    "No object files were discovered in thin archive: ${INPUT_ARCHIVE}")
endif()

set(OBJECT_FILELIST "${OUTPUT_ARCHIVE}.filelist")
file(WRITE "${OBJECT_FILELIST}" "")
foreach(OBJECT_FILE IN LISTS OBJECT_FILES)
  file(APPEND "${OBJECT_FILELIST}" "${OBJECT_FILE}\n")
endforeach()

execute_process(
  COMMAND /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/libtool
          -static
          -o "${OUTPUT_ARCHIVE}"
          -filelist "${OBJECT_FILELIST}"
  RESULT_VARIABLE LIBTOOL_RESULT
  OUTPUT_VARIABLE LIBTOOL_OUTPUT
  ERROR_VARIABLE LIBTOOL_ERROR
)

if(NOT LIBTOOL_RESULT EQUAL 0)
  message(FATAL_ERROR
    "Failed to repack ${INPUT_ARCHIVE} into ${OUTPUT_ARCHIVE}: ${LIBTOOL_OUTPUT}\n${LIBTOOL_ERROR}")
endif()
