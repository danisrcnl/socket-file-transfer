# Socket based file transfer (C)

## Overview

The repository contains 3 modules:
- **client1**, module capable of asking server for a file (or more)
- **server1**, server capable of sending a file (or more) contained in its directory (only supports one connection each time)
- **server2**, server capable of sending a file (or more) contained in its directory (supports more than one connection each time)

## Installation

Compile the modules you need using gcc and linking the libraries contained in the source folder (empty, errlib and sockwrap).

## Usage

Put the files you want to make available in the same folder where the server sources are (e.g., into server1 or server2) and start it.
```bash
./server1 1500
```
where 1500 is the port number the server will listen on (you can use any valid port number you prefer).

Then start the client too and ask for the files you want to download
```bash
./client1 0.0.0.0 1500 file.txt
```
You must be into a local network to make it work, 0.0.0.0 is the address you should use if you start both the client and the server on the same pc, otherwise use the addresses you can see launching ifconfig in your linux terminal.

## Compatibility

Supports both Linux and Mac distributions.
