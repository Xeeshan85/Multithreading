#include <SFML/Graphics.hpp>
#include <tinyxml2.h>
#include <pthread.h>
#include <queue>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>
#include <iostream>
#include <X11/Xlib.h> 

#define TOTAL_PLAYERS 2
#define MAX_ITEMS 40
#define GAME_DURATION 60
#define ITEM_SPAWN_INTERVAL 2
#define MAX_CRATES 7

// Structures
struct Item {
    int x, y;
    bool collected;
    sf::Sprite sprite;
    float spawnTime;
    
    Item() : collected(false), spawnTime(0) {}
};

struct Crate {
    int x, y;
    sf::Sprite sprite;
};

struct PlayerData {
    int x, y, score;
    PlayerData() : x(1), y(1), score(0) {}
};

// Message structures for thread communication
struct MoveMessage {
    int playerID;
    int newX;
    int newY;
};

struct GameState {
    std::atomic<bool> gameRunning;
    std::vector<PlayerData> players;
    std::vector<Item> items;
    std::vector<Crate> crates;
    std::queue<MoveMessage> moveQueue;
    float lastItemSpawnTime;
    sf::Text gameOverText;
    sf::Text timerText;
    sf::Text scoreText;
    
    GameState() : gameRunning(true), lastItemSpawnTime(0) {
        players.resize(TOTAL_PLAYERS);
    }
};

struct SubTexture {
    std::string name;
    int x, y, width, height;
};

// Helper functions declarations
bool isPositionOccupied(int x, int y, const std::vector<Crate>& crates);
void generateCrates(std::vector<Crate>& crates, int N, const sf::Texture& crateTexture, int cellSize);
bool trySpawnItem(GameState& gameState, int N, const sf::Texture& itemTexture, int cellSize, float currentTime);
void* playerThread(void* arg);

struct PlayerThreadData {
    int playerNum;
    GameState* gameState;
    int gridSize;
};

std::vector<SubTexture> loadSubTextures(const std::string& xmlFile, const std::string& prefix) {
    std::vector<SubTexture> subTextures;
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(xmlFile.c_str()) != tinyxml2::XML_SUCCESS) {
        std::cerr << "Failed to load XML file!" << std::endl;
        return subTextures;
    }

    tinyxml2::XMLElement* atlas = doc.FirstChildElement("TextureAtlas");
    if (!atlas) return subTextures;

    for (tinyxml2::XMLElement* elem = atlas->FirstChildElement("SubTexture"); elem != nullptr; elem = elem->NextSiblingElement("SubTexture")) {
        SubTexture subTexture;
        subTexture.name = elem->Attribute("name");
        subTexture.x = elem->IntAttribute("x");
        subTexture.y = elem->IntAttribute("y");
        subTexture.width = elem->IntAttribute("width");
        subTexture.height = elem->IntAttribute("height");
        // subTextures.push_back(subTexture);
        
        if (subTexture.name.find(prefix) == 0) {
            subTextures.push_back(subTexture);
        }
    }
    return subTextures;
}

int generateGridSize(int rollNo) {
    srand(time(0));
    int randomNum = 10 + rand() % 90;
    float res = static_cast<float>(rollNo)/ (randomNum * (rollNo % 10));
    res = static_cast<int>(res) % 25;
    if (res < 10) res += 15;
    return static_cast<int>(res);
}

int main() {
    XInitThreads();

    int rollNum = 0615;
    int N = generateGridSize(rollNum);
    int windowSize = 600;
    int cellSize = windowSize/N;
    
    sf::RenderWindow window(sf::VideoMode(windowSize, windowSize), "MAGA FIGHT");
    
    // Load textures
    sf::Texture textureSheet, itemTexture, crateTexture;
    if (!textureSheet.loadFromFile("resourcePack/Spritesheet/sokoban_spritesheet@2.png") ||
        !itemTexture.loadFromFile("item.png") ||
        !crateTexture.loadFromFile("crate.png")) {
        std::cerr << "Failed to load textures!" << std::endl;
        return -1;
    }

    std::vector<SubTexture> groundTextures = loadSubTextures("resourcePack/Spritesheet/sokoban_spritesheet@2.xml", "ground");
    std::vector<SubTexture> blockTextures = loadSubTextures("resourcePack/Spritesheet/sokoban_spritesheet@2.xml", "block");

    // Create sprites for ground and walls
    // std::vector<sf::Sprite> groundSprites;
    // std::vector<sf::Sprite> blockSprites;
    
    // Initialize ground sprite
    std::vector<sf::Sprite> groundSprites;
    for (const auto& subTex : groundTextures) {
        sf::Sprite sprite;
        sprite.setTexture(textureSheet);
        sprite.setTextureRect(sf::IntRect(subTex.x, subTex.y, subTex.width, subTex.height));
        groundSprites.push_back(sprite);
    }

    std::vector<sf::Sprite> blockSprites;
    for (const auto& subTex : blockTextures) {
        sf::Sprite sprite;
        sprite.setTexture(textureSheet);
        sprite.setTextureRect(sf::IntRect(subTex.x, subTex.y, subTex.width, subTex.height));
        blockSprites.push_back(sprite);
    }

    sf::Texture playerTextures[TOTAL_PLAYERS];
    if (!playerTextures[0].loadFromFile("player_03.png")) return EXIT_FAILURE;
    if (!playerTextures[1].loadFromFile("player_06.png")) return EXIT_FAILURE;
    
    
    std::vector<sf::Sprite> playerSprites(TOTAL_PLAYERS);
    for (int i = 0; i < TOTAL_PLAYERS; i++) {
        playerSprites[i].setTexture(playerTextures[i]);  // Assign texture directly
        playerSprites[i].setTextureRect(sf::IntRect(0, 0, 128, 128));
    }

    // Initialize game state
    GameState gameState;
    
    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        std::cerr << "Failed to load font!" << std::endl;
        return -1;
    }

    gameState.timerText.setFont(font);
    gameState.timerText.setCharacterSize(20);
    gameState.timerText.setFillColor(sf::Color::White);
    gameState.timerText.setPosition(10, 10);

    gameState.scoreText.setFont(font);
    gameState.scoreText.setCharacterSize(20);
    gameState.scoreText.setFillColor(sf::Color::White);
    gameState.scoreText.setPosition(10, 40);

    gameState.gameOverText.setFont(font);
    gameState.gameOverText.setCharacterSize(40);
    gameState.gameOverText.setFillColor(sf::Color::White);
    gameState.gameOverText.setPosition(windowSize/4, windowSize/2);

    // Generate initial crates
    generateCrates(gameState.crates, N, crateTexture, cellSize);

    // Initialize and start player threads
    std::vector<PlayerThreadData> threadData(TOTAL_PLAYERS);
    std::vector<pthread_t> playerThreads(TOTAL_PLAYERS);
    
    for (int i = 0; i < TOTAL_PLAYERS; i++) {
        threadData[i].playerNum = i;
        threadData[i].gameState = &gameState;
        threadData[i].gridSize = N;
        
        if (pthread_create(&playerThreads[i], nullptr, playerThread, &threadData[i]) != 0) {
            std::cerr << "Failed to create player thread " << i << std::endl;
            return -1;
        }
    }

    // Set initial player positions
    gameState.players[0].x = 1;
    gameState.players[0].y = 1;
    gameState.players[1].x = N-2;
    gameState.players[1].y = N-2;

    // Game clock
    sf::Clock gameClock;

    int blockSpIndex = rand() % blockTextures.size();
    int groundSpIndex = rand() % (groundTextures.size() - 1);

    // Main game loop
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        float currentTime = gameClock.getElapsedTime().asSeconds();
        float remainingTime = GAME_DURATION - currentTime;

        // Handle game over condition
        if (remainingTime <= 0 && gameState.gameRunning) {
            gameState.gameRunning = false;
            std::string winnerText;
            if (gameState.players[0].score > gameState.players[1].score) {
                winnerText = "Player 1 Wins!\nScore: " + std::to_string(gameState.players[0].score);
            } else if (gameState.players[1].score > gameState.players[0].score) {
                winnerText = "Player 2 Wins!\nScore: " + std::to_string(gameState.players[1].score);
            } else {
                winnerText = "It's a Tie!\nScore: " + std::to_string(gameState.players[0].score);
            }
            gameState.gameOverText.setString(winnerText);
        }

        // Process move messages from player threads
        while (!gameState.moveQueue.empty()) {
            MoveMessage msg = gameState.moveQueue.front();
            gameState.moveQueue.pop();
            
            int newX = gameState.players[msg.playerID].x + msg.newX;
            int newY = gameState.players[msg.playerID].y + msg.newY;
            
            if (newX > 0 && newX < N-1 && newY > 0 && newY < N-1 && 
                !isPositionOccupied(newX, newY, gameState.crates)) {
                gameState.players[msg.playerID].x = newX;
                gameState.players[msg.playerID].y = newY;
                
                for (auto& item : gameState.items) {
                    if (!item.collected && item.x == newX && item.y == newY) {
                        item.collected = true;
                        gameState.players[msg.playerID].score++;
                    }
                }
            }
        }

        // Spawn items periodically
        if (gameState.gameRunning) {
            float timeSinceLastSpawn = currentTime - gameState.lastItemSpawnTime;
            if (timeSinceLastSpawn >= ITEM_SPAWN_INTERVAL) {
                if (trySpawnItem(gameState, N, itemTexture, cellSize, currentTime)) {
                    gameState.lastItemSpawnTime = currentTime;
                }
            }
        }

        // Render
        window.clear();
        
        // Draw ground and walls
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                if (i == 0 || i == N - 1 || j == 0 || j == N - 1) {
                    blockSprites[blockSpIndex].setPosition(j * cellSize, i * cellSize);
                    blockSprites[blockSpIndex].setScale(
                        static_cast<float>(cellSize) / blockSprites[blockSpIndex].getTextureRect().width,
                        static_cast<float>(cellSize) / blockSprites[blockSpIndex].getTextureRect().height
                    );
                    window.draw(blockSprites[blockSpIndex]);
                } else {
                    groundSprites[groundSpIndex].setPosition(j * cellSize, i * cellSize);
                    groundSprites[groundSpIndex].setScale(
                        static_cast<float>(cellSize) / groundSprites[groundSpIndex].getTextureRect().width,
                        static_cast<float>(cellSize) / groundSprites[groundSpIndex].getTextureRect().height
                    );
                    window.draw(groundSprites[groundSpIndex]);
                }
            }
        }

        // Draw crates
        for (const auto& crate : gameState.crates) {
            window.draw(crate.sprite);
        }

        // Draw items
        for (const auto& item : gameState.items) {
            if (!item.collected) {
                window.draw(item.sprite);
            }
        }

        // Draw players if game is running
        if (gameState.gameRunning) {
            for (int i = 0; i < TOTAL_PLAYERS; i++) {
                playerSprites[i].setPosition(gameState.players[i].y * cellSize, gameState.players[i].x * cellSize);
                playerSprites[i].setScale(
                    static_cast<float>(cellSize) / playerSprites[i].getTextureRect().width,
                    static_cast<float>(cellSize) / playerSprites[i].getTextureRect().height
                );
                window.draw(playerSprites[i]);
            }
        }

        // Draw UI
        if (gameState.gameRunning) {
            std::string timerString = "Time: " + std::to_string(static_cast<int>(remainingTime));
            gameState.timerText.setString(timerString);
            window.draw(gameState.timerText);

            std::string scoreString = "P1: " + std::to_string(gameState.players[0].score) + 
                                    " | P2: " + std::to_string(gameState.players[1].score);
            gameState.scoreText.setString(scoreString);
            window.draw(gameState.scoreText);
        } else {
            window.draw(gameState.gameOverText);
        }

        window.display();
    }

    // Clean up threads
    gameState.gameRunning = false;
    for (int i = 0; i < TOTAL_PLAYERS; i++) {
        pthread_join(playerThreads[i], nullptr);
    }

    return 0;
}

// Helper function implementations
bool isPositionOccupied(int x, int y, const std::vector<Crate>& crates) {
    for (const auto& crate : crates) {
        if (crate.x == x && crate.y == y) {
            return true;
        }
    }
    return false;
}

void generateCrates(std::vector<Crate>& crates, int N, const sf::Texture& crateTexture, int cellSize) {
    for (int i = 0; i < MAX_CRATES; i++) {
        Crate crate;
        do {
            crate.x = 1 + (rand() % (N - 2));
            crate.y = 1 + (rand() % (N - 2));
        } while (isPositionOccupied(crate.x, crate.y, crates));

        crate.sprite.setTexture(crateTexture);
        crate.sprite.setTextureRect(sf::IntRect(0, 0, 64, 64));
        crate.sprite.setPosition(crate.y * cellSize, crate.x * cellSize);
        crate.sprite.setScale(
            static_cast<float>(cellSize) / 64,
            static_cast<float>(cellSize) / 64
        );
        
        crates.push_back(crate);
    }
}

bool trySpawnItem(GameState& gameState, int N, const sf::Texture& itemTexture, int cellSize, float currentTime) {
    if (gameState.items.size() >= MAX_ITEMS) {
        return false;
    }

    Item item;
    bool validPosition;
    int attempts = 0;
    const int maxAttempts = 10;

    do {
        validPosition = true;
        item.x = 1 + (rand() % (N - 2));
        item.y = 1 + (rand() % (N - 2));

        if (isPositionOccupied(item.x, item.y, gameState.crates)) {
            validPosition = false;
            continue;
        }

        for (const auto& existingItem : gameState.items) {
            if (!existingItem.collected && existingItem.x == item.x && existingItem.y == item.y) {
                validPosition = false;
                break;
            }
        }

        attempts++;
    } while (!validPosition && attempts < maxAttempts);

    if (validPosition) {
        item.sprite.setTexture(itemTexture);
        item.sprite.setTextureRect(sf::IntRect(0, 0, 64, 64));
        item.sprite.setPosition(item.y * cellSize, item.x * cellSize);
        item.sprite.setScale(
            static_cast<float>(cellSize) / 64,
            static_cast<float>(cellSize) / 64
        );
        item.spawnTime = currentTime;
        gameState.items.push_back(item);
        return true;
    }

    return false;
}

void* playerThread(void* arg) {
    auto* threadData = static_cast<PlayerThreadData*>(arg);
    int playerNum = threadData->playerNum;
    GameState* gameState = threadData->gameState;
    
    while (gameState->gameRunning) {
        bool moved = false;
        
        // Handle player 1 (WASD)
        if (playerNum == 0) {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
                MoveMessage msg = {playerNum, -1, 0};
                gameState->moveQueue.push(msg);
                moved = true;
            }
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
                MoveMessage msg = {playerNum, 1, 0};
                gameState->moveQueue.push(msg);
                moved = true;
            }
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
                MoveMessage msg = {playerNum, 0, -1};
                gameState->moveQueue.push(msg);
                moved = true;
            }
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
                MoveMessage msg = {playerNum, 0, 1};
                gameState->moveQueue.push(msg);
                moved = true;
            }
        }
        // Handle player 2 (Arrow keys)
        else if (playerNum == 1) {
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
                MoveMessage msg = {playerNum, -1, 0};
                gameState->moveQueue.push(msg);
                moved = true;
            }
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {
                MoveMessage msg = {playerNum, 1, 0};
                gameState->moveQueue.push(msg);
                moved = true;
            }
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
                MoveMessage msg = {playerNum, 0, -1};
                gameState->moveQueue.push(msg);
                moved = true;
            }
            else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
                MoveMessage msg = {playerNum, 0, 1};
                gameState->moveQueue.push(msg);
                moved = true;
            }
        }
        
        // Add a small delay after movement to prevent too rapid updates
        if (moved) {
            sf::sleep(sf::milliseconds(100));
        } else {
            // Small sleep to prevent CPU hogging when not moving
            sf::sleep(sf::milliseconds(10));
        }
    }
    
    return nullptr;
}