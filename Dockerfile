FROM alpine AS builder

# Check required arguments exist. These will be provided by the Github Action
# Workflow and are required to ensure the correct branches are being used.
ARG DACARONI_BRANCH
RUN test -n "$DACARONI_BRANCH"

RUN apk -U add \
        autoconf \
        automake \
        build-base \
        git \
        alsa-lib-dev

##### Build  #####
WORKDIR /dacaroni
COPY . .
RUN git checkout "$DACARONI_BRANCH"
RUN autoreconf -i 
RUN ./configure
RUN make
WORKDIR /
##### Built #####

# dacaroni runtime
FROM alpine

RUN apk -U add \
        alsa-lib

# Copy build files.
COPY --from=builder /dacaroni/dacaroni /

# Remove anything we don't need.
RUN rm -rf /lib/apk/db/*

ENTRYPOINT ["/dacaroni"]

# Build like this:
# $ docker build --build-arg DACARONI_BRANCH=main -t dacaroni-latest -f Dockerfile .
# Run like this:
# $ docker run --device /dev/snd dacaroni-latest

