FROM alpine AS builder

# Check required arguments exist. These will be provided by the Github Action
# Workflow and are required to ensure the correct branches are being used.
ARG DACTARI_BRANCH
RUN test -n "$DACTARI_BRANCH"

RUN apk -U add \
        autoconf \
        automake \
        build-base \
        git \
        alsa-lib-dev

##### Build  #####
WORKDIR /dactari
COPY . .
RUN git checkout "$DACTARI_BRANCH"
RUN autoreconf -i 
RUN ./configure
RUN make
WORKDIR /
##### Built #####

# dactari runtime
FROM alpine

RUN apk -U add \
        alsa-lib

# Copy build files.
COPY --from=builder /dactari/dactari /

# Remove anything we don't need.
RUN rm -rf /lib/apk/db/*

Entrypoint ["/dactari"]