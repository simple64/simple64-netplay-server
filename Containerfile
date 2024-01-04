FROM golang:1.21 as builder
WORKDIR /workspace

COPY . .
RUN go mod download
RUN CGO_ENABLED=0 go build -a -o simple64-netplay-server .

FROM registry.access.redhat.com/ubi9/ubi-minimal:latest
WORKDIR /

COPY --from=builder /workspace/simple64-netplay-server .

ENTRYPOINT ["/simple64-netplay-server"]
