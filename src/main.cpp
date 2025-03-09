#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <vector>
#include <ctime>
#include <cstdlib>
#define STB_EASY_FONT_IMPLEMENTATION
#include "../include/stb_easy_font.h"
#define MINIAUDIO_IMPLEMENTATION
#include "../include/miniaudio.h"
#include "../include/bonk_wav.h"

// Constants
namespace {
    constexpr unsigned int SCR_WIDTH = 900;
    constexpr unsigned int SCR_HEIGHT = 900;
    constexpr float BORDER_WIDTH = 0.07f;
    constexpr float BORDER_OFFSET = 1.0f - BORDER_WIDTH/2;
}

// Game State Structure
struct GameState {
    int playerScore = 0;
    int aiScore = 0;
    bool gameOver = false;
    bool gameStarted = false;
    bool waitingToServe = false;
    float lastFrameTime = 0.0f;
    float scoreTime = 0.0f;
    float nextServeDir = 1.5f;
};

// Audio Resources
struct AudioManager {
    ma_engine engine;
    std::vector<ma_sound> bonkSounds;  // Pool of sounds
    std::vector<ma_decoder> decoders;  // Dedicated decoder per sound
    size_t nextSound = 0;

    AudioManager() {
        ma_engine_init(NULL, &engine);

        constexpr size_t POOL_SIZE = 4;
        bonkSounds.resize(POOL_SIZE);
        decoders.resize(POOL_SIZE);

        for (size_t i = 0; i < POOL_SIZE; ++i) {
            ma_decoder_init_memory(bonk_wav, bonk_wav_size, NULL, &decoders[i]);
            ma_sound_init_from_data_source(&engine, &decoders[i], 0, NULL, &bonkSounds[i]);
            ma_sound_set_volume(&bonkSounds[i], 0.5f);
        }
    }

    ~AudioManager() {
        for (auto& sound : bonkSounds) {
            ma_sound_uninit(&sound);
        }
        for (auto& decoder : decoders) {
            ma_decoder_uninit(&decoder);
        }
        ma_engine_uninit(&engine);
    }

    void playBonk() {
        ma_sound* sound = &bonkSounds[nextSound];
        ma_decoder* decoder = &decoders[nextSound];
        
        // Stop, reset decoder, and play
        ma_sound_stop(sound);
        ma_decoder_seek_to_pcm_frame(decoder, 0);
        ma_sound_start(sound);
        
        nextSound = (nextSound + 1) % bonkSounds.size();
    }
};

// Shader Sources
namespace Shaders {
    const char* vertex = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        uniform vec2 translation;
        uniform vec2 scale;
        out vec2 localPos;
        
        void main() {
            localPos = aPos;
            vec2 scaled = aPos * scale;
            gl_Position = vec4(scaled + translation, 0.0, 1.0);
        })";

    const char* fragment = R"(
        #version 330 core
        in vec2 localPos;
        out vec4 FragColor;
        uniform vec3 color;
        uniform bool isCircle;
        
        void main() {
            if (isCircle) {
                float edge = fwidth(length(localPos));
                if (length(localPos) > 0.5 - edge) discard;
            }
            FragColor = vec4(color, 1.0);
        })";
}

// Game Objects
struct Paddle {
    float x, y;
    float width = 0.05f;
    float height = 0.25f;
    float speed = 0.0f;
    
    Paddle(float xPos, float yPos) : x(xPos), y(yPos) {}
};

struct Ball {
    float x = 0.0f, y = 0.0f;
    float size = 0.05f;
    float speedX = 1.3f, speedY = 0.6f;
    bool firstHit = true;
};

// Function Declarations
GLFWwindow* initWindow();
unsigned int createShaderProgram();
void processInput(GLFWwindow* window);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
bool checkPaddleCollision(const Paddle& paddle, Ball& ball);
void resetGameState(GameState& state, Paddle& player, Paddle& ai, Ball& ball, float currentTime);
void renderText(GLFWwindow* window, const char* text, float scale, float yOffset, const float color[3], unsigned int shaderProgram);

int main() {
    GLFWwindow* window = initWindow();
    if (!window) return EXIT_FAILURE;

    AudioManager audio;

    // Initialize game objects and state
    GameState state;
    Paddle playerPaddle(0.9f, 0.0f);
    Paddle aiPaddle(-0.9f, 0.0f);
    aiPaddle.speed = 1.3f;
    Ball ball;
    unsigned int shaderProgram = createShaderProgram();

    // Vertex data for quad rendering
    const float vertices[] = {
        -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
         0.5f, 0.5f, -0.5f, 0.5f, -0.5f, -0.5f
    };

    // Setup VAO/VBO
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Rendering helper
    auto drawObject = [&](float x, float y, float w, float h, const float color[3], bool circle) {
        glUniform2f(glGetUniformLocation(shaderProgram, "translation"), x, y);
        glUniform2f(glGetUniformLocation(shaderProgram, "scale"), w, h);
        glUniform3fv(glGetUniformLocation(shaderProgram, "color"), 1, color);
        glUniform1i(glGetUniformLocation(shaderProgram, "isCircle"), circle);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };

    // Game loop
    while (!glfwWindowShouldClose(window)) {
        
        // Timing
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - state.lastFrameTime;
        state.lastFrameTime = currentFrame;

        processInput(window);

        // Game state transitions
        if (!state.gameStarted && glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
            resetGameState(state, playerPaddle, aiPaddle, ball, glfwGetTime());
            state.gameStarted = true;
        }
        if (state.gameOver && glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
            resetGameState(state, playerPaddle, aiPaddle, ball, glfwGetTime());
            state.gameOver = false;
        }

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        if (!state.gameStarted || state.gameOver) glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (!state.gameStarted) {
            const float yellow[3] = {1.0f, 1.0f, 0.0f};
            const float white[3] = {1.0f, 1.0f, 1.0f};
            renderText(window, "PONG", 4.0f, -25.0f, yellow, shaderProgram);
            renderText(window, "Press P to play", 2.5f, -15.0f, white, shaderProgram);
        } else if (!state.gameOver) {
            // Update paddles
            const float PADDLE_BOUNDARY = 1.0f - (playerPaddle.height/2 + BORDER_WIDTH);
            
            // Player paddle follows mouse
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            playerPaddle.y = std::clamp(static_cast<float>(1.0f - (2.0f * mouseY / SCR_HEIGHT)), -PADDLE_BOUNDARY, PADDLE_BOUNDARY);

            // AI paddle follows ball
            aiPaddle.y += aiPaddle.speed * ((ball.y > aiPaddle.y) ? deltaTime : -deltaTime);
            aiPaddle.y = std::clamp(aiPaddle.y, -PADDLE_BOUNDARY, PADDLE_BOUNDARY);

            // Update ball position
            if (!state.waitingToServe) {
                ball.x += ball.speedX * deltaTime;
                ball.y += ball.speedY * deltaTime;
            }

            // Border collisions
            if (std::abs(ball.y) + ball.size/2 > BORDER_OFFSET) {
                ball.speedY = -std::copysign(ball.speedY, ball.y);
                ball.y = std::copysign(BORDER_OFFSET - ball.size/2 - 0.001f, ball.y);
                audio.playBonk();
            }

            // Paddle collisions
            if (checkPaddleCollision(playerPaddle, ball) || checkPaddleCollision(aiPaddle, ball)) {
                audio.playBonk();
            }

            // Scoring
            if (std::abs(ball.x) > 1.0f) {
                (ball.x < 0 ? state.playerScore : state.aiScore)++;
                state.nextServeDir = (ball.x > 0) ? 1.5f : -1.5f;
                state.waitingToServe = true;
                state.scoreTime = currentFrame;
                ball = Ball();

                if (std::max(state.playerScore, state.aiScore) >= 5) {
                    state.gameOver = true;
                }
            }

            // Serve after delay
            if (state.waitingToServe && (currentFrame - state.scoreTime >= 1.0f)) {
                state.waitingToServe = false;
                ball.speedX = state.nextServeDir;
            }

            // Render game objects
            glUseProgram(shaderProgram);
            glBindVertexArray(VAO);
            
            const float red[3] = {1.0f, 0.0f, 0.0f};
            const float blue[3] = {0.0f, 0.5f, 1.0f};
            const float yellow[3] = {1.0f, 1.0f, 0.0f};
            const float gray[3] = {0.35f, 0.35f, 0.35f};

            drawObject(playerPaddle.x, playerPaddle.y, playerPaddle.width, 
                      playerPaddle.height, red, false);
            drawObject(aiPaddle.x, aiPaddle.y, aiPaddle.width, 
                      aiPaddle.height, blue, false);
            if (!state.waitingToServe)
                drawObject(ball.x, ball.y, ball.size, ball.size, yellow, true);
            drawObject(0.0f, BORDER_OFFSET, 2.0f, BORDER_WIDTH, gray, false);
            drawObject(0.0f, -BORDER_OFFSET, 2.0f, BORDER_WIDTH, gray, false);

            // Render score
            char scoreText[50];
            snprintf(scoreText, sizeof(scoreText), "Cleetus: %d  Player: %d", state.aiScore, state.playerScore);
            renderText(window, scoreText, 3.0f, 141.0f, yellow, shaderProgram);
        } else {
            // Game over screen
            const char* winner = (state.aiScore >= 5) ? "Cleetus Wins!" : "Humanity Wins!";
            const float white[3] = {1.0f, 1.0f, 1.0f};
            const float yellow[3] = {1.0f, 1.0f, 0.0f};
            renderText(window, winner, 4.0f, -20.0f, yellow, shaderProgram);
            renderText(window, "Press P to play again.", 2.5f, -5.0f, white, shaderProgram);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return EXIT_SUCCESS;
}

void resetGameState(GameState& state, Paddle& player, Paddle& ai, Ball& ball, float currentTime) {
    state.playerScore = 0;
    state.aiScore = 0;
    state.waitingToServe = true;
    state.scoreTime = currentTime;
    state.nextServeDir = -1.5f;

    player = Paddle(0.9f, 0.0f);
    ai = Paddle(-0.9f, 0.0f);
    ai.speed = 1.3f;
    ball = Ball();
}

bool checkPaddleCollision(const Paddle& paddle, Ball& ball) {
    // Calculate collision box boundaries
    const float ballLeft = ball.x - ball.size/2;
    const float ballRight = ball.x + ball.size/2;
    const float paddleLeft = paddle.x - paddle.width/2;
    const float paddleRight = paddle.x + paddle.width/2;

    const float ballBottom = ball.y - ball.size/2;
    const float ballTop = ball.y + ball.size/2;
    const float paddleBottom = paddle.y - paddle.height/2;
    const float paddleTop = paddle.y + paddle.height/2;

    // Axis-aligned bounding box collision
    if (ballRight > paddleLeft && ballLeft < paddleRight &&
        ballTop > paddleBottom && ballBottom < paddleTop) {
        // Reposition ball
        ball.x = (ball.speedX > 0) 
               ? paddleLeft - ball.size/2 - 0.001f 
               : paddleRight + ball.size/2 + 0.001f;

        // Update ball dynamics
        ball.speedX = (ball.firstHit) 
                     ? ((ball.speedX > 0) ? 3.0f : -3.0f)
                     : -ball.speedX;
        ball.speedY += ((ball.y - paddle.y) / (paddle.height/2)) * 1.0f;
        ball.firstHit = false;
        return true;
    }
    return false;
}

void renderText(GLFWwindow* window, const char* text, float scale, float yOffset, 
    const float color[3], unsigned int shaderProgram) {
    std::vector<float> vertices;
    int winWidth, winHeight;
    glfwGetFramebufferSize(window, &winWidth, &winHeight);

    int textWidth = stb_easy_font_width(const_cast<char*>(text));
    float x = (winWidth / (2.0f * scale)) - (textWidth / 2.0f);
    float y = (winHeight / (2.0f * scale)) + yOffset;

    char buffer[99999];
    int quads = stb_easy_font_print(x, y, const_cast<char*>(text), NULL, buffer, sizeof(buffer));

    // Convert quads to triangles
    const int indices[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < quads; ++i) {
        const char* quad = buffer + i * 4 * 16;
        float v[4][2];
        for (int j = 0; j < 4; ++j) {
            v[j][0] = *reinterpret_cast<const float*>(quad + j*16);
            v[j][1] = *reinterpret_cast<const float*>(quad + j*16 + 4);
        }

    for (int j = 0; j < 6; ++j) {
        int idx = indices[j];
        float px = v[idx][0] * scale;
        float py = v[idx][1] * scale;

        // Convert to NDC
        vertices.push_back(2.0f * px / winWidth - 1.0f);
        vertices.push_back(1.0f - 2.0f * py / winHeight);
        }
    }

    // Render text
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(float), vertices.data(), GL_STREAM_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glUseProgram(shaderProgram);
    glUniform2f(glGetUniformLocation(shaderProgram, "translation"), 0.0f, 0.0f);
    glUniform2f(glGetUniformLocation(shaderProgram, "scale"), 1.0f, 1.0f);
    glUniform3fv(glGetUniformLocation(shaderProgram, "color"), 1, color);
    glUniform1i(glGetUniformLocation(shaderProgram, "isCircle"), 0);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertices.size()/2);

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

GLFWwindow* initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Pong", NULL, NULL);
    if (!window) {
        std::cerr << "Window creation failed\n";
        glfwTerminate();
        return nullptr;
    }

    // Center window
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwSetWindowPos(window, (mode->width - SCR_WIDTH)/2, (mode->height - SCR_HEIGHT)/2);

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "GLAD init failed\n";
        return nullptr;
    }

    return window;
}

unsigned int createShaderProgram() {
    auto compileShader = [](const char* source, unsigned int type) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        
        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[512];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::cerr << "Shader error: " << log << "\n";
        }
        return shader;
    };

    unsigned int vertex = compileShader(Shaders::vertex, GL_VERTEX_SHADER);
    unsigned int fragment = compileShader(Shaders::fragment, GL_FRAGMENT_SHADER);
    
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Program linking error: " << log << "\n";
    }

    return program;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}