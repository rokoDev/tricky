include(FetchContent)

# Setup directory to which dependencies will be downloaded
cmake_path(APPEND FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/../" "deps_content" "${CMAKE_GENERATOR_NAME_WITHOUT_SPACES}")

# If dependencies are not available then download it and make available
FetchContent_Declare(
	user_literals
	GIT_REPOSITORY https://github.com/rokoDev/user_literals.git
	GIT_TAG        5c62773ff93b21cc5868eb3c7377deb9a8d8ec97
)

FetchContent_Declare(
	interval
	GIT_REPOSITORY https://github.com/rokoDev/interval.git
	GIT_TAG        f1ef357b5b41ade09de06b4faacd5aa6287dd1d7
)

FetchContent_Declare(
	utils
	GIT_REPOSITORY https://github.com/rokoDev/utils.git
	GIT_TAG        a52b8c1cb964f7f478522cbe78027141a0dfc649
)

FetchContent_Declare(
	type_name
	GIT_REPOSITORY https://github.com/rokoDev/type_name.git
	GIT_TAG        fe39b753ddad0d034c10b45c2de26f77f180d591
)

# If necessary it's possible to use VersionHelper from different location instead of downloaded from GitHub
# string(TOUPPER VersionHelper UP_VersionHelper)
# set(FETCHCONTENT_SOURCE_DIR_${UP_VersionHelper} ${CMAKE_SOURCE_DIR}/../VersionHelper)

FetchContent_MakeAvailable(user_literals interval utils type_name)
target_link_libraries(tricky INTERFACE user_literals interval utils type_name)

# If use IDE add strong_type target into deps source group
set_target_properties(user_literals interval utils type_name PROPERTIES FOLDER deps)
