FROM debian:buster
#FROM ubuntu:22.04
#RUN sed -i "s@/archive.ubuntu.com/@/mirrors.tuna.tsinghua.edu.cn/@g" /etc/apt/sources.list

RUN apt-get -y -q update && \
    apt-get install -qqy \
        openjdk-11-jdk \
        libjna-java \
        git \
        gnuplot \
        wget \
        vim \
        graphviz

ENV LEIN_ROOT true
RUN wget https://raw.githubusercontent.com/technomancy/leiningen/stable/bin/lein && \
    mv lein /usr/bin && \
    chmod +x /usr/bin/lein && \
    lein self-install

RUN git clone https://github.com/longfar-ncy/jepsen-pikiwidb

ADD entrypoint.sh /entrypoint.sh
RUN chmod 0755 /entrypoint.sh

EXPOSE 8080
ENTRYPOINT ["/entrypoint.sh"]
