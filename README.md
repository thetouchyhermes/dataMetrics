# dataMetrics

**dataMetrics** is a simple C project created for the Laboratorio2 exam at the Department of Computer Science, University of Pisa (di.unipi.it). The project demonstrates fundamental C programming concepts, focusing on file handling and basic data metric computations.

## Overview

This program analyzes all `.dat` files in a specified directory (and its subdirectories) to calculate:
- The number of numeric values in each file
- The average of these values
- The standard deviation of these values

The application uses a multi-threaded approach with a master-collector architecture:
- The master process searches for `.dat` files and manages the worker threads
- Worker threads process individual files and send results to the collector
- The collector aggregates and displays the results

## Requirements

- Linux/Unix sistem (POSIX-compliant)
- Packages:
   - GCC
   - Make
   - Git
- Required libraries:
  - pthread
  - math

## Setup

1. **Clone the repository:**
   ```sh
   git clone https://github.com/thetouchyhermes/dataMetrics.git
   ```
   ```sh
   cd dataMetrics
   ```
   
2. **Install/check installation of `GCC` and `make` packages**
   ```sh
   sudo apt update
   ```
   ```sh
   sudo apt install build-essential
   ```
   
3. **Compile the code:**
   ```sh
   make
   ```
   
## Usage

Run the program with the following format:
 ```sh
./main <directory> <W>
```
where:
- `<directory>`: The directory to scan for `.dat` files (including subdirectories)
- `<W>`: The number of worker threads to use for processing files

Example:
```sh
./main ./check 5
```

## Project Structure

- `main.c` — Main source code file
- `Makefile` — For automated building
- `testo.pdf` — Project manual and instructions
- `\check` — Test files directory
- `unboundedqueue.c` `unboundedqueue.h` — Prof-provided implementation of the queue data structure
- `sysmacro.h` — Project-related system macros referenced in main
- `util.h` — Project-related utility functions
- `README.md` — this file

## License

This project is for educational purposes and does not currently use a specific open-source license.

---

**Author:** [thetouchyhermes](https://github.com/thetouchyhermes)
