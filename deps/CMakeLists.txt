include(FetchContent)

# Setup directory to which dependencies will be downloaded
cmake_path(APPEND FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/../" "deps_content" "${CMAKE_GENERATOR_NAME_WITHOUT_SPACES}")

# If dependencies are not available then download it and make available
FetchContent_Declare(
	user_literals
	GIT_REPOSITORY https://github.com/rokoDev/user_literals.git
	GIT_TAG        5c62773ff93b21cc5868eb3c7377deb9a8d8ec97
)

# If necessary it's possible to use VersionHelper from different location instead of downloaded from GitHub
# string(TOUPPER VersionHelper UP_VersionHelper)
# set(FETCHCONTENT_SOURCE_DIR_${UP_VersionHelper} ${CMAKE_SOURCE_DIR}/../VersionHelper)

FetchContent_MakeAvailable(user_literals)
target_link_libraries(tricky INTERFACE user_literals)

# If use IDE add strong_type target into deps source group
set_target_properties(user_literals PROPERTIES FOLDER deps)
