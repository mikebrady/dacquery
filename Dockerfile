FROM alpine AS builder

# Check required arguments exist. These will be provided by the Github Action
# Workflow and are required to ensure the correct branches are being used.
ARG DACQUERY_BRANCH
RUN test -n "$DACQUERY_BRANCH"

RUN apk -U add \
        autoconf \
        automake \
        build-base \
        git \
        alsa-lib-dev

##### Build  #####
WORKDIR /dacquery
COPY . .
RUN git checkout "$DACQUERY_BRANCH"
RUN autoreconf -i 
RUN ./configure
RUN make
WORKDIR /
##### Built #####

# dacquery runtime
FROM alpine

RUN apk -U add \
        alsa-lib

# Copy build files.
COPY --from=builder /dacquery/dacquery /

# Remove anything we don't need.
RUN rm -rf /lib/apk/db/*

ENTRYPOINT ["/dacquery"]

# Build like this:
# $ docker build --build-arg DACQUERY_BRANCH=main -t dacquery-latest -f Dockerfile .
# Run like this:
# $ docker run --device /dev/snd dacquery-latest

