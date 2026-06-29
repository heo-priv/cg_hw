// =============================================================================
//  Computer Graphics HW2 - Textured Cubes
//
//  Requirements covered:
//   1) Two differently textured cubes with different sizes and texture images
//      - Cube #1 : size 1.0, "brick.bmp"
//      - Cube #2 : size 0.6, "checker.bmp"
//   2) Keyboard + mouse interface to rotate and translate the scene
//   3) New feature : an additional shape (a textured square pyramid)
//
//  Self-contained single file: GLFW + GLEW + GLM, shaders are inlined,
//  textures are loaded from 24-bit .bmp files next to this source file.
//
//  Build target: Win32 (x86) - the bundled libs in ..\lib are 32-bit.
// =============================================================================

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace glm;

// -----------------------------------------------------------------------------
//  Globals describing the interactive transform of the whole scene
// -----------------------------------------------------------------------------
static float gRotX = 25.0f;   // pitch (degrees)
static float gRotY = -30.0f;  // yaw   (degrees)
static float gTransX = 0.0f;  // scene translation
static float gTransY = 0.0f;
static float gDist = 16.0f;   // camera distance (zoom)

// mouse drag state
static bool   gDragging = false;
static double gLastX = 0.0, gLastY = 0.0;

GLFWwindow* window = NULL;

// -----------------------------------------------------------------------------
//  Minimal 24-bit BMP loader. Tries a few candidate paths so the program runs
//  whether the working directory is the project folder or the output folder.
// -----------------------------------------------------------------------------
static GLuint loadBMP(const char* filename)
{
    const char* prefixes[] = { "", "HW2_202111387/", "../HW2_202111387/", "../../HW2_202111387/" };
    FILE* file = NULL;
    for (int i = 0; i < 4 && file == NULL; ++i) {
        std::string p = std::string(prefixes[i]) + filename;
        file = fopen(p.c_str(), "rb");
    }
    if (!file) {
        fprintf(stderr, "Could not open texture '%s'. Make sure it sits next to main.cpp.\n", filename);
        return 0;
    }

    unsigned char header[54];
    if (fread(header, 1, 54, file) != 54 || header[0] != 'B' || header[1] != 'M') {
        fprintf(stderr, "'%s' is not a valid BMP file.\n", filename);
        fclose(file);
        return 0;
    }

    unsigned int dataPos  = *(int*)&header[0x0A];
    unsigned int width    = *(int*)&header[0x12];
    unsigned int height   = *(int*)&header[0x16];

    unsigned int rowSize  = ((width * 3 + 3) / 4) * 4; // each row padded to 4 bytes
    unsigned int imageSize = rowSize * height;
    if (dataPos == 0) dataPos = 54;

    std::vector<unsigned char> data(imageSize);
    fseek(file, dataPos, SEEK_SET);
    fread(data.data(), 1, imageSize, file);
    fclose(file);

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    // BMP stores pixels as BGR, bottom-to-top -> upload as GL_BGR directly
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);

    printf("Loaded texture '%s' (%ux%u)\n", filename, width, height);
    return textureID;
}

// -----------------------------------------------------------------------------
//  Inline GLSL shaders
// -----------------------------------------------------------------------------
static const char* kVertexShader =
    "#version 330 core\n"
    "layout(location = 0) in vec3 vertexPosition;\n"
    "layout(location = 1) in vec2 vertexUV;\n"
    "out vec2 UV;\n"
    "uniform mat4 MVP;\n"
    "void main(){\n"
    "    gl_Position = MVP * vec4(vertexPosition, 1.0);\n"
    "    UV = vertexUV;\n"
    "}\n";

static const char* kFragmentShader =
    "#version 330 core\n"
    "in vec2 UV;\n"
    "out vec3 color;\n"
    "uniform sampler2D myTextureSampler;\n"
    "void main(){\n"
    "    color = texture(myTextureSampler, UV).rgb;\n"
    "}\n";

static GLuint compileShader(GLenum type, const char* src)
{
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, NULL);
    glCompileShader(id);
    GLint ok = GL_FALSE;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(id, 1024, NULL, log);
        fprintf(stderr, "Shader compile error:\n%s\n", log);
    }
    return id;
}

static GLuint createProgram()
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// -----------------------------------------------------------------------------
//  Geometry
// -----------------------------------------------------------------------------
// Unit cube centered at origin, spanning [-1, 1]. 36 vertices, per-face UVs 0..1.
static const GLfloat kCubePos[] = {
    // front (+Z)
    -1,-1, 1,  1,-1, 1,  1, 1, 1,  -1,-1, 1,  1, 1, 1, -1, 1, 1,
    // back (-Z)
     1,-1,-1, -1,-1,-1, -1, 1,-1,   1,-1,-1, -1, 1,-1,  1, 1,-1,
    // left (-X)
    -1,-1,-1, -1,-1, 1, -1, 1, 1,  -1,-1,-1, -1, 1, 1, -1, 1,-1,
    // right (+X)
     1,-1, 1,  1,-1,-1,  1, 1,-1,   1,-1, 1,  1, 1,-1,  1, 1, 1,
    // top (+Y)
    -1, 1, 1,  1, 1, 1,  1, 1,-1,  -1, 1, 1,  1, 1,-1, -1, 1,-1,
    // bottom (-Y)
    -1,-1,-1,  1,-1,-1,  1,-1, 1,  -1,-1,-1,  1,-1, 1, -1,-1, 1,
};

static const GLfloat kFaceUV[] = {
    0,0, 1,0, 1,1, 0,0, 1,1, 0,1   // repeated for each of the 6 faces below
};

// Square pyramid: base spans [-1,1] in X/Z at y=-1, apex at (0,1,0). 18 vertices.
static const GLfloat kPyramidPos[] = {
    // four triangular sides (base edge -> apex)
    -1,-1,-1,  1,-1,-1,  0, 1, 0,   // front
     1,-1,-1,  1,-1, 1,  0, 1, 0,   // right
     1,-1, 1, -1,-1, 1,  0, 1, 0,   // back
    -1,-1, 1, -1,-1,-1,  0, 1, 0,   // left
    // square base (two triangles)
    -1,-1,-1,  1,-1,-1,  1,-1, 1,
    -1,-1,-1,  1,-1, 1, -1,-1, 1,
};

static const GLfloat kPyramidUV[] = {
    0,0, 1,0, 0.5f,1,
    0,0, 1,0, 0.5f,1,
    0,0, 1,0, 0.5f,1,
    0,0, 1,0, 0.5f,1,
    0,0, 1,0, 1,1,
    0,0, 1,1, 0,1,
};

// -----------------------------------------------------------------------------
//  Input callbacks (mouse)
// -----------------------------------------------------------------------------
static void scrollCallback(GLFWwindow* w, double xoff, double yoff)
{
    gDist -= (float)yoff;                 // wheel up = zoom in
    if (gDist < 2.0f)  gDist = 2.0f;
    if (gDist > 40.0f) gDist = 40.0f;
}

static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            gDragging = true;
            glfwGetCursorPos(w, &gLastX, &gLastY);
        } else if (action == GLFW_RELEASE) {
            gDragging = false;
        }
    }
}

static void cursorPosCallback(GLFWwindow* w, double x, double y)
{
    if (!gDragging) return;
    double dx = x - gLastX;
    double dy = y - gLastY;
    gLastX = x; gLastY = y;
    gRotY += (float)dx * 0.4f;            // drag horizontally -> yaw
    gRotX += (float)dy * 0.4f;            // drag vertically   -> pitch
}

// Continuous keyboard handling (polled every frame for smooth motion)
static void processKeyboard(float dt)
{
    const float rotSpeed   = 90.0f * dt;  // degrees / second
    const float transSpeed = 4.0f  * dt;  // units / second
    const float zoomSpeed  = 8.0f  * dt;

    // rotate the scene
    if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) gRotY -= rotSpeed;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) gRotY += rotSpeed;
    if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) gRotX -= rotSpeed;
    if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) gRotX += rotSpeed;

    // translate the scene
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) gTransX -= transSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) gTransX += transSpeed;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) gTransY += transSpeed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) gTransY -= transSpeed;

    // zoom
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) gDist -= zoomSpeed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) gDist += zoomSpeed;
    if (gDist < 2.0f)  gDist = 2.0f;
    if (gDist > 40.0f) gDist = 40.0f;

    // reset
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        gRotX = 25.0f; gRotY = -30.0f; gTransX = gTransY = 0.0f; gDist = 16.0f;
    }
}

// Build a per-face UV buffer for the cube (same 0..1 mapping repeated 6 times)
static void buildCubeUV(std::vector<GLfloat>& out)
{
    out.clear();
    for (int f = 0; f < 6; ++f)
        for (int i = 0; i < 12; ++i)
            out.push_back(kFaceUV[i]);
}

int main(void)
{
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(1024, 768, "HW2 - Textured Cubes (202111387)", NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to open GLFW window.\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        glfwTerminate();
        return -1;
    }

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);

    printf("Controls:\n");
    printf("  Arrow keys : rotate scene\n");
    printf("  W/A/S/D    : translate scene\n");
    printf("  Q/E or wheel: zoom\n");
    printf("  Mouse drag : rotate scene\n");
    printf("  R          : reset view,  ESC: quit\n");

    glClearColor(0.10f, 0.12f, 0.20f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_MULTISAMPLE);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint program  = createProgram();
    GLuint matrixID = glGetUniformLocation(program, "MVP");
    GLuint texID    = glGetUniformLocation(program, "myTextureSampler");

    // ---- cube buffers ----
    std::vector<GLfloat> cubeUV;
    buildCubeUV(cubeUV);

    GLuint cubePosBuf, cubeUVBuf;
    glGenBuffers(1, &cubePosBuf);
    glBindBuffer(GL_ARRAY_BUFFER, cubePosBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubePos), kCubePos, GL_STATIC_DRAW);
    glGenBuffers(1, &cubeUVBuf);
    glBindBuffer(GL_ARRAY_BUFFER, cubeUVBuf);
    glBufferData(GL_ARRAY_BUFFER, cubeUV.size() * sizeof(GLfloat), cubeUV.data(), GL_STATIC_DRAW);

    // ---- pyramid buffers ----
    GLuint pyrPosBuf, pyrUVBuf;
    glGenBuffers(1, &pyrPosBuf);
    glBindBuffer(GL_ARRAY_BUFFER, pyrPosBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kPyramidPos), kPyramidPos, GL_STATIC_DRAW);
    glGenBuffers(1, &pyrUVBuf);
    glBindBuffer(GL_ARRAY_BUFFER, pyrUVBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kPyramidUV), kPyramidUV, GL_STATIC_DRAW);

    // ---- textures ----
    GLuint texBrick   = loadBMP("brick.bmp");
    GLuint texChecker = loadBMP("checker.bmp");

    glm::mat4 Projection = glm::perspective(glm::radians(45.0f), 1024.0f / 768.0f, 0.1f, 100.0f);

    // helper lambda to draw a buffer pair with a given model matrix + texture
    auto drawObject = [&](GLuint posBuf, GLuint uvBuf, int vertexCount,
                          const glm::mat4& model, GLuint texture)
    {
        glm::mat4 MVP = Projection *
            glm::lookAt(glm::vec3(0, 0, gDist), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0)) *
            model;
        glUniformMatrix4fv(matrixID, 1, GL_FALSE, glm::value_ptr(MVP));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(texID, 0);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, posBuf);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, uvBuf);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

        glDrawArrays(GL_TRIANGLES, 0, vertexCount);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    };

    double lastTime = glfwGetTime();

    do {
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;

        processKeyboard(dt);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(program);

        // Scene transform shared by all objects (translate then rotate)
        glm::mat4 scene = glm::translate(glm::mat4(1.0f), glm::vec3(gTransX, gTransY, 0.0f));
        scene = glm::rotate(scene, glm::radians(gRotX), glm::vec3(1, 0, 0));
        scene = glm::rotate(scene, glm::radians(gRotY), glm::vec3(0, 1, 0));

        // Cube #1 : big, brick texture, on the left
        glm::mat4 m1 = glm::translate(scene, glm::vec3(-2.4f, 0.0f, 0.0f));
        drawObject(cubePosBuf, cubeUVBuf, 36, m1, texBrick);

        // Cube #2 : small (0.6x), checker texture, on the right
        glm::mat4 m2 = glm::translate(scene, glm::vec3(2.2f, 0.4f, 0.0f));
        m2 = glm::scale(m2, glm::vec3(0.6f));
        drawObject(cubePosBuf, cubeUVBuf, 36, m2, texChecker);

        // New feature : a textured square pyramid in front
        glm::mat4 mp = glm::translate(scene, glm::vec3(0.0f, -0.3f, 2.4f));
        mp = glm::scale(mp, glm::vec3(1.3f));
        drawObject(pyrPosBuf, pyrUVBuf, 18, mp, texChecker);

        glfwSwapBuffers(window);
        glfwPollEvents();

    } while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
             glfwWindowShouldClose(window) == 0);

    glDeleteBuffers(1, &cubePosBuf);
    glDeleteBuffers(1, &cubeUVBuf);
    glDeleteBuffers(1, &pyrPosBuf);
    glDeleteBuffers(1, &pyrUVBuf);
    glDeleteProgram(program);
    glDeleteTextures(1, &texBrick);
    glDeleteTextures(1, &texChecker);
    glDeleteVertexArrays(1, &vao);

    glfwTerminate();
    return 0;
}
