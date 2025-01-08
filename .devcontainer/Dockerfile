# Define global arguments
ARG DEBIAN_FRONTEND="noninteractive"
ARG UID=1000
ARG USER=root

# POSIX compatible (Linux/Unix) base image
FROM debian:bookworm-slim

# Import global arguments
ARG DEBIAN_FRONTEND
ARG UID
ARG USER

# Define local arguments

# Create Non-Root User If Needed (CI will use root user)
RUN ["dash", "-c", "\
    if [ \"$USER\" != \"root\" ]; \
    then \
    addgroup \
     --gid ${UID} \
     \"${USER}\" \
 && adduser \
     --disabled-password \
     --gecos \"\" \
     --ingroup \"${USER}\" \
     --uid ${UID} \
     \"${USER}\" \
 && usermod \
     --append \
     --groups \"dialout,plugdev\" \
     \"${USER}\"; \
    fi \
"]
ENV PATH="/home/${USER}/.local/bin:${PATH}"

# Install Prerequisites
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-setup.html#install-prerequisites
RUN ["dash", "-c", "\
    apt-get update --quiet \
 && apt-get install --assume-yes --no-install-recommends --quiet \
     bison \
     build-essential \
     catch2 \
     ccache \
     cmake \
     dfu-util \
     flex \
     gdb \
     git \
     gnupg \
     gperf \
     libffi-dev \
     libssl-dev \
     libusb-1.0-0 \
     nano \
     ninja-build \
     python3 \
     python3-pip \
     python3-setuptools \
     python3-virtualenv \
     python3.11-venv \
     udev \
     usbutils \
     valgrind \
     wget \
 && apt-get clean \
 && apt-get purge \
 && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* \
"]

# Install IDF as non-root user
WORKDIR /home/${USER}/
USER ${USER}

# Get ESP-IDF
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-2-get-esp-idf
RUN ["dash", "-c", "\
    mkdir esp \
 && cd esp/ \
 && git clone --recursive https://github.com/espressif/esp-idf.git --branch v5.4 ./idf  \
"]

# Set up the tools
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-3-set-up-the-tools
ENV IDF_TARGET="esp32s3"
ENV IDF_PATH="/home/${USER}/esp/idf"
ENV IDF_TOOLS_PATH="/home/${USER}/.espressif"

RUN ["dash", "-c", "\
    cd ./esp/idf \
 && ./install.sh ${IDF_TARGET} \
"]

RUN echo "source /home/${USER}/esp/idf/export.sh" >> /home/${USER}/.bashrc
ENV LC_ALL=C