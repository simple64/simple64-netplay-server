FROM golang:1.20 as builder
WORKDIR /workspace

COPY go.mod go.mod
COPY go.sum go.sum
RUN go mod download
COPY main.go main.go
COPY internal/ internal/

RUN CGO_ENABLED=0 go build -a -o simple64-netplay-server main.go

FROM registry.access.redhat.com/ubi9/ubi-micro:latest
WORKDIR /

COPY --from=builder /workspace/simple64-netplay-server .

ENTRYPOINT ["/simple64-netplay-server"]
