add_executable(assignment main.cpp ../external/imgui/imgui.cpp imgui_impl.cpp)
target_link_libraries(assignment glt ${SDL2_LIBRARY} ${OPENGL_LIBRARIES})
install(TARGETS assignment DESTINATION ${FRAMEWORK_INSTALL_DIR})

