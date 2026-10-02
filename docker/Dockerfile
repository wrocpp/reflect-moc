# reflect-moc build image: GCC 16.2 plus Qt 6.10 (qtbase, qtdeclarative, offscreen QPA).
#
# Build:  docker build -t reflect-moc/gcc16-qt610 docker
# Run:    docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 <cmd>
#
# The Qt download depends on the CPU architecture. Docker sets TARGETARCH
# (arm64 or amd64) and the RUN below picks the matching aqtinstall host and
# arch: linux_arm64 / linux_gcc_arm64 (native on Apple Silicon, Qt 6.7 and
# later) or linux / linux_gcc_64 (x86_64, GitHub-hosted runners). Override with
#   --build-arg QT_HOST=... --build-arg QT_ARCH=...
# Only the arm64 path has been built and run so far.
FROM gcc:16.2

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      python3 python3-venv cmake ninja-build pkg-config \
      libgl1-mesa-dev libegl1 libopengl0 libfontconfig1 libfreetype6 \
      libxkbcommon0 libglib2.0-0 libdbus-1-3 \
 && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/aqt && /opt/aqt/bin/pip install --no-cache-dir aqtinstall

ARG TARGETARCH
ARG QT_HOST=
ARG QT_ARCH=
ARG QT_VERSION=6.10.3
RUN set -eu; \
    deb_arch="${TARGETARCH:-$(dpkg --print-architecture)}"; \
    case "${deb_arch}" in \
      arm64)  host=linux_arm64; arch=linux_gcc_arm64 ;; \
      amd64)  host=linux;       arch=linux_gcc_64 ;; \
      *) echo "unsupported architecture ${deb_arch}" >&2; exit 1 ;; \
    esac; \
    host="${QT_HOST:-$host}"; arch="${QT_ARCH:-$arch}"; \
    echo "Qt ${QT_VERSION} for ${host} / ${arch}"; \
    /opt/aqt/bin/aqt install-qt "${host}" desktop "${QT_VERSION}" "${arch}" -O /opt/Qt \
 && ln -s /opt/Qt/"${QT_VERSION}"/gcc_* /opt/qt \
 && echo "${QT_VERSION}" > /opt/qt/VERSION \
 && ls /opt/qt/lib/cmake /opt/qt/plugins/platforms

ENV QTDIR=/opt/qt \
    PATH=/opt/qt/bin:/opt/qt/libexec:$PATH \
    LD_LIBRARY_PATH=/opt/qt/lib \
    CMAKE_PREFIX_PATH=/opt/qt \
    QT_QPA_PLATFORM=offscreen
