#!/bin/sh

# upload all artifacts from a single worker
gsutil cp -r -a public-read binaries/* gs://dl.itch.ovh/elevate/
