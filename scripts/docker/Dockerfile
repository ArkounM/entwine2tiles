# The runtime base must be at least as new as the builder's glibc. A conda
# environment ships everything except libc, and this image compiles from source
# rather than installing a conda-forge package built against an old sysroot, so
# the binary carries whatever glibc symbol versions the builder's toolchain used.
# On ubuntu:22.04, which upstream used, that fails at once with
# "version `GLIBC_2.38' not found". 24.04 carries 2.39. If the builder image ever
# moves past that, the smoke test in the Docker workflow catches it.
ARG BASE_IMAGE="ubuntu:24.04"
FROM --platform=$TARGETPLATFORM condaforge/miniforge3:latest as build

ARG TARGETPLATFORM
ARG TARGETARCH
ARG TARGETVARIANT
RUN printf "I'm building for TARGETPLATFORM=${TARGETPLATFORM}" \
    && printf ", TARGETARCH=${TARGETARCH}" \
    && printf ", TARGETVARIANT=${TARGETVARIANT} \n" \
    && printf "With uname -s : " && uname -s \
    && printf "and  uname -m : " && uname -m

RUN mamba install -c conda-forge conda-pack -y

# Two environments, following conda's build/host split.
#
# "entwine2tiles" is the one that ships: runtime libraries only, plus the
# binary we are about to install into it. libpdal-e57 is what makes E57 input
# work, and E57 is most of what this fork is used on.
#
# "build" holds the toolchain and is thrown away with this stage. Nothing is
# compiled against it: CMAKE_PREFIX_PATH points at the runtime environment, so
# the headers and libraries we link against are exactly the ones that ship,
# which removes any chance of a version skew between the two solves.
RUN mamba create -y -n entwine2tiles -c conda-forge \
        libpdal-core libpdal-e57 curl openssl xz
RUN mamba create -y -n build -c conda-forge \
        compilers cmake ninja make

ENV CONDA_ENV_NAME=entwine2tiles
ENV CONDAENV=/opt/conda/envs/entwine2tiles

COPY . /src

RUN conda run --no-capture-output -n build cmake -S /src -B /build -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DBUILD_SHARED_LIBS=ON \
        -DWITH_TESTS=OFF \
        -DCMAKE_PREFIX_PATH=${CONDAENV} \
        -DCMAKE_INSTALL_PREFIX=${CONDAENV} \
        -DCMAKE_INSTALL_RPATH='\$ORIGIN/../lib'
RUN conda run --no-capture-output -n build cmake --build /build --target install

# Prove the binary runs before it is packed, so a broken link shows up here
# rather than in the shipped image.
RUN conda run --no-capture-output -n entwine2tiles entwine2tiles help

RUN conda run -n base conda-pack -n ${CONDA_ENV_NAME} --dest-prefix=/var/task -o /tmp/env.tar && \
     mkdir /venv && cd /venv && tar xf /tmp/env.tar  && \
     rm /tmp/env.tar

FROM --platform=$TARGETPLATFORM ${BASE_IMAGE:?} as al2

ARG BASE_IMAGE
ARG TARGETPLATFORM
ARG TARGETARCH

ENV CONDAENV="/var/task"
ENV CONDA_PREFIX="/var/task"
ENV TARGETPLATFORM="${TARGETPLATFORM}"
COPY --from=build /venv ${CONDAENV}

ENV PROJ_LIB=${CONDAENV}/share/proj
ENV PROJ_NETWORK=TRUE
ENV GDAL_DATA=${CONDAENV}/share/gdal
ENV PATH=$PATH:${CONDAENV}/bin
ENV LD_LIBRARY_PATH=${CONDAENV}/lib

SHELL [ "/bin/bash", "--login", "-c" ]

ENTRYPOINT ["entwine2tiles"]
