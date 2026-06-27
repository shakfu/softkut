include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

#############################################################
# MAX EXTERNAL
#############################################################

include_directories(
	"${MAX_SDK_INCLUDES}"
	"${MAX_SDK_MSP_INCLUDES}"
	"${MAX_SDK_JIT_INCLUDES}"
)

set(PROJECT_SRC
    "softkut~.cpp"
)

add_library(
	${PROJECT_NAME}
	MODULE
	${PROJECT_SRC}
)

# softcut + the host-agnostic engine header (SOFTCUT_* set in the top CMakeLists)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_14)
target_include_directories(${PROJECT_NAME} PRIVATE "${SOFTKUT_INCLUDE}" "${SOFTCUT_INCLUDE}")
target_link_libraries(${PROJECT_NAME} PRIVATE softcut)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)
