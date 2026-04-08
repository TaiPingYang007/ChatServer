# Repository Guidelines

## Project Structure & Module Organization
`src/server/` contains the chat server entrypoint, service layer, database access, Redis integration, and domain models. `src/client/` contains the terminal client. Public headers live under `include/`, organized to mirror the server modules (`include/server/model`, `include/server/db`, `include/server/redis`). Build artifacts are written to `build/` and executables to `bin/`. Experimental or standalone verification code lives in `test/`, with current examples in `test/testjson/` and `test/testmoduo/`.

## Build, Test, and Development Commands
Use the provided script for normal builds:

```bash
./autobuild.sh
```

This recreates `build/`, runs `cmake ..`, and builds with `make -j4`. Manual build flow:

```bash
mkdir -p build && cd build
cmake ..
make -j4
```

Run the compiled programs from `bin/`, for example `./bin/ChatServer 127.0.0.1 6000` or `./bin/ChatClient 127.0.0.1 8000`.

## Coding Style & Naming Conventions
The codebase is C++11-oriented and currently builds with `-Wall -g`. Follow the existing style: 2-space indentation, opening braces on the same line, header guards in all caps, `CamelCase` for classes, `lowerCamelCase` for methods, and underscore-prefixed members such as `_userModel` or `_connMutex`. Keep source and header paths aligned by module. No formatter is configured in-repo, so match surrounding style before submitting changes.

## Testing Guidelines
There is no top-level `ctest` suite yet. Add focused test programs under `test/` and keep names descriptive, such as `testjson.cpp` or `muduo_server.cpp`. Build and run tests explicitly from their directory when applicable. For server changes, verify at minimum: project build succeeds, login/register flows still work, and Redis/MySQL-backed paths do not regress.

## Commit & Pull Request Guidelines
Recent history uses short, direct commit subjects, mostly in Chinese, with occasional conventional prefixes such as `feat:`. Keep subjects imperative and concise, for example `完善readme文件` or `feat: add group chat handler`. PRs should state the problem, summarize the fix, list verification commands, and note any required infrastructure changes.

## Security & Configuration Tips
Local service endpoints are hardcoded in [`src/server/db/db.cpp`](/home/taipingyang/learn/cpp_project/02_cluster_chat_server/ChatServer/src/server/db/db.cpp) and [`src/server/redis/redis.cpp`](/home/taipingyang/learn/cpp_project/02_cluster_chat_server/ChatServer/src/server/redis/redis.cpp). Do not commit real credentials or environment-specific addresses without review.
