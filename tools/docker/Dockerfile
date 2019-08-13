FROM ubuntu:16.04

ENV DEBIAN_FRONTEND noninteractive
ENV HOME=/home/big

RUN apt-get update \
  && apt-get install -yq libssl-dev --no-install-recommends \
  && apt-get install -yq libsodium-dev --no-install-recommends \
  && apt-get install -yq libreadline6-dev --no-install-recommends

VOLUME ["${HOME}"]
COPY entrypoint.sh /sbin/
RUN chmod 755 /sbin/entrypoint.sh
COPY build/bigbang* /usr/bin/
COPY bigbang.conf /bigbang.conf
EXPOSE 9901 9902 9905
ENTRYPOINT ["/sbin/entrypoint.sh"]
CMD ["bigbang"]
