FROM registry.fedoraproject.org/fedora-minimal:36 AS build
COPY .git .
RUN microdnf -y install git wget unzip && \
    wget https://github.com/m64p/m64p-netplay-server/releases/download/$(git describe --tags --abbrev=0)/m64p-netplay-server-linux.zip && \
    unzip /m64p-netplay-server-linux.zip && \
    chmod +x /m64p-netplay-server

FROM registry.fedoraproject.org/fedora-minimal:36
COPY --from=build /m64p-netplay-server /m64p-netplay-server
RUN microdnf -y update && microdnf install -y qt6-qtwebsockets && microdnf clean all -y
ENTRYPOINT ["/m64p-netplay-server"]
