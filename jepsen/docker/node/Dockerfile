FROM ubuntu:22.04

# Install packages
RUN sed -i "s@/archive.ubuntu.com/@/mirrors.tuna.tsinghua.edu.cn/@g" /etc/apt/sources.list
RUN apt-get update && \
    apt-get -y install \
        openssh-server \
        pwgen \
        && \
mkdir -p /var/run/sshd && \
sed -i "s/UsePrivilegeSeparation.*/UsePrivilegeSeparation no/g" /etc/ssh/sshd_config && \
sed -i "s/PermitRootLogin without-password/PermitRootLogin yes/g" /etc/ssh/sshd_config

# Install Jepsen deps
RUN apt-get update && \
    apt-get -y install \
        apt-transport-https \
        software-properties-common \
        build-essential \
        bzip2 \
        curl \
        faketime \
        iproute2 \
        iptables \
        iputils-ping \
        libzip4 \
        logrotate \
        man \
        man-db \
        net-tools \
        ntpdate \
        psmisc \
        python3 \
        rsyslog \
        sudo \
        tar \
        unzip \
        vim \
        wget \
        tcpdump \
        git \
        cmake \
        automake \
        autoconf \
        libtool \
        libssl-dev \
        zlib1g-dev \
        && \
        apt-get remove -y --purge --auto-remove systemd

RUN git clone https://github.com/longfar-ncy/pikiwidb -b test/jepsen

ADD entrypoint.sh /entrypoint.sh
RUN chmod 0755 /entrypoint.sh

EXPOSE 22
ENTRYPOINT ["/entrypoint.sh"]
