FROM quay.io/centos/centos:stream9 AS build
COPY . .
RUN dnf -y install dnf-plugins-core && \
    dnf config-manager --set-enabled crb && \
    dnf -y install epel-release epel-next-release && \
    dnf -y update && \
    dnf -y install cmake ninja-build qt6-qtwebsockets-devel && \
    mkdir build && \
    cd build && \
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .. && \
    VERBOSE=1 cmake --build .

FROM quay.io/centos/centos:stream9-minimal
COPY --from=build build/simple64-netplay-server /simple64-netplay-server
RUN microdnf install epel-release epel-next-release && \
    microdnf -y upgrade && \
    microdnf install -y qt6-qtwebsockets && \
    microdnf clean all -y
ENTRYPOINT ["/simple64-netplay-server"]
