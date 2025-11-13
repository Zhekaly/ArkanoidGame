#include <glad.h>
#include <glfw3.h>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include <vector>
#include <iostream>
#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdlib>
#include <ctime>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetShaderInfoLog(s, 512, nullptr, buf);
        std::cerr << "Shader compile error: " << buf << "\n";
    }
    return s;
}
GLuint linkProgram(const char* vs, const char* fs) {
    GLuint P = glCreateProgram();
    GLuint a = compileShader(GL_VERTEX_SHADER, vs);
    GLuint b = compileShader(GL_FRAGMENT_SHADER, fs);
    glAttachShader(P, a); glAttachShader(P, b);
    glLinkProgram(P);
    GLint ok; glGetProgramiv(P, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512]; glGetProgramInfoLog(P, 512, nullptr, buf);
        std::cerr << "Program link error: " << buf << "\n";
    }
    glDeleteShader(a); glDeleteShader(b);
    return P;
}

const char* quad_vs = R"(
#version 330 core
layout(location=0) in vec2 inPos;
layout(location=1) in vec2 inUV;

uniform mat4 projection;
uniform mat4 model;

out vec2 uv;

void main(){
    uv = inUV;
    gl_Position = projection * model * vec4(inPos, 0.0, 1.0);
}
)";

const char* quad_fs = R"(
#version 330 core
in vec2 uv;
out vec4 FragColor;

uniform sampler2D tex;
uniform vec4 color;

void main(){
    vec4 t = texture(tex, uv);
    FragColor = t * color;
}
)";

const char* rect_vs = R"(
#version 330 core
layout(location=0) in vec2 inPos;

uniform mat4 projection;
uniform mat4 model;

void main(){
    gl_Position = projection * model * vec4(inPos, 0.0, 1.0);
}
)";

const char* rect_fs = R"(
#version 330 core
out vec4 FragColor;

uniform vec4 color;

void main(){
    FragColor = color;
}
)";

GLuint loadTexture(const char* path) {
    int w, h, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &channels, 4);
    if (!data) {
        std::cout << "Failed to load texture: " << path << std::endl;
        return 0;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);
    return tex;
}


GLuint createQuadVAO() {
    float data[] = {
        0.0f, 1.0f,  0.0f, 1.0f,
        1.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 0.0f,  0.0f, 0.0f,

        0.0f, 1.0f,  0.0f, 1.0f,
        1.0f, 1.0f,  1.0f, 1.0f,
        1.0f, 0.0f,  1.0f, 0.0f
    };
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    return VAO;
}

GLuint createRectVAO() {
    float data[] = {
        0.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f
    };
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    return VAO;
}

struct Sprite {
    glm::vec2 pos;
    glm::vec2 size;
    float rotation = 0.0f;
    GLuint tex = 0;
    glm::vec4 color = glm::vec4(1.0f);
    bool transparent = false;
    float depth = 0.0f;
};

struct Brick {
    Sprite s;
    bool alive = true;
};

struct Ball {
    glm::vec2 pos;
    glm::vec2 vel;
    float radius;
    GLuint tex;
};

struct Paddle {
    glm::vec2 pos;
    glm::vec2 size;
    GLuint tex;
};

enum PowerUpType {
    MULTIBALL = 0,
    EXTRALIFE = 1
};

struct PowerUp {
    glm::vec2 pos;
    glm::vec2 vel;
    glm::vec2 size;
    PowerUpType type;
    bool active;
    GLuint tex;
};

bool AABBvsCircle(const glm::vec2& aPos, const glm::vec2& aSize,
    const glm::vec2& cPos, float r, glm::vec2& outClosest)
{
    glm::vec2 aMin = aPos;
    glm::vec2 aMax = aPos + aSize;
    float cx = std::max(aMin.x, std::min(cPos.x, aMax.x));
    float cy = std::max(aMin.y, std::min(cPos.y, aMax.y));
    outClosest = glm::vec2(cx, cy);
    float dx = cx - cPos.x;
    float dy = cy - cPos.y;
    return (dx * dx + dy * dy) <= r * r;
}

void drawRect(float x, float y, float w, float h, glm::vec4 color,
    GLuint program, GLuint VAO, const glm::mat4& proj) {
    GLint loc_projection = glGetUniformLocation(program, "projection");
    GLint loc_model = glGetUniformLocation(program, "model");
    GLint loc_color = glGetUniformLocation(program, "color");

    glUseProgram(program);
    glUniformMatrix4fv(loc_projection, 1, GL_FALSE, &proj[0][0]);
    glUniform4fv(loc_color, 1, &color[0]);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(w, h, 1.0f));

    glUniformMatrix4fv(loc_model, 1, GL_FALSE, &model[0][0]);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void drawBigText(const std::string& text, float x, float y, float scale, glm::vec4 color,
    GLuint program, GLuint VAO, const glm::mat4& proj) {

    float charW = 45.0f * scale;
    float charH = 70.0f * scale;
    float thick = 8.0f * scale;
    float gap = 15.0f * scale;

    float currentX = x;

    for (char c : text) {
        if (c == ' ') {
            currentX += charW * 0.8f;
            continue;
        }

        switch (c) {
        case 'G':
            drawRect(currentX, y, thick, charH, color, program, VAO, proj);
            drawRect(currentX, y + charH - thick, charW, thick, color, program, VAO, proj);
            drawRect(currentX, y, charW, thick, color, program, VAO, proj);
            drawRect(currentX + charW - thick, y, thick, charH * 0.5f, color, program, VAO, proj);
            drawRect(currentX + charW * 0.4f, y + charH * 0.4f, charW * 0.6f - thick, thick, color, program, VAO, proj);
            break;
        case 'A':
            drawRect(currentX, y, thick, charH, color, program, VAO, proj);
            drawRect(currentX + charW - thick, y, thick, charH, color, program, VAO, proj);
            drawRect(currentX, y + charH - thick, charW, thick, color, program, VAO, proj);
            drawRect(currentX, y + charH * 0.4f, charW, thick, color, program, VAO, proj);
            break;
        case 'M': {
            drawRect(currentX, y, thick, charH, color, program, VAO, proj);
            drawRect(currentX + charW - thick, y, thick, charH, color, program, VAO, proj);

            for (float i = 0; i < charH / 2.0f; i += thick) {
                float dx = i * (charW / (charH)); 
                float dy = i;                      
                drawRect(currentX + dx, y + charH - i - thick, thick, thick, color, program, VAO, proj);
            }

            for (float i = 0; i < charH / 2.0f; i += thick) {
                float dx = i * (charW / (charH));
                drawRect(currentX + charW - dx - thick, y + charH - i - thick, thick, thick, color, program, VAO, proj);
            }
            break;
        }


        case 'E':
            drawRect(currentX, y, thick, charH, color, program, VAO, proj);
            drawRect(currentX, y + charH - thick, charW, thick, color, program, VAO, proj);
            drawRect(currentX, y + charH * 0.45f, charW * 0.8f, thick, color, program, VAO, proj);
            drawRect(currentX, y, charW, thick, color, program, VAO, proj);
            break;
        case 'O':
            drawRect(currentX, y, thick, charH, color, program, VAO, proj);
            drawRect(currentX + charW - thick, y, thick, charH, color, program, VAO, proj);
            drawRect(currentX, y + charH - thick, charW, thick, color, program, VAO, proj);
            drawRect(currentX, y, charW, thick, color, program, VAO, proj);
            break;
        case 'V': {
            for (float i = 0; i < charH; i += thick) {
                float dx = (i / charH) * (charW * 0.5f); 
                drawRect(currentX + dx, y + charH - i - thick, thick, thick, color, program, VAO, proj);
            }

            for (float i = 0; i < charH; i += thick) {
                float dx = (i / charH) * (charW * 0.5f); 
                drawRect(currentX + charW - dx - thick, y + charH - i - thick, thick, thick, color, program, VAO, proj);
            }
            break;
        }

                break;
        case 'R': {
            drawRect(currentX, y, thick, charH, color, program, VAO, proj);
            drawRect(currentX, y + charH - thick, charW * 0.7f, thick, color, program, VAO, proj);

            drawRect(currentX, y + charH * 0.55f, charW * 0.7f, thick, color, program, VAO, proj);

            drawRect(currentX + charW * 0.7f - thick, y + charH * 0.55f, thick, charH * 0.45f - thick, color, program, VAO, proj);

            for (float i = 0; i < charH * 0.45f; i += thick) {
                float dx = (i / (charH * 0.45f)) * (charW * 0.4f); 
                drawRect(currentX + charW * 0.7f - dx - thick, y + i, thick, thick, color, program, VAO, proj);
            }
            break;
        }




        case 'Y': {
            float midX = currentX + charW * 0.5f;
            float halfW = charW * 0.5f;

            drawRect(currentX, y + charH * 0.5f, thick, charH * 0.5f, color, program, VAO, proj);
            glPushMatrix();
            glTranslatef(currentX + halfW * 0.25f, y + charH * 0.75f, 0);
            glRotatef(45, 0, 0, 1);
            glPopMatrix();

            drawRect(currentX + charW - thick, y + charH * 0.5f, thick, charH * 0.5f, color, program, VAO, proj);

            drawRect(midX - thick * 0.5f, y, thick, charH * 0.5f, color, program, VAO, proj);
            break;
        }

        case 'U':
            drawRect(currentX, y + thick, thick, charH - thick, color, program, VAO, proj);
            drawRect(currentX + charW - thick, y + thick, thick, charH - thick, color, program, VAO, proj);
            drawRect(currentX, y, charW, thick, color, program, VAO, proj);
            break;
        case 'W':
            drawRect(currentX, y, thick, charH, color, program, VAO, proj);
            drawRect(currentX + charW - thick, y, thick, charH, color, program, VAO, proj);
            drawRect(currentX, y, charW, thick, color, program, VAO, proj);
            drawRect(currentX + charW * 0.5f - thick * 0.5f, y + thick, thick, charH * 0.5f, color, program, VAO, proj);
            break;
        case 'I':
            drawRect(currentX + charW * 0.4f, y, thick, charH, color, program, VAO, proj);
            drawRect(currentX, y, charW, thick, color, program, VAO, proj);
            drawRect(currentX, y + charH - thick, charW, thick, color, program, VAO, proj);
            break;
        case 'N': {
            drawRect(currentX, y, thick, charH, color, program, VAO, proj);

            drawRect(currentX + charW - thick, y, thick, charH, color, program, VAO, proj);

            int segments = 10; 
            for (int i = 0; i < segments; i++) {
                float t = (float)i / segments;
                float segX = currentX + (1.0f - t) * (charW - thick);
                float segY = y + t * (charH - thick);
                drawRect(segX, segY, thick, thick, color, program, VAO, proj);
            }
            break;
        }

        case '!':
            drawRect(currentX + charW * 0.4f, y + charH * 0.3f, thick, charH * 0.7f, color, program, VAO, proj);
            drawRect(currentX + charW * 0.4f, y, thick, thick * 1.5f, color, program, VAO, proj);
            break;
        }

        currentX += charW + gap;
    }
}

int WINDOW_W = 800, WINDOW_H = 600;
bool keys[1024];

glm::vec2 getTextSize(const std::string& text, float scale) {
    float charW = 50.0f * scale;
    float gap = 15.0f * scale;
    int count = 0;
    for (char c : text)
        if (c != ' ') count++;
    float totalWidth = count * charW + (count - 1) * gap;
    float totalHeight = 70.0f * scale;
    return glm::vec2(totalWidth, totalHeight);
}


void key_callback(GLFWwindow* w, int key, int sc, int action, int mods) {
    if (action == GLFW_PRESS) keys[key] = true;
    else if (action == GLFW_RELEASE) keys[key] = false;
}

int main() {
    srand((unsigned int)time(nullptr));

    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_W, WINDOW_H, "Arkanoid Prototype", nullptr, nullptr);
    if (!window) { std::cerr << "Window create failed\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "GLAD failed\n"; return -1; }
    glViewport(0, 0, WINDOW_W, WINDOW_H);

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    GLuint program = linkProgram(quad_vs, quad_fs);
    GLuint rectProgram = linkProgram(rect_vs, rect_fs);
    GLuint VAO = createQuadVAO();
    GLuint rectVAO = createRectVAO();

    glm::mat4 proj = glm::ortho(0.0f, (float)WINDOW_W, 0.0f, (float)WINDOW_H, -1.0f, 1.0f);
    GLint loc_projection = glGetUniformLocation(program, "projection");
    GLint loc_model = glGetUniformLocation(program, "model");
    GLint loc_color = glGetUniformLocation(program, "color");
    GLint loc_tex = glGetUniformLocation(program, "tex");

    GLuint tex_brick = loadTexture("brick.png");
    GLuint tex_paddle = loadTexture("paddle.png");
    GLuint tex_ball = loadTexture("ball.png");
    GLuint tex_heart = loadTexture("heart.png");


    auto makeFallback = []() {
        GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
        unsigned char white[4] = { 255,255,255,255 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        return t;
        };
    if (!tex_brick) tex_brick = makeFallback();
    if (!tex_paddle) tex_paddle = makeFallback();
    if (!tex_ball) tex_ball = makeFallback();
    if (!tex_heart) tex_heart = makeFallback();

    Paddle paddle;
    paddle.size = glm::vec2(120.0f, 20.0f);
    paddle.pos = glm::vec2(WINDOW_W / 2.0f - paddle.size.x / 2.0f, 50.0f);
    paddle.tex = tex_paddle;

    std::vector<Ball> balls;
    Ball ball;
    ball.radius = 10.0f;
    ball.pos = glm::vec2(WINDOW_W / 2.0f, 200.0f);
    ball.vel = glm::vec2(200.0f, 200.0f);
    ball.tex = tex_ball;
    balls.push_back(ball);

    std::vector<PowerUp> powerUps;
    int lives = 5;
    bool gameOver = false;
    bool youWin = false;

    std::vector<Brick> bricks;
    int rows = 5, cols = 10;
    float margin = 10.0f;
    float brickW = (WINDOW_W - (cols + 1) * margin) / cols;
    float brickH = 30.0f;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            Brick b;
            b.alive = true;
            b.s.pos = glm::vec2(margin + c * (brickW + margin), WINDOW_H - (r + 1) * (brickH + 20.0f));
            b.s.size = glm::vec2(brickW, brickH);
            b.s.tex = tex_brick;
            b.s.color = glm::vec4(1.0f - r * 0.12f, 0.2f + r * 0.12f, 0.3f + c * 0.01f, 1.0f);
            b.s.transparent = false;
            bricks.push_back(b);
        }
    }

    double lastTime = glfwGetTime();
    glUseProgram(program);
    glUniformMatrix4fv(loc_projection, 1, GL_FALSE, &proj[0][0]);
    glUniform1i(loc_tex, 0);

    auto drawSprite = [&](const Sprite& s) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(s.pos, s.depth));
        model = glm::scale(model, glm::vec3(s.size, 1.0f));
        glUniformMatrix4fv(loc_model, 1, GL_FALSE, &model[0][0]);
        glUniform4fv(loc_color, 1, &s.color[0]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s.tex);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        };

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;
        glfwPollEvents();

        if (!gameOver && !youWin) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            paddle.pos.x = (float)xpos - paddle.size.x / 2.0f;
            if (paddle.pos.x < 0) paddle.pos.x = 0;
            if (paddle.pos.x + paddle.size.x > WINDOW_W) paddle.pos.x = WINDOW_W - paddle.size.x;

            for (auto& pu : powerUps) {
                if (!pu.active) continue;
                pu.pos += pu.vel * dt;
                if (pu.pos.y < -pu.size.y) {
                    pu.active = false;
                }
            }

            powerUps.erase(std::remove_if(powerUps.begin(), powerUps.end(),
                [](const PowerUp& pu) { return !pu.active; }), powerUps.end());

            bool ballLost = false;
            for (auto& ball : balls) {
                ball.pos += ball.vel * dt;
                if (ball.pos.x - ball.radius < 0) { ball.pos.x = ball.radius; ball.vel.x *= -1; }
                if (ball.pos.x + ball.radius > WINDOW_W) { ball.pos.x = WINDOW_W - ball.radius; ball.vel.x *= -1; }
                if (ball.pos.y + ball.radius > WINDOW_H) { ball.pos.y = WINDOW_H - ball.radius; ball.vel.y *= -1; }

                if (ball.pos.y - ball.radius < 0) {
                    ballLost = true;
                    break;
                }

                glm::vec2 closest;
                if (AABBvsCircle(paddle.pos, paddle.size, ball.pos, ball.radius, closest)) {
                    ball.vel.y = fabs(ball.vel.y);
                    float hitNorm = (ball.pos.x - (paddle.pos.x + paddle.size.x * 0.5f)) / (paddle.size.x * 0.5f);
                    ball.vel.x += hitNorm * 150.0f;
                }

                for (auto& b : bricks) {
                    if (!b.alive) continue;
                    if (AABBvsCircle(b.s.pos, b.s.size, ball.pos, ball.radius, closest)) {
                        b.alive = false;
                        glm::vec2 diff = ball.pos - closest;
                        if (fabs(diff.x) > fabs(diff.y)) ball.vel.x *= -1.0f;
                        else ball.vel.y *= -1.0f;

                        if (rand() % 100 < 30) {
                            PowerUp pu;
                            pu.pos = glm::vec2(b.s.pos.x + b.s.size.x * 0.5f, b.s.pos.y);
                            pu.vel = glm::vec2(0.0f, -100.0f);
                            pu.size = glm::vec2(30.0f, 30.0f);
                            pu.type = (PowerUpType)(rand() % 2); 
                            pu.active = true;
                            pu.tex = (pu.type == MULTIBALL) ? tex_ball : tex_heart;
                            powerUps.push_back(pu);
                        }
                        break;
                    }
                }
            }

            if (ballLost) {
                balls.erase(std::remove_if(balls.begin(), balls.end(),
                    [](const Ball& b) { return b.pos.y - b.radius < 0; }), balls.end());

                if (balls.empty()) {
                    lives--;
                    if (lives <= 0) {
                        gameOver = true;
                    }
                    else {
                        Ball newBall;
                        newBall.radius = 10.0f;
                        newBall.pos = glm::vec2(WINDOW_W / 2.0f, 200.0f);
                        newBall.vel = glm::vec2(200.0f, 200.0f);
                        newBall.tex = tex_ball;
                        balls.push_back(newBall);
                    }
                }
            }

            for (auto& pu : powerUps) {
                if (!pu.active) continue;
                if (pu.pos.x + pu.size.x > paddle.pos.x &&
                    pu.pos.x < paddle.pos.x + paddle.size.x &&
                    pu.pos.y + pu.size.y > paddle.pos.y &&
                    pu.pos.y < paddle.pos.y + paddle.size.y) {

                    if (pu.type == MULTIBALL) {
                        std::vector<Ball> newBalls;
                        int ballsToCreate = std::min(3, (int)balls.size());
                        for (int i = 0; i < ballsToCreate; i++) {
                            const Ball& existingBall = balls[i % balls.size()];
                            Ball newBall = existingBall;
                            float baseAngle = atan2(existingBall.vel.y, existingBall.vel.x);
                            float angleOffset = ((rand() % 90) - 45) * 3.14159f / 180.0f;
                            float newAngle = baseAngle + angleOffset;
                            float speed = glm::length(existingBall.vel);
                            newBall.vel.x = cos(newAngle) * speed;
                            newBall.vel.y = sin(newAngle) * speed;
                            if (newBall.vel.y < 0) newBall.vel.y = -newBall.vel.y;
                            newBalls.push_back(newBall);
                        }
                        balls.insert(balls.end(), newBalls.begin(), newBalls.end());
                    }
                    else if (pu.type == EXTRALIFE) {
                        lives++;
                    }

                    pu.active = false;
                }
            }

            bool allDestroyed = true;
            for (const auto& b : bricks) {
                if (b.alive) {
                    allDestroyed = false;
                    break;
                }
            }
            if (allDestroyed) {
                youWin = true;
            }
        }

        std::vector<Sprite> opaqueSprites;
        std::vector<Sprite> transparentSprites;

        Sprite spP;
        spP.pos = paddle.pos; spP.size = paddle.size; spP.tex = paddle.tex;
        spP.color = glm::vec4(1.0f); spP.transparent = false; spP.depth = 0.0f;
        opaqueSprites.push_back(spP);

        for (auto& b : bricks) {
            if (!b.alive) continue;
            Sprite sb = b.s;
            sb.depth = 0.0f;
            sb.transparent = false;
            opaqueSprites.push_back(sb);
        }

        for (const auto& ball : balls) {
            Sprite spB;
            spB.pos = ball.pos - glm::vec2(ball.radius);
            spB.size = glm::vec2(ball.radius * 2.0f);
            spB.tex = ball.tex;
            spB.color = glm::vec4(1.0f);
            spB.transparent = true;
            spB.depth = 0.0f;
            transparentSprites.push_back(spB);
        }

        for (const auto& pu : powerUps) {
            if (!pu.active) continue;
            Sprite spPU;
            spPU.pos = pu.pos;
            spPU.size = pu.size;
            spPU.tex = pu.tex;
            spPU.color = glm::vec4(1.0f, 1.0f, 0.5f, 1.0f); 
            spPU.transparent = true;
            spPU.depth = 0.0f;
            transparentSprites.push_back(spPU);
        }

        std::sort(transparentSprites.begin(), transparentSprites.end(), [](const Sprite& a, const Sprite& b) {
            if (a.depth != b.depth) return a.depth > b.depth;
            return a.pos.y > b.pos.y;
            });

        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);
        for (auto& s : opaqueSprites) drawSprite(s);
        for (auto& s : transparentSprites) drawSprite(s);
        float heartSize = 32.0f;
        float heartSpacing = 40.0f;
        for (int i = 0; i < lives; i++) {
            Sprite heart;
            heart.pos = glm::vec2(20.0f + i * heartSpacing, WINDOW_H - 50.0f);
            heart.size = glm::vec2(heartSize, heartSize);
            heart.tex = tex_heart;
            heart.transparent = true;
            drawSprite(heart);
        }

        if (gameOver) {
            drawRect(0, 0, WINDOW_W, WINDOW_H, glm::vec4(0.0f, 0.0f, 0.0f, 0.7f), rectProgram, rectVAO, proj);
            float scale = 1.5f;
            glm::vec2 textSize = getTextSize("GAME OVER", scale);
            float x = (WINDOW_W - textSize.x) / 2.0f;
            float y = (WINDOW_H - textSize.y) / 2.0f;
            drawBigText("GAME OVER", x, y, scale, glm::vec4(1.0f, 0.1f, 0.1f, 1.0f), rectProgram, rectVAO, proj);

        }

        if (youWin) {
            drawRect(0, 0, WINDOW_W, WINDOW_H, glm::vec4(0.0f, 0.0f, 0.0f, 0.7f), rectProgram, rectVAO, proj);
            drawBigText("YOU WIN!", 150.0f, WINDOW_H / 2.0f - 35.0f, 1.5f,
                glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), rectProgram, rectVAO, proj);
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
