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

std::vector<Particle> particles(1000);

unsigned int VAO, VBO, shaderProgram;
bool leftMousePressed = false;
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

        std::cout << "Rendering ImGui Window..." << std::endl;

        ImGui::Begin("Particle Settings");
        ImGui::ColorEdit4("Particle Color", glm::value_ptr(particleColor));
        ImGui::End();

        renderParticles();

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
        particle.color = glm::vec4(1.0f);
    }
}

glm::vec2 getWorldPositionFromMouse(double mouseX, double mouseY) {
    return glm::vec2(mouseX, mouseY);
}

void updateParticles(float deltaTime) {
    for (auto& particle : particles) {
        if (particle.lifetime > 0.0f) {
            particle.position += particle.velocity * deltaTime;
            particle.lifetime -= deltaTime;

            if (particle.lifetime < 0.0f) {
                particle.lifetime = 0.0f;
            }
        }
    }

    if (leftMousePressed) {
        glm::vec2 worldPos = getWorldPositionFromMouse(mouseX, mouseY);
        for (auto& particle : particles) {
            if (particle.lifetime <= 0.0f) {
                particle.position = worldPos;
                particle.velocity = glm::vec2(
                    (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 200.0f,
                    (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 200.0f
                );
                particle.lifetime = static_cast<float>(rand()) / RAND_MAX * 5.0f;
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
    std::vector<float> particleData;
    particleData.reserve(particles.size() * 6);

    for (auto& particle : particles) {
        if (particle.lifetime > 0.0f) {
            particleData.push_back(particle.position.x);
            particleData.push_back(particle.position.y);

            particleData.push_back(particle.color.r);
            particleData.push_back(particle.color.g);
            particleData.push_back(particle.color.b);
            particleData.push_back(particle.color.a);
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

void setupImGui(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}
