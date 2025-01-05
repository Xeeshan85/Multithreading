#include <SFML/Graphics.hpp>
#include <tinyxml2.h>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <ctime>
#include <map>
#include <queue>
#include <memory>
#include <chrono>

#define TOTAL_PLAYERS 2
#define MAX_ITEMS 10
#define GAME_DURATION 60  // Game duration in seconds
#define ITEM_SPAWN_INTERVAL 5  // Spawn new items every 5 seconds
#define MAX_CRATES 5  // Maximum number of crates on the board

struct SubTexture {
    std::string name;
    int x, y, width, height;
};

struct Item {
    int x, y;
    bool collected;
    sf::Sprite sprite;
    float spawnTime;  // Time when item was spawned
    
    Item() : collected(false), spawnTime(0) {}
};

struct Crate {
    int x, y;
    sf::Sprite sprite;
};

struct PlayerData {
    int x, y, score;
    std::queue<std::pair<int, int>> moveQueue;
    PlayerData() : score(0) {}
};

struct PlayerThreadData {
    PlayerData* playerData;
    sf::Event event;
    int gridSize;
    int playerNum;
};

// Message queue for item collection
struct CollectionMessage {
    int playerNum;
    int itemX;
    int itemY;
};
std::queue<CollectionMessage> collectionQueue;

// Function to check if position is occupied by a crate
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

bool trySpawnItem(std::vector<Item>& items, int N, const sf::Texture& itemTexture, int cellSize, 
                 const std::vector<Crate>& crates, float currentTime) {
    if (items.size() >= MAX_ITEMS) {
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

        // Check if position is occupied by crates
        if (isPositionOccupied(item.x, item.y, crates)) {
            validPosition = false;
            continue;
        }

        // Check if position is occupied by other items
        for (const auto& existingItem : items) {
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
        items.push_back(item);
        return true;
    }

    return false;
}

void* playerMoves(void* args) {
    PlayerThreadData* ptd = (PlayerThreadData*)args;
    int newX = ptd->playerData[ptd->playerNum].x;
    int newY = ptd->playerData[ptd->playerNum].y;
    
    // Handle movement based on player number
    if (ptd->playerNum == 0) {
        if (ptd->event.key.code == sf::Keyboard::Up && newX > 1) newX--;
        if (ptd->event.key.code == sf::Keyboard::Down && newX < ptd->gridSize - 2) newX++;
        if (ptd->event.key.code == sf::Keyboard::Left && newY > 1) newY--;
        if (ptd->event.key.code == sf::Keyboard::Right && newY < ptd->gridSize - 2) newY++;
    } else {
        if (ptd->event.key.code == sf::Keyboard::W && newX > 1) newX--;
        if (ptd->event.key.code == sf::Keyboard::S && newX < ptd->gridSize - 2) newX++;
        if (ptd->event.key.code == sf::Keyboard::A && newY > 1) newY--;
        if (ptd->event.key.code == sf::Keyboard::D && newY < ptd->gridSize - 2) newY++;
    }
    
    // Add movement to queue if position changed
    if (newX != ptd->playerData[ptd->playerNum].x || newY != ptd->playerData[ptd->playerNum].y) {
        ptd->playerData[ptd->playerNum].moveQueue.push({newX, newY});
    }
    
    pthread_exit(NULL);
}

std::vector<SubTexture> loadSubTextures(const std::string& xmlFile, const std::string& prefix) {
    // Your existing loadSubTextures function remains unchanged
    // ...
    std::vector<SubTexture> subTextures;
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(xmlFile.c_str()) != tinyxml2::XML_SUCCESS) {
        std::cerr << "Failed to load XML file!" << std::endl;
        return subTextures;
    }

    tinyxml2::XMLElement* atlas = doc.FirstChildElement("TextureAtlas");
    if (!atlas) return subTextures;

    for (tinyxml2::XMLElement* elem = atlas->FirstChildElement("SubTexture"); 
         elem != nullptr; 
         elem = elem->NextSiblingElement("SubTexture")) {
        SubTexture subTexture;
        subTexture.name = elem->Attribute("name");
        subTexture.x = elem->IntAttribute("x");
        subTexture.y = elem->IntAttribute("y");
        subTexture.width = elem->IntAttribute("width");
        subTexture.height = elem->IntAttribute("height");
        
        if (subTexture.name.find(prefix) == 0) {
            subTextures.push_back(subTexture);
        }
    }
    return subTextures;
}

int generateGridSize(int rollNo) {
    srand(time(0));
    int randomNum = 10 + rand() % 90;
    float res = static_cast<float>(rollNo) / (randomNum * (rollNo % 10));
    res = static_cast<int>(res) % 25;
    if (res < 10) res += 15;
    return static_cast<int>(res);
}

int main() {
    int rollNum = 0615;
    int N = generateGridSize(rollNum);
    int windowSize = 600;
    int cellSize = windowSize/N;
    
    sf::RenderWindow window(sf::VideoMode(windowSize, windowSize), "Item Collection Game");
    
    // Load all textures
    sf::Texture textureSheet, itemTexture, crateTexture;
    if (!textureSheet.loadFromFile("resourcePack/Spritesheet/sokoban_spritesheet@2.png") ||
        !itemTexture.loadFromFile("item.png") ||
        !crateTexture.loadFromFile("crate.png")) {
        std::cerr << "Failed to load textures!" << std::endl;
        return -1;
    }

    // Setup font
    sf::Font font;
    if (!font.loadFromFile("arial.ttf")) {
        std::cerr << "Failed to load font!" << std::endl;
        return -1;
    }

    // Timer text setup
    sf::Text timerText;
    timerText.setFont(font);
    timerText.setCharacterSize(20);
    timerText.setFillColor(sf::Color::White);
    timerText.setPosition(windowSize - 150, 10);

    sf::Text scoreText;
    scoreText.setFont(font);
    scoreText.setCharacterSize(20);
    scoreText.setFillColor(sf::Color::White);
    scoreText.setPosition(10, 10);

    // Game over text
    sf::Text gameOverText;
    gameOverText.setFont(font);
    gameOverText.setCharacterSize(40);
    gameOverText.setFillColor(sf::Color::Yellow);
    gameOverText.setPosition(windowSize/2 - 150, windowSize/2 - 20);

    // Initialize game clock
    sf::Clock gameClock;
    sf::Clock itemSpawnClock;
    float lastItemSpawnTime = 0;

    // Setup game elements
    std::vector<SubTexture> groundTextures = loadSubTextures("resourcePack/Spritesheet/sokoban_spritesheet@2.xml", "ground");
    std::vector<SubTexture> blockTextures = loadSubTextures("resourcePack/Spritesheet/sokoban_spritesheet@2.xml", "block");
    
    // Setup sprites and players
    std::vector<sf::Sprite> groundSprites;
    std::vector<sf::Sprite> blockSprites;
    for (const auto& subTex : groundTextures) {
        sf::Sprite sprite;
        sprite.setTexture(textureSheet);
        sprite.setTextureRect(sf::IntRect(subTex.x, subTex.y, subTex.width, subTex.height));
        groundSprites.push_back(sprite);
    }
    for (const auto& subTex : blockTextures) {
        sf::Sprite sprite;
        sprite.setTexture(textureSheet);
        sprite.setTextureRect(sf::IntRect(subTex.x, subTex.y, subTex.width, subTex.height));
        blockSprites.push_back(sprite);
    }

    // Player setup
    sf::Texture playerTextures[TOTAL_PLAYERS];
    if (!playerTextures[0].loadFromFile("player_03.png")) return EXIT_FAILURE;
    if (!playerTextures[1].loadFromFile("player_06.png")) return EXIT_FAILURE;
    
    PlayerData playerData[TOTAL_PLAYERS];
    pthread_t tid[TOTAL_PLAYERS];
    std::vector<sf::Sprite> playerSprites;
    
    // Initialize players
    for (int i = 0; i < TOTAL_PLAYERS; i++) {
        sf::Sprite sprite;
        sprite.setTexture(playerTextures[i]);
        sprite.setTextureRect(sf::IntRect(0, 0, 128, 128));
        playerSprites.push_back(sprite);
        playerData[i].x = (rand() % (N - 2)) + 1;
        playerData[i].y = (rand() % (N - 2)) + 1;
    }

    // Generate items
    // std::vector<Item> items;
    // generateItems(items, N, itemTexture, cellSize);
    
    // Game loop
    int blockSpIndex = rand() % blockTextures.size();
    int groundSpIndex = rand() % groundTextures.size();


    // Generate initial crates
    std::vector<Crate> crates;
    generateCrates(crates, N, crateTexture, cellSize);

    // Initialize empty items vector
    std::vector<Item> items;

    bool gameRunning = true;
    while (window.isOpen()) {
        float currentTime = gameClock.getElapsedTime().asSeconds();
        float remainingTime = GAME_DURATION - currentTime;

        if (remainingTime <= 0 && gameRunning) {
            gameRunning = false;
            // Determine winner
            std::string winnerText;
            if (playerData[0].score > playerData[1].score) {
                winnerText = "Player 1 Wins!\nScore: " + std::to_string(playerData[0].score);
            } else if (playerData[1].score > playerData[0].score) {
                winnerText = "Player 2 Wins!\nScore: " + std::to_string(playerData[1].score);
            } else {
                winnerText = "It's a Tie!\nScore: " + std::to_string(playerData[0].score);
            }
            gameOverText.setString(winnerText);
        }

        // Handle events and movement
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            } else if (event.type == sf::Event::KeyPressed && gameRunning) {
                if (event.key.code == sf::Keyboard::Space) {
                    window.close();
                }
                // Handle player 1 movement
                if (event.key.code == sf::Keyboard::Up || 
                    event.key.code == sf::Keyboard::Down || 
                    event.key.code == sf::Keyboard::Left || 
                    event.key.code == sf::Keyboard::Right) {
                    PlayerThreadData ptd = {playerData, event, N, 0};
                    pthread_create(&tid[0], NULL, playerMoves, (void*)&ptd);
                    pthread_join(tid[0], NULL);
                }
                
                // Handle player 2 movement
                if (event.key.code == sf::Keyboard::W || 
                    event.key.code == sf::Keyboard::S || 
                    event.key.code == sf::Keyboard::A || 
                    event.key.code == sf::Keyboard::D) {
                    PlayerThreadData ptd = {playerData, event, N, 1};
                    pthread_create(&tid[1], NULL, playerMoves, (void*)&ptd);
                    pthread_join(tid[1], NULL);
                }
            }
        }

        // Spawn new items periodically if game is running
        if (gameRunning) {
            float timeSinceLastSpawn = currentTime - lastItemSpawnTime;
            if (timeSinceLastSpawn >= ITEM_SPAWN_INTERVAL) {
                if (trySpawnItem(items, N, itemTexture, cellSize, crates, currentTime)) {
                    lastItemSpawnTime = currentTime;
                }
            }

            // Remove items that have been on the board too long (15 seconds)
            items.erase(
                std::remove_if(items.begin(), items.end(),
                    [currentTime](const Item& item) {
                        return !item.collected && (currentTime - item.spawnTime) > 15.0f;
                    }),
                items.end()
            );
        }

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
        for (const auto& crate : crates) {
            window.draw(crate.sprite);
        }

        // Draw items
        for (const auto& item : items) {
            if (!item.collected) {
                window.draw(item.sprite);
            }
        }

        // Draw players if game is running
        if (gameRunning) {
            for (int i = 0; i < playerSprites.size(); i++) {
                playerSprites[i].setPosition(playerData[i].y * cellSize, playerData[i].x * cellSize);
                playerSprites[i].setScale(
                    static_cast<float>(cellSize) / playerSprites[i].getTextureRect().width,
                    static_cast<float>(cellSize) / playerSprites[i].getTextureRect().height
                );
                window.draw(playerSprites[i]);
            }
        }

        // Update and draw timer
        if (gameRunning) {
            std::string timerString = "Time: " + std::to_string(static_cast<int>(remainingTime));
            timerText.setString(timerString);
            window.draw(timerText);

            std::string scoreString = "P1: " + std::to_string(playerData[0].score) + 
                                    " | P2: " + std::to_string(playerData[1].score);
            scoreText.setString(scoreString);
            window.draw(scoreText);
        } else {
            window.draw(gameOverText);
        }

        window.display();
    }

    return 0;
}