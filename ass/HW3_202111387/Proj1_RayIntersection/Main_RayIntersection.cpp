// =============================================================================
//  Computer Graphics HW3 - Q1: Ray Intersection
//
//  A simple ray tracer that renders a plane + three spheres.
//  For each pixel we shoot an eye ray through the pixel center, find the
//  closest surface it hits, and color the pixel WHITE on a hit, BLACK otherwise.
//
//  OpenGL/GLFW are used ONLY to display the finished image buffer on screen
//  (via glDrawPixels). All ray-object intersection is computed on the CPU.
//
//  Classes: Ray, Camera, Surface (abstract), Plane, Sphere, Scene.
// =============================================================================

#include <Windows.h>
#include <iostream>
#include <GL/glew.h>
#include <GL/GL.h>
#include <GL/freeglut.h>

#define GLFW_INCLUDE_GLU
#define GLFW_DLL
#include <GLFW/glfw3.h>
#include <vector>
#include <limits>

#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

using namespace glm;

// -----------------------------------------------------------------------------
//  Ray : p(t) = origin + t * dir          (dir is not necessarily normalized)
// -----------------------------------------------------------------------------
class Ray {
public:
    vec3 origin;
    vec3 dir;
    Ray(const vec3& o, const vec3& d) : origin(o), dir(d) {}
    vec3 at(float t) const { return origin + t * dir; }
};

// -----------------------------------------------------------------------------
//  Surface : abstract base for anything a ray can hit.
//  intersect() returns true if the ray hits within (tMin, tMax) and writes the
//  closest hit parameter into tHit.
// -----------------------------------------------------------------------------
class Surface {
public:
    virtual bool intersect(const Ray& ray, float tMin, float tMax, float& tHit) const = 0;
    virtual ~Surface() {}
};

// -----------------------------------------------------------------------------
//  Sphere : |p - center|^2 = radius^2
//  Substituting the ray gives a quadratic a t^2 + b t + c = 0.
// -----------------------------------------------------------------------------
class Sphere : public Surface {
public:
    vec3  center;
    float radius;
    Sphere(const vec3& c, float r) : center(c), radius(r) {}

    bool intersect(const Ray& ray, float tMin, float tMax, float& tHit) const override {
        vec3  oc = ray.origin - center;
        float a  = dot(ray.dir, ray.dir);
        float b  = 2.0f * dot(oc, ray.dir);
        float c  = dot(oc, oc) - radius * radius;
        float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f) return false;          // no real root -> miss

        float sq = sqrtf(disc);
        float t  = (-b - sq) / (2.0f * a);       // nearer root first
        if (t < tMin || t > tMax) {
            t = (-b + sq) / (2.0f * a);          // try the farther root
            if (t < tMin || t > tMax) return false;
        }
        tHit = t;
        return true;
    }
};

// -----------------------------------------------------------------------------
//  Plane : the horizontal plane y = yValue (matches "y = -2" in the spec).
// -----------------------------------------------------------------------------
class Plane : public Surface {
public:
    float yValue;
    Plane(float y) : yValue(y) {}

    bool intersect(const Ray& ray, float tMin, float tMax, float& tHit) const override {
        if (fabsf(ray.dir.y) < 1e-8f) return false;          // ray parallel to plane
        float t = (yValue - ray.origin.y) / ray.dir.y;
        if (t < tMin || t > tMax) return false;
        tHit = t;
        return true;
    }
};

// -----------------------------------------------------------------------------
//  Camera : perspective camera. Builds an eye ray through the center of pixel
//  (ix, iy) following the standard image-plane mapping from the lecture.
//      u_s = l + (r - l) * (ix + 0.5) / nx
//      v_s = b + (t - b) * (iy + 0.5) / ny
//      dir = u_s * u + v_s * v - d * w
// -----------------------------------------------------------------------------
class Camera {
public:
    vec3  eye, u, v, w;
    float l, r, b, t, d;
    int   nx, ny;

    Camera(const vec3& eye_, const vec3& u_, const vec3& v_, const vec3& w_,
           float l_, float r_, float b_, float t_, float d_, int nx_, int ny_)
        : eye(eye_), u(u_), v(v_), w(w_),
          l(l_), r(r_), b(b_), t(t_), d(d_), nx(nx_), ny(ny_) {}

    Ray getRay(int ix, int iy) const {
        float us  = l + (r - l) * (ix + 0.5f) / nx;
        float vs  = b + (t - b) * (iy + 0.5f) / ny;
        vec3  dir = us * u + vs * v - d * w;     // camera looks along -w
        return Ray(eye, dir);
    }
};

// -----------------------------------------------------------------------------
//  Scene : holds the surfaces and finds the closest hit along a ray.
//  trace() returns true if ANY surface is hit (used for the white/black test).
// -----------------------------------------------------------------------------
class Scene {
public:
    std::vector<Surface*> surfaces;
    void add(Surface* s) { surfaces.push_back(s); }

    bool trace(const Ray& ray, float tMin, float tMax) const {
        bool  hit     = false;
        float closest = tMax;
        for (const Surface* s : surfaces) {
            float t;
            if (s->intersect(ray, tMin, closest, t)) {
                hit     = true;
                closest = t;                     // shrink the window to keep the nearest
            }
        }
        return hit;
    }
};

// -------------------------------------------------
// Global Variables
// -------------------------------------------------
int Width  = 1024;   // Q1 requires nx = ny = 1024
int Height = 1024;
std::vector<float> OutputImage; // RGB float buffer shown with glDrawPixels(...)
// -------------------------------------------------


void render()
{
    OutputImage.clear();
    OutputImage.reserve(Width * Height * 3);

    // --- Build the scene: plane y = -2 and three spheres at z = -7 ---
    Plane  plane(-2.0f);
    Sphere s1(vec3(-4.0f, 0.0f, -7.0f), 1.0f);
    Sphere s2(vec3( 0.0f, 0.0f, -7.0f), 2.0f);
    Sphere s3(vec3( 4.0f, 0.0f, -7.0f), 1.0f);

    Scene scene;
    scene.add(&plane);
    scene.add(&s1);
    scene.add(&s2);
    scene.add(&s3);

    // --- Camera from the spec (eye at origin, looking along -w) ---
    Camera camera(
        vec3(0.0f, 0.0f, 0.0f),                       // eye e
        vec3(1.0f, 0.0f, 0.0f),                       // u
        vec3(0.0f, 1.0f, 0.0f),                       // v
        vec3(0.0f, 0.0f, 1.0f),                       // w
        -0.1f, 0.1f, -0.1f, 0.1f,                     // l, r, b, t
        0.1f,                                         // d
        Width, Height);                               // nx, ny

    const float INF = std::numeric_limits<float>::infinity();

    // iy = 0 is the BOTTOM row (matches glDrawPixels' bottom-to-top order)
    for (int j = 0; j < Height; ++j)
    {
        for (int i = 0; i < Width; ++i)
        {
            Ray  ray   = camera.getRay(i, j);
            vec3 color = scene.trace(ray, 0.0f, INF) ? vec3(1.0f) : vec3(0.0f);

            OutputImage.push_back(color.r);
            OutputImage.push_back(color.g);
            OutputImage.push_back(color.b);
        }
    }
}


void resize_callback(GLFWwindow*, int nw, int nh)
{
    //This is called in response to the window resizing.
    //The new width and height are passed in so we make
    //any necessary changes:
    Width = nw;
    Height = nh;
    //Tell the viewport to use all of our screen estate
    glViewport(0, 0, nw, nh);

    //This is not necessary, we're just working in 2d so
    //why not let our spaces reflect it?
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(0.0, static_cast<double>(Width)
        , 0.0, static_cast<double>(Height)
        , 1.0, -1.0);

    //Reserve memory for our render so that we don't do
    //excessive allocations and render the image
    OutputImage.reserve(Width * Height * 3);
    render();
}


int main(int argc, char* argv[])
{
    // -------------------------------------------------
    // Initialize Window
    // -------------------------------------------------

    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(Width, Height, "HW3 Q1 - Ray Intersection", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    //We have an opengl context now. Everything from here on out
    //is just managing our window or opengl directly.

    //Tell the opengl state machine we don't want it to make
    //any assumptions about how pixels are aligned in memory
    //during transfers between host and device (like glDrawPixels(...) )
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    //We call our resize function once to set everything up initially
    //after registering it as a callback with glfw
    glfwSetFramebufferSizeCallback(window, resize_callback);
    resize_callback(NULL, Width, Height);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        //Clear the screen
        glClear(GL_COLOR_BUFFER_BIT);

        // -------------------------------------------------------------
        //Rendering begins!
        glDrawPixels(Width, Height, GL_RGB, GL_FLOAT, &OutputImage[0]);
        //and ends.
        // -------------------------------------------------------------

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();

        //Close when the user hits 'q' or escape
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
