FROM ubuntu:16.04

ADD 3rd/libarcsoft_fsdk_face_detection.so /usr/lib/
ADD 3rd/libarcsoft_fsdk_face_recognition.so /usr/lib/
ADD helium /usr/bin/

RUN sed -i 's/archive.ubuntu.com/mirrors.aliyun.com/g' /etc/apt/sources.list \
    && apt update && apt install -y libunwind8 libcurl3 libsqlite3-0 iproute
ENTRYPOINT ["/usr/bin/helium"]
