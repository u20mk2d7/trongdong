#!/usr/bin/env bash
set -euo pipefail

cd ~/repos/blustone/.env

# Generate the private key
openssl genpkey -algorithm ed25519 -out test-prv-key.pem

# Generate the public key in PEM format
openssl pkey -pubout -in test-prv-key.pem -out test-pub-key.pem

cat test-pub-key.pem