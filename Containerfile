FROM quay.io/centos/centos:stream9 AS build
COPY .git .
RUN dnf -y install git wget unzip && \
    wget https://github.com/m64p/m64p-netplay-server/releases/download/$(git describe --tags --abbrev=0)/m64p-netplay-server-linux.zip && \
    unzip /m64p-netplay-server-linux.zip && \
    chmod +x /m64p-netplay-server

FROM quay.io/centos/centos:stream9
COPY --from=build /m64p-netplay-server /m64p-netplay-server
RUN dnf -y install dnf-plugins-core && dnf -y config-manager --set-enabled crb && dnf -y install epel-release epel-next-release && \
    dnf -y update && dnf install -y qt6-qtwebsockets && dnf clean all -y
ENTRYPOINT ["/m64p-netplay-server"]
