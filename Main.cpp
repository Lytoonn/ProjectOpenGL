#include <GLAD/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <iostream>
#include <cstdlib>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

void mouse_callback(GLFWwindow* window, double xPos, double yPos);
void scroll_callback(GLFWwindow* window, double xoffSet, double yoffSet);

void initializeParticles();
void updateParticles(float deltaTime);
void setupParticleRendering();
void renderParticles();

const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool firstMouse = true;
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = 400, lastY = 300;
float fov = 45.0f;

// Particle structure
struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    float lifetime;
    glm::vec4 color;
};

// Particle container
std::vector<Particle> particles(10000);

// OpenGL variables
unsigned int VAO, VBO;
unsigned int shaderProgram;

// Vertex and Fragment Shaders
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;

out vec4 particleColor;

void main() {
    gl_Position = projection * view * vec4(aPos, 1.0);
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

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Particle System", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

    if (primaryMonitor && mode) {
        int screenWidth = mode->width;
        int screenHeight = mode->height;
        int windowPosX = (screenWidth - SCR_WIDTH) / 2;
        int windowPosY = (screenHeight - SCR_HEIGHT) / 2;
        glfwSetWindowPos(window, windowPosX, windowPosY);
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Initialize particles
    initializeParticles();
    setupParticleRendering();

    // Compile shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    float lastFrame = 0.0f;

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        updateParticles(deltaTime);

        // Render
        glClear(GL_COLOR_BUFFER_BIT);

        renderParticles();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = 2.5f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffSet = xpos - lastX;
    float yoffSet = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 1.0f;
    xoffSet *= sensitivity;
    yoffSet *= sensitivity;

    yaw += xoffSet;
    pitch += yoffSet;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(direction);
}

void scroll_callback(GLFWwindow* window, double xoffSet, double yoffSet) {
    fov -= (float)yoffSet;
    if (fov < 1.0f)
        fov = 1.0f;
    if (fov > 45.0f)
        fov = 45.0f;
}

void initializeParticles() {
    for (auto& particle : particles) {
        particle.position = glm::vec3(0.0f, 0.0f, 0.0f);
        particle.velocity = glm::vec3(
            static_cast<float>(rand()) / RAND_MAX - 0.5f,
            static_cast<float>(rand()) / RAND_MAX - 0.5f,
            static_cast<float>(rand()) / RAND_MAX - 0.5f
        );
        particle.lifetime = static_cast<float>(rand()) / RAND_MAX * 5.0f;
        particle.color = glm::vec4(
            static_cast<float>(rand()) / RAND_MAX,
            static_cast<float>(rand()) / RAND_MAX,
            static_cast<float>(rand()) / RAND_MAX,
            1.0f
        );
    }
}

void updateParticles(float deltaTime) {
    for (auto& particle : particles) {
        if (particle.lifetime > 0.0f) {
            particle.position += particle.velocity * deltaTime;
            particle.lifetime -= deltaTime;
        }
    }
}

void setupParticleRendering() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, particles.size() * (3+4) * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void renderParticles() {
    std::vector<float> particleData;

    for (const auto& particle : particles) {
        if (particle.lifetime > 0.0f) {
            particleData.push_back(particle.position.x);
            particleData.push_back(particle.position.y);
            particleData.push_back(particle.position.z);

            particleData.push_back(particle.color.r);
            particleData.push_back(particle.color.g);
            particleData.push_back(particle.color.b);
            particleData.push_back(particle.color.a);
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, particleData.size() * sizeof(float), particleData.data());

    glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    glm::mat4 projection = glm::perspective(glm::radians(fov), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

    glUseProgram(shaderProgram);

    unsigned int viewLoc = glGetUniformLocation(shaderProgram, "view");
    unsigned int viewProj = glGetUniformLocation(shaderProgram, "projection");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(viewProj, 1, GL_FALSE, glm::value_ptr(projection));

    glBindVertexArray(VAO);
    glDrawArrays(GL_POINTS, 0, particleData.size() / 7);
    glBindVertexArray(0);
}