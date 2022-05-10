FROM quay.io/centos/centos:stream8
RUN dnf -y update && \
    dnf install -y unzip git wget qt5-qtwebsockets && \
    wget https://github.com/m64p/m64p-netplay-server/releases/download/$(git describe --tags --abbrev=0)/m64p-netplay-server-linux.zip && \
    unzip /m64p-netplay-server-linux.zip && \
    dnf remove -y unzip wget git && \
    dnf clean all -y && \
    rm /m64p-netplay-server-linux.zip && \
    chmod +x /m64p-netplay-server
ENTRYPOINT ["/m64p-netplay-server"]
