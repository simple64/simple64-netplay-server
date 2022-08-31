FROM registry.fedoraproject.org/fedora-minimal:36 AS build
COPY .git .
RUN microdnf -y install git wget unzip && \
    wget https://github.com/simple64/simple64-netplay-server/releases/download/$(git describe --tags --abbrev=0)/simple64-netplay-server-linux.zip && \
    unzip /simple64-netplay-server-linux.zip && \
    chmod +x /simple64-netplay-server

FROM registry.fedoraproject.org/fedora-minimal:36
COPY --from=build /simple64-netplay-server /simple64-netplay-server
RUN microdnf -y update && microdnf install -y qt6-qtwebsockets && microdnf clean all -y
ENTRYPOINT ["/simple64-netplay-server"]
