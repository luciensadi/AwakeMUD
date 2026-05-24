#!/bin/bash
set -e

MUD_PORT="${MUD_PORT:-4000}"

# Generate config files if missing (src/ is volume-mounted from host)
if [ ! -f src/mysql_config.cpp ]; then
    echo "Generating src/mysql_config.cpp for Docker..."
    cat > src/mysql_config.cpp <<'CPP'
const char *mysql_host =     "db";
const char *mysql_password = "awake_dev_password";
const char *mysql_user =     "AwakeMUD";
const char *mysql_db =       "AwakeMUD";
CPP
fi

if [ ! -f src/github_config.cpp ]; then
    echo "Generating src/github_config.cpp..."
    cat > src/github_config.cpp <<'CPP'
const char *github_issues_url = "";
const char *github_authentication = "";
CPP
fi

mkdir -p bin

# Clean stale .d dependency files — container and host generate these with different
# absolute paths, and the shared volume mount means they cross-contaminate.
rm -f src/*.d

# Build with Linux flags (overrides host Makefile settings without modifying the file)
echo "=== Building AwakeMUD... ==="
BUILD_START=$(date +%s)
make -C src -j"$(nproc)" \
    CXX="ccache clang++" \
    OS_CPPFLAGS="-Dlinux -DDEBUG_CRYPTO -DNO_CLS -DUSE_MYSQL_8"
echo "=== Build succeeded in $(($(date +%s) - BUILD_START))s ==="

if [ "${DEBUGGER:-0}" = "1" ]; then
    echo "=== lldb-server listening on :2345 — connect with: lldb -o 'gdb-remote host:2345' ==="
    exec lldb-server gdbserver "*:2345" -- bin/awake "$MUD_PORT"
else
    exec bin/awake "$MUD_PORT"
fi
