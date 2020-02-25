add_library(mcom
  cf.cpp
  cf.hpp
  codable.hpp
  deferred.hpp
  directory.cpp
  directory.hpp
  disk_name.hpp
  disk_name.cpp
  dispatch.cpp
  dispatch.hpp
  result.hpp
  file.cpp
  file.hpp
  file_path.cpp
  file_path.hpp
  iokit.cpp
  iokit.hpp
  optional.hpp
  process.cpp
  process.hpp
  security.cpp
  security.hpp
  string.cpp
  string.hpp
  sync.hpp
  types.hpp
  utility.hpp
  uuid.cpp
  uuid.hpp
)

target_compile_features(mcom PUBLIC cxx_std_17)
target_include_directories(mcom PUBLIC ${MCOM_SOURCE_DIR})
