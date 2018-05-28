add_executable(helium helium.cpp jpeg2faceid_transfer.cpp jpeg2faceid_transfer.h)
set_target_properties(helium PROPERTIES LINK_FLAGS "-static-libstdc++")
target_link_libraries(
        helium uv.a pthread unwind arcsoft_fsdk_face_recognition
        arcsoft_fsdk_face_detection turbojpeg.a
        tcmalloc.a
)