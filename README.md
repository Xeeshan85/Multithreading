# 2-Player Game with SFML

A 2-player game implemented in C++ using SFML graphics library and multi-threading capabilities.

 - The game board is represented as a two-dimensional grid of squares. Each square can either be empty or contain a coin.
 - Players can move around and collect coins.
 - Coins appear randomly within certain time threshold.
 - The player with maximum coins wins.
 - Multiple players can move and collect items concurrently using mutlithreading.

## Demo

[MAGAFIGHT.webm](https://github.com/user-attachments/assets/31a601d0-c7e2-4413-9587-f40a641849b6)

## Prerequisites

### System Requirements
- Linux-based operating system
- C++ compiler (g++)
- Git (for cloning the repository)

### Required Libraries
1. SFML (Simple and Fast Multimedia Library)
2. pthread
3. X11
4. tinyxml2

### Installing Dependencies

On Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install libsfml-dev
sudo apt-get install libx11-dev
sudo apt-get install libtinyxml2-dev
```

On Fedora:
```bash
sudo dnf install SFML-devel
sudo dnf install libX11-devel
sudo dnf install tinyxml2-devel
```

## Installation

1. Clone the repository:
```bash

git clone https://github.com/Xeeshan85/Multithreading
cd Multithreading
```

2. Compile the code:
```bash
g++ -std=c++11 main.cpp -o prog -lsfml-graphics -lsfml-window -lsfml-system -pthread -lX11 -ltinyxml2
```

## Running the Game

1. Start the game:
```bash
./prog
```

2. The game will launch in a new window.

## Controls

### Player 1
- `W` - Move Up
- `S` - Move Down
- `A` - Move Left
- `D` - Move Right

### Player 2
- `↑` - Move Up
- `↓` - Move Down
- `←` - Move Left
- `→` - Move Right

## Troubleshooting

If you encounter any issues:

1. Ensure all dependencies are properly installed:
```bash
ldconfig -p | grep sfml
ldconfig -p | grep X11
ldconfig -p | grep tinyxml2
```

2. Check compiler errors:
- Make sure you have g++ installed: `g++ --version`
- Verify SFML installation: `pkg-config --modversion sfml-all`

3. Common Issues:
   - "Library not found": Make sure all required libraries are installed
   - "Permission denied": Use `chmod +x prog` to make the executable file runnable

## Contributing

Feel free to fork the repository and submit pull requests for any improvements.
