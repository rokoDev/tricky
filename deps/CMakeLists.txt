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
	GIT_TAG        9621cf141f8503aecf89de6233c0dda4267cb53a
)

FetchContent_Declare(
	utils
	GIT_REPOSITORY https://github.com/rokoDev/utils.git
	GIT_TAG        a7a7472c6f70e2c8ad919a88b8399b2ee2398f7a
)

FetchContent_Declare(
	type_name
	GIT_REPOSITORY https://github.com/rokoDev/type_name.git
	GIT_TAG        05ce55919dc1dc55a3d87100f676d21c28fa42f7
)

FetchContent_Declare(
	strong_type
	GIT_REPOSITORY https://github.com/rokoDev/strong_type.git
	GIT_TAG        b34f11ad2cbb164030a56a80e488cfd716d43b5d
)

FetchContent_Declare(
	cargo
	GIT_REPOSITORY https://github.com/rokoDev/cargo.git
	GIT_TAG        4d9d54567c75bc750125375492e4756d10d6c964
)

# If necessary it's possible to use VersionHelper or utils from different location instead of downloaded from GitHub
# string(TOUPPER VersionHelper UP_VersionHelper)
# set(FETCHCONTENT_SOURCE_DIR_${UP_VersionHelper} ${CMAKE_SOURCE_DIR}/../VersionHelper)
# string(TOUPPER utils UP_utils)
# set(FETCHCONTENT_SOURCE_DIR_${UP_utils} ${CMAKE_SOURCE_DIR}/../utils)

FetchContent_MakeAvailable(user_literals interval utils type_name strong_type cargo)
target_link_libraries(tricky INTERFACE user_literals interval utils type_name strong_type cargo)

# If use IDE add strong_type target into deps source group
set_target_properties(user_literals interval utils type_name strong_type cargo PROPERTIES FOLDER deps)
