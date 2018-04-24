add_executable(helium helium.cpp)
set_target_properties(helium PROPERTIES LINK_FLAGS "-static-libstdc++")
target_link_libraries(helium uv.a pthread unwind tcmalloc.a)