# VCG OS Scheduler

A distributed Operating System scheduler simulation written in C++ that allocates CPU time based on the **Vickrey-Clarke-Groves (VCG)** economic auction model. 

This project demonstrates deep systems-level programming, utilizing custom UDP network protocols for Inter-Process Communication (IPC) and direct manipulation of Linux kernel process states to execute context switches.

## Core Features

* **Kernel Hijacking (Context Switching):** Uses POSIX signals (`SIGSTOP`, `SIGCONT`) to dynamically freeze and resume background processes, acting as a true OS-level traffic cop.
* **Asynchronous UDP Networking:** Implements a custom non-blocking UDP protocol with `select()` timers to allow independent "bidder" programs to request CPU time across a network.
* **VCG Auction Math:** Calculates the true social cost of process execution. The highest bidder wins the CPU but is charged the *second-highest* bid price.
* **Economic State Management & Starvation Prevention:** Maintains a persistent "Ready Queue" and implements a dynamic "Robin Hood" wealth redistribution algorithm (taxing the winner to fund the losers) to prevent infinite resource starvation and enforce fair-share context switching.

## Project Architecture

* `auctioneer.cpp`: The central scheduler. Listens for UDP bids, calculates the VCG auction, maintains virtual bank accounts, and fires OS signals to context-switch processes.
* `bidder.cpp`: The client processes. They submit varying bids to the auctioneer and simulate workload execution when granted CPU time.
* `protocol.h`: The shared data structures defining the IPC packet payloads.

## How to Build and Run

### Compilation
Ensure you have `g++` and `make` installed on your Linux/WSL environment.
```bash
make
