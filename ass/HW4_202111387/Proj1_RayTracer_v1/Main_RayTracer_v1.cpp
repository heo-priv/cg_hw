// =============================================================================
//  Computer Graphics HW4 - Q1: RayTracer_v1 (baseline, from HW3)
//
//  The HW3 ray tracer: plane y=-2 + three spheres at z=-7, shaded with the
//  Phong illumination model:
//      L = ka*Ia + sum_lights [ kd*I*max(0, n.l) + ks*I*max(0, r.v)^p ]
//  A single point light at (-4,4,-3) (white, unit intensity, no falloff) and a
//  white unit-intensity ambient light are used. Hard shadows via shadow rays.
//  NOTE: this is the raw linear result with NO gamma correction - gamma is
//  added in Q2, anti-aliasing in Q3. Output must match the HW4 Q1 reference.
//
//  OpenGL/GLFW are used ONLY to show the image buffer (glDrawPixels). All ray
//  tracing and shading is done on the CPU.
//
//  Classes: Ray, Material, Hit, Camera, Light, Surface, Plane, Sphere, Scene.
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
#include <algorithm>
#include <cmath>

#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

using namespace glm;

// -----------------------------------------------------------------------------
//  Ray : p(t) = origin + t * dir
// -----------------------------------------------------------------------------
class Ray {
public:
    vec3 origin;
    vec3 dir;
    Ray(const vec3& o, const vec3& d) : origin(o), dir(d) {}
    vec3 at(float t) const { return origin + t * dir; }
};

// -----------------------------------------------------------------------------
//  Material : Phong coefficients (per RGB channel) and specular exponent.
// -----------------------------------------------------------------------------
struct Material {
    vec3  ka;       // ambient
    vec3  kd;       // diffuse
    vec3  ks;       // specular
    float specPow;  // specular power (shininess)
    Material() : ka(0), kd(0), ks(0), specPow(0) {}
    Material(const vec3& a, const vec3& d, const vec3& s, float p)
        : ka(a), kd(d), ks(s), specPow(p) {}
};

// -----------------------------------------------------------------------------
//  Hit : information recorded at a ray-surface intersection.
// -----------------------------------------------------------------------------
struct Hit {
    float           t;
    vec3            point;
    vec3            normal;     // unit, outward
    const Material* mat;
};

// -----------------------------------------------------------------------------
//  Light : point light with an RGB intensity.
// -----------------------------------------------------------------------------
struct Light {
    vec3 position;
    vec3 intensity;
    Light(const vec3& p, const vec3& I) : position(p), intensity(I) {}
};

// -----------------------------------------------------------------------------
//  Surface : abstract base. Holds a Material; reports the closest hit in range.
// -----------------------------------------------------------------------------
class Surface {
public:
    Material material;
    virtual bool intersect(const Ray& ray, float tMin, float tMax, Hit& rec) const = 0;
    virtual ~Surface() {}
};

// -----------------------------------------------------------------------------
//  Sphere
// -----------------------------------------------------------------------------
class Sphere : public Surface {
public:
    vec3  center;
    float radius;
    Sphere(const vec3& c, float r, const Material& m) : center(c), radius(r) { material = m; }

    bool intersect(const Ray& ray, float tMin, float tMax, Hit& rec) const override {
        vec3  oc = ray.origin - center;
        float a  = dot(ray.dir, ray.dir);
        float b  = 2.0f * dot(oc, ray.dir);
        float c  = dot(oc, oc) - radius * radius;
        float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f) return false;

        float sq = sqrtf(disc);
        float t  = (-b - sq) / (2.0f * a);
        if (t < tMin || t > tMax) {
            t = (-b + sq) / (2.0f * a);
            if (t < tMin || t > tMax) return false;
        }
        rec.t      = t;
        rec.point  = ray.at(t);
        rec.normal = normalize(rec.point - center);
        rec.mat    = &material;
        return true;
    }
};

// -----------------------------------------------------------------------------
//  Plane : horizontal plane y = yValue, normal pointing up (+y).
// -----------------------------------------------------------------------------
class Plane : public Surface {
public:
    float yValue;
    Plane(float y, const Material& m) : yValue(y) { material = m; }

    bool intersect(const Ray& ray, float tMin, float tMax, Hit& rec) const override {
        if (fabsf(ray.dir.y) < 1e-8f) return false;
        float t = (yValue - ray.origin.y) / ray.dir.y;
        if (t < tMin || t > tMax) return false;
        rec.t      = t;
        rec.point  = ray.at(t);
        rec.normal = vec3(0.0f, 1.0f, 0.0f);
        rec.mat    = &material;
        return true;
    }
};

// -----------------------------------------------------------------------------
//  Camera : same perspective camera as Q1.
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
        vec3  dir = us * u + vs * v - d * w;
        return Ray(eye, dir);
    }
};

// -----------------------------------------------------------------------------
//  Scene : surfaces + lights. Finds closest hit; tests shadow-ray occlusion.
// -----------------------------------------------------------------------------
class Scene {
public:
    std::vector<Surface*> surfaces;
    std::vector<Light>    lights;
    vec3                  ambient;   // ambient light intensity Ia

    void add(Surface* s) { surfaces.push_back(s); }
    void add(const Light& L) { lights.push_back(L); }

    bool hit(const Ray& ray, float tMin, float tMax, Hit& rec) const {
        bool  found   = false;
        float closest = tMax;
        Hit   tmp;
        for (const Surface* s : surfaces) {
            if (s->intersect(ray, tMin, closest, tmp)) {
                found   = true;
                closest = tmp.t;
                rec     = tmp;
            }
        }
        return found;
    }

    // is there any surface between (tMin, tMax) along this ray? (shadow test)
    bool occluded(const Ray& ray, float tMin, float tMax) const {
        Hit tmp;
        for (const Surface* s : surfaces)
            if (s->intersect(ray, tMin, tMax, tmp)) return true;
        return false;
    }
};

// -----------------------------------------------------------------------------
//  Phong shading for a single hit point.
// -----------------------------------------------------------------------------
static vec3 shade(const Scene& scene, const Ray& ray, const Hit& rec)
{
    const Material& m = *rec.mat;

    // normal facing against the incoming ray
    vec3 n = rec.normal;
    if (dot(n, ray.dir) > 0.0f) n = -n;

    vec3 p = rec.point;
    vec3 v = normalize(-ray.dir);              // toward the eye

    // ambient term (always present)
    vec3 color = m.ka * scene.ambient;

    for (const Light& light : scene.lights) {
        vec3  toLight = light.position - p;
        float dist    = length(toLight);
        vec3  l       = toLight / dist;

        // shadow ray: offset a touch off the surface to avoid self-hit
        Ray shadowRay(p + n * 1e-4f, l);
        if (scene.occluded(shadowRay, 1e-4f, dist - 1e-4f))
            continue;                          // in shadow -> skip diffuse+specular

        float ndotl   = std::max(0.0f, dot(n, l));
        vec3  diffuse = m.kd * light.intensity * ndotl;

        // Phong specular: reflect light dir about the normal, compare to view
        vec3  r  = reflect(-l, n);             // = 2*(n.l)*n - l
        float rv = std::max(0.0f, dot(r, v));
        vec3  specular = m.ks * light.intensity * powf(rv, m.specPow);

        color += diffuse + specular;
    }
    return color;
}

// -------------------------------------------------
// Global Variables
// -------------------------------------------------
int Width  = 1024;   // same resolution as Q1
int Height = 1024;
std::vector<float> OutputImage;
// -------------------------------------------------


void render()
{
    OutputImage.clear();
    OutputImage.reserve(Width * Height * 3);

    // --- Materials from the spec (ka, kd, ks, specular power) ---
    Material matPlane(vec3(0.2f, 0.2f, 0.2f), vec3(1.0f, 1.0f, 1.0f), vec3(0.0f), 0.0f);
    Material matS1   (vec3(0.2f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f), vec3(0.0f), 0.0f);
    Material matS2   (vec3(0.0f, 0.2f, 0.0f), vec3(0.0f, 0.5f, 0.0f), vec3(0.5f, 0.5f, 0.5f), 32.0f);
    Material matS3   (vec3(0.0f, 0.0f, 0.2f), vec3(0.0f, 0.0f, 1.0f), vec3(0.0f), 0.0f);

    // --- Scene ---
    Plane  plane(-2.0f, matPlane);
    Sphere s1(vec3(-4.0f, 0.0f, -7.0f), 1.0f, matS1);
    Sphere s2(vec3( 0.0f, 0.0f, -7.0f), 2.0f, matS2);
    Sphere s3(vec3( 4.0f, 0.0f, -7.0f), 1.0f, matS3);

    Scene scene;
    scene.add(&plane);
    scene.add(&s1);
    scene.add(&s2);
    scene.add(&s3);
    scene.ambient = vec3(1.0f, 1.0f, 1.0f);                       // white ambient, unit intensity
    scene.add(Light(vec3(-4.0f, 4.0f, -3.0f), vec3(1.0f)));       // white point light, unit intensity

    // --- Camera (identical to Q1) ---
    Camera camera(
        vec3(0.0f, 0.0f, 0.0f),
        vec3(1.0f, 0.0f, 0.0f),
        vec3(0.0f, 1.0f, 0.0f),
        vec3(0.0f, 0.0f, 1.0f),
        -0.1f, 0.1f, -0.1f, 0.1f,
        0.1f,
        Width, Height);

    const float INF = std::numeric_limits<float>::infinity();

    for (int j = 0; j < Height; ++j)
    {
        for (int i = 0; i < Width; ++i)
        {
            Ray  ray = camera.getRay(i, j);
            vec3 color(0.0f);                                    // background = black

            Hit rec;
            if (scene.hit(ray, 0.0f, INF, rec))
                color = shade(scene, ray, rec);

            color = clamp(color, 0.0f, 1.0f);                    // clamp only - NO gamma in Q1

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
    window = glfwCreateWindow(Width, Height, "HW4 Q1 - RayTracer_v1", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

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
