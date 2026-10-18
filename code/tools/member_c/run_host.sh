#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
artifact_root=.
if [ -f ../report/report.md ]; then artifact_root=../report; fi
mkdir -p build/member-c-host "$artifact_root/evidence/member-c"
gcc -std=gnu99 -g -O1 -Wall -Wextra -Werror -Wno-unused-parameter \
  -fno-builtin -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Ilibs -Ikern/mm -Ikern/debug \
  kern/mm/slub.c kern/mm/slub_check.c tools/member_c/host_test.c \
  -o build/member-c-host/slub-test
build/member-c-host/slub-test > "$artifact_root/evidence/member-c/slub-host.log" 2>&1
cat "$artifact_root/evidence/member-c/slub-host.log"
