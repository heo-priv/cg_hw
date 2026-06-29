// =============================================================================
//  Computer Graphics HW5 - Q1: Transformations and Rasterization
//
//  A simple SOFTWARE rasterizer (no OpenGL hardware rasterization). It draws a
//  single unit sphere (tessellated into triangles by the provided code) as an
//  UNSHADED white image using:
//    1) a transformation pipeline  model -> camera -> perspective -> viewport
//    2) triangle rasterization     (barycentric / edge-function coverage test)
//    3) a depth buffer             (keep the nearest fragment per pixel)
//  OpenGL/GLFW are used ONLY to show the finished pixel buffer via glDrawPixels.
//
//  Pipeline (from the lecture / Shirley):
//    M = M_viewport * M_ortho * M_persp * M_camera * M_model
//    screen = (M * v).xyz / (M * v).w
// =============================================================================

#define _USE_MATH_DEFINES
#define NOMINMAX
#include <Windows.h>
#include <iostream>
#include <GL/glew.h>
#include <GL/GL.h>
#include <GL/freeglut.h>

#define GLFW_INCLUDE_GLU
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <vector>
#include <algorithm>
#include <cmath>

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;

// -----------------------------------------------------------------------------
//  Sphere mesh (provided sphere_scene.cpp, with the vertex array filled in).
//  width=32, height=16  ->  450 vertices, 868 triangles.
// -----------------------------------------------------------------------------
static int               gNumVertices  = 0;
static int               gNumTriangles = 0;
static std::vector<int>  gIndexBuffer;
static std::vector<vec3> gVertices;

void create_scene()
{
    int width  = 32;
    int height = 16;

    gNumVertices  = (height - 2) * width + 2;
    gNumTriangles = (height - 2) * (width - 1) * 2;
    gVertices.resize(gNumVertices);
    gIndexBuffer.resize(3 * gNumTriangles);

    int t = 0;
    for (int j = 1; j < height - 1; ++j)
        for (int i = 0; i < width; ++i)
        {
            float theta = (float)j / (height - 1) * (float)M_PI;
            float phi   = (float)i / (width - 1)  * (float)M_PI * 2.0f;
            gVertices[t++] = vec3(sinf(theta) * cosf(phi),
                                  cosf(theta),
                                  -sinf(theta) * sinf(phi));
        }
    gVertices[t++] = vec3(0.0f,  1.0f, 0.0f);   // top pole
    gVertices[t++] = vec3(0.0f, -1.0f, 0.0f);   // bottom pole

    t = 0;
    for (int j = 0; j < height - 3; ++j)
        for (int i = 0; i < width - 1; ++i)
        {
            gIndexBuffer[t++] = j * width + i;
            gIndexBuffer[t++] = (j + 1) * width + (i + 1);
            gIndexBuffer[t++] = j * width + (i + 1);
            gIndexBuffer[t++] = j * width + i;
            gIndexBuffer[t++] = (j + 1) * width + i;
            gIndexBuffer[t++] = (j + 1) * width + (i + 1);
        }
    for (int i = 0; i < width - 1; ++i)
    {
        gIndexBuffer[t++] = (height - 2) * width;
        gIndexBuffer[t++] = i;
        gIndexBuffer[t++] = i + 1;
        gIndexBuffer[t++] = (height - 2) * width + 1;
        gIndexBuffer[t++] = (height - 3) * width + (i + 1);
        gIndexBuffer[t++] = (height - 3) * width + i;
    }
}

// -------------------------------------------------
// Global Variables
// -------------------------------------------------
int Width  = 1024;
int Height = 1024;
std::vector<float> OutputImage;
// -------------------------------------------------

// Build the perspective projection matrix (Shirley convention: l,r,b,t and the
// near/far PLANE Z-VALUES n,f, both negative). Returns M_ortho * M_persp.
static mat4 makeProjection(float l, float r, float b, float t, float n, float f)
{
    // perspective matrix: maps the frustum to an orthographic box, w := z
    mat4 P(0.0f);
    P[0][0] = n;
    P[1][1] = n;
    P[2][2] = n + f;
    P[2][3] = 1.0f;        // row3 = (0,0,1,0) -> w = z
    P[3][2] = -f * n;

    // orthographic matrix: box [l,r]x[b,t]x[n,f] -> [-1,1]^3
    mat4 O(0.0f);
    O[0][0] = 2.0f / (r - l);
    O[1][1] = 2.0f / (t - b);
    O[2][2] = 2.0f / (n - f);
    O[3][0] = -(r + l) / (r - l);
    O[3][1] = -(t + b) / (t - b);
    O[3][2] = -(n + f) / (n - f);
    O[3][3] = 1.0f;

    return O * P;
}

// Viewport matrix: NDC [-1,1]^2 -> screen [0,nx] x [0,ny] (z left unchanged).
static mat4 makeViewport(int nx, int ny)
{
    mat4 V(0.0f);
    V[0][0] = nx / 2.0f;  V[3][0] = nx / 2.0f;
    V[1][1] = ny / 2.0f;  V[3][1] = ny / 2.0f;
    V[2][2] = 1.0f;
    V[3][3] = 1.0f;
    return V;
}

static inline float edge(const vec3& a, const vec3& b, float px, float py)
{
    return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
}

void render()
{
    if (gVertices.empty()) create_scene();

    OutputImage.assign((size_t)Width * Height * 3, 0.0f);          // black background
    std::vector<float> depthBuffer((size_t)Width * Height, -1e30f); // keep MAX (nearer = larger ndc.z)

    // --- transformation pipeline ---
    // model: unit sphere -> radius 2, centered at (0,0,-7)
    mat4 Mmodel = translate(mat4(1.0f), vec3(0.0f, 0.0f, -7.0f)) *
                  scale(mat4(1.0f), vec3(2.0f));
    // camera: eye=(0,0,0), u/v/w = world axes, looking along -w  => identity
    mat4 Mcam = mat4(1.0f);
    mat4 Mproj = makeProjection(-0.1f, 0.1f, -0.1f, 0.1f, -0.1f, -1000.0f);
    mat4 Mvp   = makeViewport(Width, Height);

    mat4 M = Mvp * Mproj * Mcam * Mmodel;

    // project every vertex to screen space (sx, sy, depth=ndc.z)
    std::vector<vec3> scr(gNumVertices);
    for (int i = 0; i < gNumVertices; ++i)
    {
        vec4 q = M * vec4(gVertices[i], 1.0f);
        scr[i] = vec3(q.x / q.w, q.y / q.w, q.z / q.w);
    }

    // rasterize each triangle (flat white = "unshaded")
    for (int tri = 0; tri < gNumTriangles; ++tri)
    {
        const vec3& A = scr[gIndexBuffer[3 * tri + 0]];
        const vec3& B = scr[gIndexBuffer[3 * tri + 1]];
        const vec3& C = scr[gIndexBuffer[3 * tri + 2]];

        float area = edge(A, B, C.x, C.y);
        if (area == 0.0f) continue;                                // degenerate

        int minx = std::max(0,          (int)std::floor(std::min({A.x, B.x, C.x})));
        int maxx = std::min(Width  - 1, (int)std::ceil (std::max({A.x, B.x, C.x})));
        int miny = std::max(0,          (int)std::floor(std::min({A.y, B.y, C.y})));
        int maxy = std::min(Height - 1, (int)std::ceil (std::max({A.y, B.y, C.y})));

        for (int py = miny; py <= maxy; ++py)
            for (int px = minx; px <= maxx; ++px)
            {
                float x = px + 0.5f, y = py + 0.5f;
                float w0 = edge(B, C, x, y);
                float w1 = edge(C, A, x, y);
                float w2 = edge(A, B, x, y);

                // inside if all weights share the triangle's sign (winding-agnostic)
                bool inside = (w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                              (w0 <= 0 && w1 <= 0 && w2 <= 0);
                if (!inside) continue;

                float a = w0 / area, bb = w1 / area, c = w2 / area;
                float depth = a * A.z + bb * B.z + c * C.z;        // interpolate ndc.z

                size_t idx = (size_t)py * Width + px;
                if (depth > depthBuffer[idx])                       // nearer -> larger
                {
                    depthBuffer[idx] = depth;
                    OutputImage[idx * 3 + 0] = 1.0f;                // white (unshaded)
                    OutputImage[idx * 3 + 1] = 1.0f;
                    OutputImage[idx * 3 + 2] = 1.0f;
                }
            }
    }
}


void resize_callback(GLFWwindow*, int nw, int nh)
{
    Width = nw;
    Height = nh;
    glViewport(0, 0, nw, nh);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(Width), 0.0, static_cast<double>(Height), 1.0, -1.0);
    OutputImage.reserve(Width * Height * 3);
    render();
}


int main(int argc, char* argv[])
{
    GLFWwindow* window;
    if (!glfwInit())
        return -1;

    window = glfwCreateWindow(Width, Height, "HW5 Q1 - Rasterizer (unshaded sphere)", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    create_scene();                                  // build the sphere mesh once

    glfwSetFramebufferSizeCallback(window, resize_callback);
    resize_callback(NULL, Width, Height);

    while (!glfwWindowShouldClose(window))
    {
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawPixels(Width, Height, GL_RGB, GL_FLOAT, &OutputImage[0]);
        glfwSwapBuffers(window);
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS
            || glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
