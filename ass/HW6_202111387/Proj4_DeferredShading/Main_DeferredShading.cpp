// =============================================================================
//  Computer Graphics HW6 - Q4: New Feature (Deferred Shading)
//
//  Renders the sphere with a DEFERRED SHADING pipeline (two passes):
//    1) Geometry pass : rasterize the triangles and, instead of lighting them,
//       write per-pixel surface attributes into a G-buffer
//       (world position, normal, albedo) + the depth buffer.
//    2) Lighting pass : for every covered pixel, read the G-buffer and evaluate
//       Blinn-Phong ONCE per pixel, summed over ALL lights.
//  Because shading is decoupled from geometry, adding lights costs only the
//  lighting pass - here we use THREE colored lights to show that benefit
//  (one green-ish base sphere with multiple, differently colored highlights).
//  Gamma (2.2) per pixel. OpenGL/GLFW only display the buffer via glDrawPixels.
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
//  Sphere mesh (provided sphere_scene.cpp, vertex array filled in).
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
    gVertices[t++] = vec3(0.0f,  1.0f, 0.0f);
    gVertices[t++] = vec3(0.0f, -1.0f, 0.0f);

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

// ----- material / lighting (Blinn-Phong) -----
static const vec3  kA      = vec3(0.0f, 1.0f, 0.0f);   // ambient coeff
static const vec3  kS      = vec3(0.8f, 0.8f, 0.8f);   // specular coeff
static const float specPow = 64.0f;
static const float Ia      = 0.2f;                     // ambient intensity
static const vec3  eye     = vec3(0.0f);
static const vec3  center  = vec3(0.0f, 0.0f, -7.0f);

struct Light { vec3 pos; vec3 color; };

// Three lights -> deferred shading evaluates all of them in the lighting pass.
static const std::vector<Light> gLights = {
    { vec3(-4.0f,  4.0f, -3.0f), vec3(1.0f, 1.0f, 1.0f) },   // white, upper-left
    { vec3( 4.0f,  3.0f, -3.0f), vec3(1.0f, 0.3f, 0.1f) },   // warm,  upper-right
    { vec3(-1.0f, -4.0f, -3.0f), vec3(0.2f, 0.4f, 1.0f) },   // cool,  lower-left
};

// G-buffer (filled by the geometry pass, consumed by the lighting pass)
static std::vector<vec3>  gPosition;   // world position
static std::vector<vec3>  gNormal;     // world normal
static std::vector<vec3>  gAlbedo;     // diffuse color (kd)

// Blinn-Phong over all lights, using G-buffer attributes (returns linear color)
static vec3 lighting(const vec3& P, const vec3& Nin, const vec3& albedo)
{
    vec3 N = normalize(Nin);
    vec3 v = normalize(eye - P);
    vec3 result = kA * Ia;                                 // ambient
    for (const Light& L : gLights)
    {
        vec3 l = normalize(L.pos - P);
        vec3 h = normalize(l + v);
        result += albedo * L.color * std::max(0.0f, dot(N, l));               // diffuse
        result += kS     * L.color * std::pow(std::max(0.0f, dot(N, h)), specPow); // specular
    }
    return result;
}

static mat4 makeProjection(float l, float r, float b, float t, float n, float f)
{
    mat4 P(0.0f);
    P[0][0] = n; P[1][1] = n; P[2][2] = n + f; P[2][3] = 1.0f; P[3][2] = -f * n;
    mat4 O(0.0f);
    O[0][0] = 2.0f / (r - l); O[1][1] = 2.0f / (t - b); O[2][2] = 2.0f / (n - f);
    O[3][0] = -(r + l) / (r - l); O[3][1] = -(t + b) / (t - b); O[3][2] = -(n + f) / (n - f); O[3][3] = 1.0f;
    return O * P;
}

static mat4 makeViewport(int nx, int ny)
{
    mat4 V(0.0f);
    V[0][0] = nx / 2.0f; V[3][0] = nx / 2.0f;
    V[1][1] = ny / 2.0f; V[3][1] = ny / 2.0f;
    V[2][2] = 1.0f; V[3][3] = 1.0f;
    return V;
}

static inline float edge(const vec3& a, const vec3& b, float px, float py)
{
    return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
}

void render()
{
    if (gVertices.empty()) create_scene();

    size_t N = (size_t)Width * Height;
    OutputImage.assign(N * 3, 0.0f);
    std::vector<float> depthBuffer(N, -1e30f);
    gPosition.assign(N, vec3(0.0f));
    gNormal.assign(N, vec3(0.0f));
    gAlbedo.assign(N, vec3(0.0f));

    mat4 Mmodel = translate(mat4(1.0f), center) * scale(mat4(1.0f), vec3(2.0f));
    mat4 Mcam   = mat4(1.0f);
    mat4 Mproj  = makeProjection(-0.1f, 0.1f, -0.1f, 0.1f, -0.1f, -1000.0f);
    mat4 Mvp    = makeViewport(Width, Height);
    mat4 M      = Mvp * Mproj * Mcam * Mmodel;

    const vec3  albedo   = vec3(0.0f, 0.5f, 0.0f);   // kd (stored in the G-buffer)
    const float invGamma = 1.0f / 2.2f;

    // ---- PASS 1: GEOMETRY -> fill the G-buffer (no lighting here) ----
    std::vector<vec3> scr(gNumVertices), wPos(gNumVertices), nrm(gNumVertices);
    for (int i = 0; i < gNumVertices; ++i)
    {
        wPos[i] = vec3(Mmodel * vec4(gVertices[i], 1.0f));
        nrm[i]  = normalize(gVertices[i]);
        vec4 q  = M * vec4(gVertices[i], 1.0f);
        scr[i]  = vec3(q.x / q.w, q.y / q.w, q.z / q.w);
    }

    for (int tri = 0; tri < gNumTriangles; ++tri)
    {
        int i0 = gIndexBuffer[3 * tri + 0], i1 = gIndexBuffer[3 * tri + 1], i2 = gIndexBuffer[3 * tri + 2];
        const vec3& A = scr[i0]; const vec3& B = scr[i1]; const vec3& C = scr[i2];
        const vec3& pA = wPos[i0]; const vec3& pB = wPos[i1]; const vec3& pC = wPos[i2];
        const vec3& nA = nrm[i0];  const vec3& nB = nrm[i1];  const vec3& nC = nrm[i2];

        float area = edge(A, B, C.x, C.y);
        if (area == 0.0f) continue;

        int minx = std::max(0,          (int)std::floor(std::min({A.x, B.x, C.x})));
        int maxx = std::min(Width  - 1, (int)std::ceil (std::max({A.x, B.x, C.x})));
        int miny = std::max(0,          (int)std::floor(std::min({A.y, B.y, C.y})));
        int maxy = std::min(Height - 1, (int)std::ceil (std::max({A.y, B.y, C.y})));

        for (int py = miny; py <= maxy; ++py)
            for (int px = minx; px <= maxx; ++px)
            {
                float x = px + 0.5f, y = py + 0.5f;
                float w0 = edge(B, C, x, y), w1 = edge(C, A, x, y), w2 = edge(A, B, x, y);
                bool inside = (w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                              (w0 <= 0 && w1 <= 0 && w2 <= 0);
                if (!inside) continue;

                float a = w0 / area, bb = w1 / area, c = w2 / area;
                float depth = a * A.z + bb * B.z + c * C.z;

                size_t idx = (size_t)py * Width + px;
                if (depth > depthBuffer[idx])
                {
                    depthBuffer[idx] = depth;
                    gPosition[idx]   = a * pA + bb * pB + c * pC;   // store attributes
                    gNormal[idx]     = a * nA + bb * nB + c * nC;
                    gAlbedo[idx]     = albedo;
                }
            }
    }

    // ---- PASS 2: LIGHTING -> shade each covered pixel from the G-buffer ----
    for (size_t idx = 0; idx < N; ++idx)
    {
        if (depthBuffer[idx] <= -1e29f) continue;               // background (no geometry)
        vec3 col = lighting(gPosition[idx], gNormal[idx], gAlbedo[idx]);
        col = pow(clamp(col, 0.0f, 1.0f), vec3(invGamma));      // gamma
        OutputImage[idx * 3 + 0] = col.r;
        OutputImage[idx * 3 + 1] = col.g;
        OutputImage[idx * 3 + 2] = col.b;
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

    window = glfwCreateWindow(Width, Height, "HW6 Q4 - Deferred Shading", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    create_scene();

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
