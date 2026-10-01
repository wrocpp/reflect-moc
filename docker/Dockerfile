# reflect-moc spike image: GCC 16.2 plus Qt 6.10 (qtbase, qtdeclarative, offscreen QPA).
#
# Build:  docker build -t reflect-moc/gcc16-qt610 docker
# Run:    docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 <cmd>
#
# Qt publishes native linux_arm64 desktop binaries (linux_gcc_arm64) since 6.7,
# so on Apple Silicon this builds natively; pass
# --build-arg QT_HOST=linux --build-arg QT_ARCH=linux_gcc_64 --platform linux/amd64
# for an x86_64 image.
FROM gcc:16.2

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      python3 python3-venv cmake ninja-build pkg-config \
      libgl1-mesa-dev libegl1 libopengl0 libfontconfig1 libfreetype6 \
      libxkbcommon0 libglib2.0-0 libdbus-1-3 \
 && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/aqt && /opt/aqt/bin/pip install --no-cache-dir aqtinstall

ARG QT_HOST=linux_arm64
ARG QT_ARCH=linux_gcc_arm64
ARG QT_SPEC=6.10
RUN QT_VERSION="$(/opt/aqt/bin/aqt list-qt ${QT_HOST} desktop --spec ${QT_SPEC} --latest-version)" \
 && echo "Qt ${QT_VERSION}" \
 && /opt/aqt/bin/aqt install-qt ${QT_HOST} desktop "${QT_VERSION}" ${QT_ARCH} -O /opt/Qt \
 && ln -s /opt/Qt/"${QT_VERSION}"/gcc_* /opt/qt \
 && echo "${QT_VERSION}" > /opt/qt/VERSION \
 && ls /opt/qt/lib/cmake /opt/qt/plugins/platforms

ENV QTDIR=/opt/qt \
    PATH=/opt/qt/bin:/opt/qt/libexec:$PATH \
    LD_LIBRARY_PATH=/opt/qt/lib \
    CMAKE_PREFIX_PATH=/opt/qt \
    QT_QPA_PLATFORM=offscreen
