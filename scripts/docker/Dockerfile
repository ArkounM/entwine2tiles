# The runtime base must carry a glibc at least as new as the builder's. This
# image compiles from source, so the binary keeps the builder toolchain's glibc
# symbol versions: on ubuntu:22.04, which upstream used, it dies at once with
# "version `GLIBC_2.38' not found". 24.04 has 2.39.
ARG BASE_IMAGE="ubuntu:24.04"
FROM condaforge/miniforge3:latest AS build

# Two environments, following conda's build/host split. entwine2tiles is the one
# that ships, runtime libraries only, and libpdal-e57 is the plugin that makes
# E57 input work. build holds the toolchain and dies with this stage.
# CMAKE_PREFIX_PATH points at the runtime environment, so what gets linked
# against is exactly what ships.
ENV CONDA_ENV_NAME=entwine2tiles
ENV ENVDIR=/opt/conda/envs/${CONDA_ENV_NAME}

RUN mamba create -y -n ${CONDA_ENV_NAME} -c conda-forge \
        libpdal-core libpdal-e57 curl openssl xz
RUN mamba create -y -n build -c conda-forge \
        compilers cmake ninja conda-pack

COPY . /src

# The install rpath is what lets the binary find its libraries after conda-pack
# relocates the environment to /var/task.
RUN conda run --no-capture-output -n build cmake -S /src -B /build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON \
        -DWITH_TESTS=OFF \
        -DCMAKE_PREFIX_PATH=${ENVDIR} \
        -DCMAKE_INSTALL_PREFIX=${ENVDIR} \
        -DCMAKE_INSTALL_RPATH='\$ORIGIN/../lib'
RUN conda run --no-capture-output -n build cmake --build /build --target install

# Prove it runs before packing, so a broken link surfaces here rather than in
# the shipped image.
RUN conda run --no-capture-output -n ${CONDA_ENV_NAME} entwine2tiles help

RUN conda run -n build conda-pack -n ${CONDA_ENV_NAME} --dest-prefix=/var/task -o /tmp/env.tar && \
     mkdir /venv && cd /venv && tar xf /tmp/env.tar  && \
     rm /tmp/env.tar

FROM ${BASE_IMAGE:?} AS al2

ENV CONDAENV="/var/task"
ENV CONDA_PREFIX="/var/task"
COPY --from=build /venv ${CONDAENV}

ENV PROJ_LIB=${CONDAENV}/share/proj
ENV PROJ_NETWORK=TRUE
ENV GDAL_DATA=${CONDAENV}/share/gdal
ENV PATH=$PATH:${CONDAENV}/bin
ENV LD_LIBRARY_PATH=${CONDAENV}/lib

ENTRYPOINT ["entwine2tiles"]
