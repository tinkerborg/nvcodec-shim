# Init-container image: copies libnvcodec-shim.so into the directory given as
# the first argument (default /nvcodec-shim), which a GPU container mounts and
# LD_PRELOADs. Injection is done by a MutatingAdmissionPolicy; see README.
FROM gcc:13 AS build
WORKDIR /src
COPY shim/ .
RUN make

FROM busybox:stable
COPY --from=build /src/libnvcodec-shim.so /libnvcodec-shim.so
COPY install.sh /install.sh
USER 65532:65532
ENTRYPOINT ["/bin/sh", "/install.sh"]
CMD ["/nvcodec-shim"]
