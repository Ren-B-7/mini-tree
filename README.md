# Mini-Tree

Mini-Tree is a high-performance, multithreaded directory tree visualization tool. It provides a clean, hierarchical overview of your filesystem, supporting various output formats and customization options.

## Project Structure

-   **`src/`**: Core source code, including multithreaded directory traversal (`fs.c`, `threading.c`).
    -   **`include/`**: Header files for modules, types, and synchronization primitives.
-   **`bin/`**: The compiled `tree` executable.
-   **`Makefile`**: Build system configuration supporting Linux/macOS and Windows (MinGW).
-   **`.clang-format` & `.clang-tidy`**: Pre-configured linting and formatting.

## Building and Running

Mini-Tree uses `gcc` and `pthread` for its multithreaded architecture.

1.  **Build:**
    ```bash
    make all
    ```
    The executable will be generated as `bin/tree`.

2.  **Run:**
    ```bash
    ./bin/tree <directory_path> [options]
    ```

3.  **Options:**
    Use `--help` (or standard CLI arguments) to see all supported features like JSON/XML output, file pruning, and link following.

## Multithreading Architecture

Mini-Tree utilizes a producer-consumer architecture to achieve high-performance traversal:
-   **Work Dispatching**: An initial `process_path` call initiates the traversal and pushes the root node to a `DirQueue`.
-   **Worker Pool**: Multiple worker threads (based on available CPU cores) pull directories from the `DirQueue`, traverse them, and potentially push new child directories back into the queue.
-   **Synchronization**: The system employs a custom `DirQueue` with robust atomic `active_count` tracking and per-node spinlocks to minimize mutex contention, ensuring thread-safe, efficient performance even on deep/wide trees.

## License

This project is licensed under the MIT License.
