add_library(mach-cpp
  bootstrap.cpp
  bootstrap.hpp
  coding.hpp
  coding.cpp
  fileport.hpp
  fileport.cpp
  message.cpp
  message.hpp
  message_handler.hpp
  port.hpp
  server.cpp
  server.hpp
  server_internal.hpp
)

target_compile_features(mach-cpp PUBLIC cxx_std_17)
target_link_libraries(mach-cpp PUBLIC mcom)
target_include_directories(mach-cpp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
