#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")"
mvn clean verify
java -jar target/remote-server-0.1.0.jar
