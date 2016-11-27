#!/bin/bash -xe

WIN_SIGN_KEY="itch corp."
WIN_SIGN_URL="http://timestamp.comodoca.com/"

vendor/signtool.exe sign //v //s MY //n "$WIN_SIGN_KEY" //fd sha256 //tr "$WIN_SIGN_URL?td=sha256" //td sha256 "$1"
