FROM ubuntu:bionic

RUN apt-get update \
  && apt-get install -y software-properties-common \
  && add-apt-repository ppa:josh-bialkowski/tangent \
  && apt-get update

ADD apt-requirements-bionic.txt \
    /var/cache/dockerinit/apt-requirements.txt

RUN apt-get install -y \
      $(tr '\n' ' ' < /var/cache/dockerinit/apt-requirements.txt)



