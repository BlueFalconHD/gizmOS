pathd is responsible for finding executables and managing system path and environment context.

pathd keeps a backing store of active paths for system and loads it into memory on start

then, it accepts several operations:

- pathd_locate(name) will search all path locations for a designated executable
- pathd_add(path) adds a new search location
- pathd_remove(path) removes an existing search location

processes interact with pathd through Lattice™