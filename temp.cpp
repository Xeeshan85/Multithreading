
                // index = groundTextures.size() - 1;
                // if ((i == 1 && j == N-2) || (i == N-2 && j == 1)) {
                //     groundSprites[index].setPosition(j * cellSize, i * cellSize);
                //     groundSprites[index].setScale(
                //         static_cast<float>(cellSize) / groundSprites[index].getTextureRect().width,
                //         static_cast<float>(cellSize) / groundSprites[index].getTextureRect().height
                //     );
                //     window.draw(groundSprites[index]);
                // } 






// ##############################################################

#include <SFML/Graphics.hpp>
#include <tinyxml2.h>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <ctime>
#include <map>

#define TOTAL_PLAYERS 2

struct SubTexture {
    std::string name;
    int x, y, width, height;
};

struct PlayerData {
    int x, y, score, itemsCollected;
    PlayerData() {
        itemsCollected = 0;
        score = 0;
    }
};

struct PlayerThreadData {
    PlayerData *playerData;
    sf::Event event;
    int gridSize;
    int playerNum;
};

void* playerMoves(void* args) {
    PlayerThreadData* ptd = (PlayerThreadData*)args;
    if (ptd->playerNum == 0) {
        if (ptd->event.key.code == sf::Keyboard::Up) {
            if (0 < ptd->playerData[ptd->playerNum].x - 1) {
                ptd->playerData[ptd->playerNum].x--;
            }
        }
        if (ptd->event.key.code == sf::Keyboard::Down) {
            if (ptd->playerData[ptd->playerNum].x + 1 < ptd->gridSize - 1) {
                ptd->playerData[ptd->playerNum].x++;
            }
        }
        if (ptd->event.key.code == sf::Keyboard::Left) {
            if (0 < ptd->playerData[ptd->playerNum].y - 1) {
                ptd->playerData[ptd->playerNum].y--;
            }
        }
        if (ptd->event.key.code == sf::Keyboard::Right) {
            if (ptd->playerData[ptd->playerNum].y + 1 < ptd->gridSize - 1) {
                ptd->playerData[ptd->playerNum].y++;
            }
        }
    } else {
        if (ptd->event.key.code == sf::Keyboard::W) {
            if (0 < ptd->playerData[ptd->playerNum].x - 1) {
                ptd->playerData[ptd->playerNum].x--;
            }
        }
        if (ptd->event.key.code == sf::Keyboard::S) {
            if (ptd->playerData[ptd->playerNum].x + 1 < ptd->gridSize - 1) {
                ptd->playerData[ptd->playerNum].x++;
            }
        }
        if (ptd->event.key.code == sf::Keyboard::A) {
            if (0 < ptd->playerData[ptd->playerNum].y - 1) {
                ptd->playerData[ptd->playerNum].y--;
            }
        }
        if (ptd->event.key.code == sf::Keyboard::D) {
            if (ptd->playerData[ptd->playerNum].y + 1 < ptd->gridSize - 1) {
                ptd->playerData[ptd->playerNum].y++;
            }
        }
    }
    pthread_exit(NULL);
}

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

int main()
{
    int rollNum = 0615;
    int N = generateGridSize(rollNum);
    int windowSize = 600;
    int cellSize = windowSize/N;
    std::cout << "Grid Size: " << N << std::endl;

    sf::RenderWindow window(sf::VideoMode(windowSize, windowSize), "MAGA FIGHT!");
    
    sf::Texture textureSheet;
    if (!textureSheet.loadFromFile("resourcePack/Spritesheet/sokoban_spritesheet@2.png")) {
        std::cerr << "Failed to load spritesheet!" << std::endl;
        return -1;
    }
    std::vector<SubTexture> groundTextures = loadSubTextures("resourcePack/Spritesheet/sokoban_spritesheet@2.xml", "ground");
    std::vector<SubTexture> blockTextures = loadSubTextures("resourcePack/Spritesheet/sokoban_spritesheet@2.xml", "block");

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
    if (!playerTextures[0].loadFromFile("player_03.png"))
        return EXIT_FAILURE;
    if (!playerTextures[1].loadFromFile("player_06.png"))
        return EXIT_FAILURE;
    
    PlayerData playerData[TOTAL_PLAYERS];
    pthread_t tid[TOTAL_PLAYERS], mtid; // For maintainig Threads
    std::vector<sf::Sprite> playerSprites;
    for (int i = 0; i < TOTAL_PLAYERS; i++) {
        sf::Sprite sprite;
        sprite.setTexture(playerTextures[i]);  // Use the correct player texture
        sprite.setTextureRect(sf::IntRect(0, 0, 128, 128));
        playerSprites.push_back(sprite);
        playerData[i].x = (rand() % (N - 2)) + 1; // ########### Boundary Condition
        playerData[i].y = (rand() % (N - 2)) + 1;
    }
    

    int blockSpIndex = rand() % blockTextures.size();
    int groundSpIndex = rand() % (groundTextures.size() - 1);
    bool start = true;
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            } else if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Space) {
                    // Do nothing, the sprites will not change
                    window.close();
                }
                if (event.key.code == sf::Keyboard::Up || event.key.code == sf::Keyboard::Down || event.key.code == sf::Keyboard::Left || event.key.code == sf::Keyboard::Right) {
                    PlayerThreadData ptd = { &playerData[0], event, N, 0 };
                    // ptd.event = event; ptd.playerData = &playerData[0]; 
                    // ptd.gridSize = N; ptd.playerNum = 0;
                    pthread_create(&tid[0], NULL, playerMoves, (void*)&ptd);
                    pthread_join(tid[0], NULL);
                }

                if (event.key.code == sf::Keyboard::W || event.key.code == sf::Keyboard::S || event.key.code == sf::Keyboard::A || event.key.code == sf::Keyboard::D) {
                    PlayerThreadData ptd = { &playerData[1], event, N, 1 };
                    // ptd.event = event; ptd.playerData = &playerData[1]; 
                    // ptd.gridSize = N; ptd.playerNum = 1;
                    pthread_create(&tid[1], NULL, playerMoves, (void*)&ptd);
                    pthread_join(tid[1], NULL);
                }
            }
        }

        window.clear();

        // Rendring Grid
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                if ((i == 0 || i == N - 1 || j == 0 || j == N - 1)) { // For Boundaries
                    blockSprites[blockSpIndex].setPosition(j * cellSize, i * cellSize);
                    blockSprites[blockSpIndex].setScale(
                        static_cast<float>(cellSize) / blockSprites[blockSpIndex].getTextureRect().width,
                        static_cast<float>(cellSize) / blockSprites[blockSpIndex].getTextureRect().height
                    );
                    window.draw(blockSprites[blockSpIndex]);
                } else { // For Ground
                    groundSprites[groundSpIndex].setPosition(j * cellSize, i * cellSize);
                    groundSprites[groundSpIndex].setScale(
                        static_cast<float>(cellSize) / groundSprites[groundSpIndex].getTextureRect().width,
                        static_cast<float>(cellSize) / groundSprites[groundSpIndex].getTextureRect().height
                    );
                    window.draw(groundSprites[groundSpIndex]);
                }

            }
        }

        // Placing player in their positions
        for (int i = 0; i < playerSprites.size(); i++) {
            playerSprites[i].setPosition(playerData[i].y * cellSize, playerData[i].x * cellSize);
            playerSprites[i].setScale(
                static_cast<float>(cellSize) / playerSprites[i].getTextureRect().width,
                static_cast<float>(cellSize) / playerSprites[i].getTextureRect().height
            );
            window.draw(playerSprites[i]);
        }

        window.display();
    }

    return 0;
}