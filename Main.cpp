#include <GLAD/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <vector>
#include <iostream>
#include <cstdlib>

const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

struct Particle {
    glm::vec2 position;
    glm::vec2 velocity;
    float lifetime;
    glm::vec4 color;
};

float particleVelocity = 100.0f;
float particleLifetime = 5.0f;
int maxParticles = 2000;
std::vector<Particle> particles(maxParticles);
bool iman = true;

float obstacleSize = 200.0f;

struct Obstacle {
    glm::vec2 position;
    float size;
    int type; // 0 = square, 1 = triangle, 2 = circle
};

std::vector<Obstacle> obstacles;

unsigned int VAO, VBO, shaderProgram;
bool leftMousePressed = false, rightMousePressed = false;
double mouseX = 0.0, mouseY = 0.0;
glm::mat4 projection;

glm::vec4 particleColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;

out vec4 particleColor;

uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    particleColor = aColor;
    gl_PointSize = 5.0;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
in vec4 particleColor;
out vec4 FragColor;

void main() {
    FragColor = particleColor;
}
)";

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void initializeParticles();
void updateParticles(float deltaTime);
void setupParticleRendering();
void renderParticles();
void renderObstacles();
void setupShader();
bool isPointInTriangle(glm::vec2 p, glm::vec2 a, glm::vec2 b, glm::vec2 c);
glm::vec2 getRandomValidPosition(float size);
glm::vec2 getWorldPositionFromMouse(double mouseX, double mouseY);
void setupImGui(GLFWwindow* window);

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "2D Particle System", nullptr, nullptr);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    setupImGui(window);

    initializeParticles();
    setupParticleRendering();

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    projection = glm::ortho(0.0f, static_cast<float>(SCR_WIDTH), static_cast<float>(SCR_HEIGHT), 0.0f);

    float lastFrame = 0.0f;
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);
        updateParticles(deltaTime);

        for (auto& particle : particles) {
            if (particle.lifetime > 0.0f) {
                particle.color = particleColor;
            }
        }

        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Settings");

        ImGui::LabelText("---------", "Obstacle Settings");

        if (ImGui::SliderInt("Max Particles", &maxParticles, 1, 2000)) {
            particles.resize(maxParticles);
            std::cout << "Max Particles changed to " << maxParticles << std::endl;
        }
        if (ImGui::Button("Reset Max")) {
            std::cout << "Max Particles reseted to 2000" << std::endl;
            particles.resize(2000);
        }

        if (ImGui::SliderFloat("Particles Lifetime", &particleLifetime, 0.1f, 20.0f)) {
            std::cout << "Lifetime changed to " << particleLifetime << std::endl;
        }
        if (ImGui::Button("Reset Lifetime")) {
            std::cout << "Lifetime reseted" << std::endl;
            particleLifetime = 5.0f;
        }

        if (ImGui::SliderFloat("Particles Velocity", &particleVelocity, 0.1f, 500)) {
            std::cout << "Velocity changed to " << particleVelocity << std::endl;
        }
        if (ImGui::Button("Reset Velocity")) {
            std::cout << "Velocity reseted" << std::endl;
            particleVelocity = 100.0f;
        }

        if (ImGui::Button(iman ? "Repel Particles" : "Attract Particles")) {
            iman = !iman;
            if (iman) std::cout << "Attract Particles" << std::endl;
            else std::cout << "Repel Particles" << std::endl;
        }

        ImGui::LabelText("---------", "Obstacle Settings");

        if (ImGui::Button("Create Square")) {
            glm::vec2 pos = getRandomValidPosition(obstacleSize);
            obstacles.push_back({ pos, obstacleSize, 0 });
            std::cout << "Square created at: " << pos.x << ", " << pos.y << std::endl;
        }
        if (ImGui::Button("Create Triangle")) {
            glm::vec2 pos = getRandomValidPosition(obstacleSize);
            obstacles.push_back({ pos, obstacleSize, 1 });
            std::cout << "Triangle created at: " << pos.x << ", " << pos.y << std::endl;
        }
        if (ImGui::Button("Create Circle")) {
            glm::vec2 pos = getRandomValidPosition(obstacleSize);
            obstacles.push_back({ pos, obstacleSize, 2 });
            std::cout << "Circle created at: " << pos.x << ", " << pos.y << std::endl;
        }

        if (ImGui::SliderFloat("Obstacle Size", &obstacleSize, 100.0f, 1000.0f)) {
            std::cout << "Obstacle size changed to " << particleVelocity << std::endl;
        }

        if (ImGui::Button("Delete All Objects")) {
            obstacles.clear();
            std::cout << "All objects deleted" << std::endl;
        }

        ImGui::End();

        renderParticles();
        renderObstacles();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        leftMousePressed = (action == GLFW_PRESS);
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        rightMousePressed = (action == GLFW_PRESS);
    }
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    mouseX = xpos;
    mouseY = ypos;
}

void initializeParticles() {
    for (auto& particle : particles) {
        particle.position = glm::vec2(0.0f);
        particle.velocity = glm::vec2(0.0f);
        particle.lifetime = 0.0f;
        particle.color = particleColor;
    }
}

glm::vec2 getWorldPositionFromMouse(double mouseX, double mouseY) {
    return glm::vec2(mouseX, mouseY);
}

void updateParticles(float deltaTime) {
    glm::vec2 cursorPos = getWorldPositionFromMouse(mouseX, mouseY);

    for (auto& particle : particles) {
        if (particle.lifetime > 0.0f) {
            if (rightMousePressed) {
                glm::vec2 direction = iman ? (cursorPos - particle.position) : (particle.position - cursorPos);
                float length = glm::length(direction);
                if (length > 0.0f) direction /= length;
                particle.velocity += direction * particleVelocity * deltaTime;
            }

            particle.position += particle.velocity * deltaTime;
            particle.lifetime -= deltaTime;

            for (auto& obstacle : obstacles) {
                if (obstacle.type == 0) { // Square collision
                    glm::vec2 min = obstacle.position - glm::vec2(obstacle.size / 2);
                    glm::vec2 max = obstacle.position + glm::vec2(obstacle.size / 2);
                    if (particle.position.x > min.x && particle.position.x < max.x &&
                        particle.position.y > min.y && particle.position.y < max.y) {
                        particle.velocity = -particle.velocity; // Bounce
                    }
                }
                else if (obstacle.type == 1) { // Triangle collision
                    glm::vec2 a = obstacle.position + glm::vec2(0, -obstacle.size / 2);
                    glm::vec2 b = obstacle.position + glm::vec2(-obstacle.size / 2, obstacle.size / 2);
                    glm::vec2 c = obstacle.position + glm::vec2(obstacle.size / 2, obstacle.size / 2);

                    if (isPointInTriangle(particle.position, a, b, c)) {
                        particle.velocity = -particle.velocity;
                    }
                }
                else if (obstacle.type == 2) { // Circle collision
                    float dist = glm::length(particle.position - obstacle.position);
                    if (dist < obstacle.size / 2) {
                        particle.velocity = -particle.velocity;
                    }
                }
            }

            if (particle.lifetime < 0.0f) particle.lifetime = 0.0f;
        }
    }

    if (leftMousePressed && !ImGui::GetIO().WantCaptureMouse) {
        glm::vec2 worldPos = getWorldPositionFromMouse(mouseX, mouseY);
        for (auto& particle : particles) {
            if (particle.lifetime <= 0.0f) {
                particle.position = worldPos;
                particle.velocity = glm::vec2(
                    (static_cast<float>(rand()) / RAND_MAX - 0.5f) * particleVelocity,
                    (static_cast<float>(rand()) / RAND_MAX - 0.5f) * particleVelocity
                );
                particle.lifetime = static_cast<float>(rand()) / RAND_MAX * particleLifetime;
                particle.color = particleColor;
                break;
            }
        }
    }
}

void setupParticleRendering() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(Particle), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)offsetof(Particle, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Particle), (void*)offsetof(Particle, color));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void renderParticles() {
    renderObstacles(); // Draw obstacles before particles

    std::vector<float> particleData;
    for (auto& particle : particles) {
        if (particle.lifetime > 0.0f) {
            particleData.insert(particleData.end(), {
                particle.position.x, particle.position.y,
                particle.color.r, particle.color.g, particle.color.b, particle.color.a
                });
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, particleData.size() * sizeof(float), particleData.data(), GL_DYNAMIC_DRAW);

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    glBindVertexArray(VAO);
    glDrawArrays(GL_POINTS, 0, particleData.size() / 6);
    glBindVertexArray(0);
}

void setupShader() {
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void renderObstacles() {
    static unsigned int VAO, VBO;
    static bool initialized = false;

    if (!initialized) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        initialized = true;
    }

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    for (auto& obstacle : obstacles) {
        float x = obstacle.position.x;
        float y = obstacle.position.y;
        float s = obstacle.size / 2;
        std::vector<float> vertices;

        if (obstacle.type == 0) {
            vertices = { x - s, y - s,  x + s, y - s,  x + s, y + s,  x - s, y + s };
        }
        else if (obstacle.type == 1) {
            vertices = { x, y - s,  x - s, y + s,  x + s, y + s };
        }
        else if (obstacle.type == 2) {
            int segments = 20;
            for (int i = 0; i <= segments; ++i) {
                float angle = i * 2.0f * 3.14159f / segments;
                vertices.push_back(x + cos(angle) * s);
                vertices.push_back(y + sin(angle) * s);
            }
        }

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttrib4f(1, 1.0f, 1.0f, 1.0f, 1.0f);

        glDrawArrays(obstacle.type == 2 ? GL_TRIANGLE_FAN : GL_TRIANGLE_FAN, 0, vertices.size() / 2);
    }
}

glm::vec2 getRandomValidPosition(float size) {
    glm::vec2 pos;
    bool validPosition = false;
    int maxAttempts = 100;

    while (!validPosition && maxAttempts > 0) {
        pos = glm::vec2(
            static_cast<float>(rand() % (SCR_WIDTH - static_cast<int>(size))) + size / 2,
            static_cast<float>(rand() % (SCR_HEIGHT - static_cast<int>(size))) + size / 2
        );

        validPosition = true;
        for (auto& obstacle : obstacles) {
            if (obstacle.type == 0) {
                glm::vec2 min = obstacle.position - glm::vec2(obstacle.size / 2);
                glm::vec2 max = obstacle.position + glm::vec2(obstacle.size / 2);
                if (pos.x > min.x && pos.x < max.x && pos.y > min.y && pos.y < max.y) {
                    validPosition = false;
                    break;
                }
            }
            else if (obstacle.type == 2) {
                float dist = glm::length(pos - obstacle.position);
                if (dist < (obstacle.size / 2 + size / 2)) {
                    validPosition = false;
                    break;
                }
            }
        }
        maxAttempts--;
    }
    return pos;
}

bool isPointInTriangle(glm::vec2 p, glm::vec2 a, glm::vec2 b, glm::vec2 c) {
    float area = 0.5f * (-b.y * c.x + a.y * (-b.x + c.x) + a.x * (b.y - c.y) + b.x * c.y);
    float s = 1 / (2 * area) * (a.y * c.x - a.x * c.y + (c.y - a.y) * p.x + (a.x - c.x) * p.y);
    float t = 1 / (2 * area) * (a.x * b.y - a.y * b.x + (a.y - b.y) * p.x + (b.x - a.x) * p.y);

    return s >= 0 && t >= 0 && (s + t) <= 1;
}

void setupImGui(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}
