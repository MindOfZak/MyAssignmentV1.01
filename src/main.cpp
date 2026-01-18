#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp> 

#include "shader.h"
#include "Mesh.h"
//#include "Node.h"


static Shader shader;

glm::mat4 matModelRoot = glm::mat4(1.0);
glm::mat4 matView = glm::mat4(1.0);
glm::mat4 matProj = glm::ortho(-2.0f,2.0f,-2.0f,2.0f, -2.0f,2.0f);

glm::vec3 lightPos = glm::vec3(5.0f, 5.0f, 10.0f);
glm::vec3 viewPos_default = glm::vec3(0.0f, 2.0f, 6.0f);
glm::vec3 viewPos = viewPos_default;

// Floor render mode: filled (false) or grid/wire (true)
static bool gFloorWireframe = false;

// Current picked mesh index (for basic object movement)
static int gPickedIndex = -1;

// We are using mesh list instead of scenegraph to demo our picking and collision detection
std::vector< std::shared_ptr <Mesh> > meshList;
std::vector< glm::mat4 > meshMatList;

// GLuint flatShader;
GLuint blinnShader;
GLuint phongShader;
// added for LabA07
GLuint texblinnShader;
// simple lit shader for the procedural floor
GLuint floorShader;

// Initialize shader
GLuint initShader(std::string pathVert, std::string pathFrag) 
{
    shader.read_source( pathVert.c_str(), pathFrag.c_str());

    shader.compile();
    glUseProgram(shader.program);

    return shader.program;
}

void setLightPosition(glm::vec3 lightPos)
{
    GLuint lightpos_loc = glGetUniformLocation(shader.program, "lightPos" );
    glUniform3fv(lightpos_loc, 1, glm::value_ptr(lightPos));
}

void setViewPosition(glm::vec3 eyePos)
{
    GLuint viewpos_loc = glGetUniformLocation(shader.program, "viewPos" );
    glUniform3fv(viewpos_loc, 1, glm::value_ptr(eyePos));
}

// Same as above, but for an explicit shader program id
void setViewPositionForProgram(GLuint programId, const glm::vec3 &eyePos)
{
    glUseProgram(programId);
    GLuint viewpos_loc = glGetUniformLocation(programId, "viewPos");
    if (viewpos_loc != (GLuint)-1)
        glUniform3fv(viewpos_loc, 1, glm::value_ptr(eyePos));
}


glm::vec3 screenPosToRay(int mouseX, int mouseY, int w, int h,
                         const glm::mat4 &proj, const glm::mat4 &view);

void mouse_button_callback(GLFWwindow *win, int button, int action, int mods);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);


glm::vec3 screenPosToRay(int mouseX, int mouseY, int w, int h,
                         const glm::mat4 &proj, const glm::mat4 &view)
{
    float x = (2.0f * mouseX) / w - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / h;

    // clicked point on the near plane in NDC space
    // if we assume the eye space is scaled so that z_{near} = -1.0
    // then this NDC coordinate is the same as its clip coordinate
    glm::vec4 ray_clip(x, y, -1.0f, 1.0f);

    // set one point with (x, y) and z = -1.0 in eye/camera space
    // the camera is located at (0, 0, 0)
    glm::vec4 ray_eye = glm::inverse(proj) * ray_clip;

    // ray direction in scaled eye space: z_{near} = -1.0
    ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);

    // convert the vector to the word space
    glm::vec3 ray_world = glm::normalize(glm::vec3(glm::inverse(view) * ray_eye));
    return ray_world;
}

void mouse_button_callback(GLFWwindow *win, int button, int action, int mods)
{
    
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        double mx, my;
        glfwGetCursorPos(win, &mx, &my);

        std::cout << "Mouse click at: (" << mx <<", " << my << ")" << std::endl;

        int w, h;
        glfwGetWindowSize(win, &w, &h);

        glm::vec3 rayOrig = viewPos;
        glm::vec3 rayDir = screenPosToRay((int)mx, (int)my, w, h, matProj, matView);

        Ray ray{rayOrig, rayDir};

        
        // Pick the closest hit only (so you don't select multiple meshes at once)
        float bestT = FLT_MAX;
        std::shared_ptr<Mesh> bestMesh = nullptr;

        // Clear previous selection
        gPickedIndex = -1;
        for (auto &pMesh : meshList)
            pMesh->setPicked(false);

        for (int i = 0; i < (int)meshList.size(); i++)
        {
            auto &pMesh = meshList[i];
            if (!pMesh->pSpatial) continue;
            HitInfo hit;
            if (pMesh->pSpatial->Raycast(ray, hit))
            {
                if (hit.t < bestT)
                {
                    bestT = hit.t;
                    bestMesh = pMesh;
                    gPickedIndex = i;
                }
            }
        }

        if (bestMesh)
        {
            bestMesh->setPicked(true);
            std::cout << "Picked mesh with hit t=" << bestT << std::endl;
        }
        else
        {
            std::cout << "No objects picked" << std::endl;
        }
    }
}


int main()
{
    GLFWwindow *window;

    // GLFW init
    if (!glfwInit())
    {
        std::cout << "glfw failed" << std::endl;
        return -1;
    }

    // create a GLFW window
    window = glfwCreateWindow(800, 800, "Hello OpenGL 11", NULL, NULL);
    glfwMakeContextCurrent(window);

    // register the key event callback function
    glfwSetKeyCallback(window, key_callback);
    
    // register the mouse button event callback function
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // loading glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Couldn't load opengl" << std::endl;
        glfwTerminate();
        return -1;
    }

    phongShader = initShader( "shaders/blinn.vert", "shaders/phong.frag");
    setLightPosition(lightPos);
    setViewPosition(viewPos);
    blinnShader = initShader( "shaders/blinn.vert", "shaders/blinn.frag");
    setLightPosition(lightPos);
    setViewPosition(viewPos);
    // added for LabA07
    texblinnShader = initShader("shaders/texblinn.vert", "shaders/texblinn.frag");
    setLightPosition(lightPos);
    setViewPosition(viewPos);

    // procedural floor shader (lit, simple colour)
    floorShader = initShader("shaders/blinn.vert", "shaders/floor.frag");
    setLightPosition(lightPos);
    setViewPosition(viewPos);

    // set the eye at (0, 0, 5), looking at the centre of the world
    // try to change the eye position
    viewPos = glm::vec3(0.0f, 2.0f, 5.0f);
    matView = glm::lookAt(viewPos, glm::vec3(0, 0, -10), glm::vec3(0, 1, 0)); 

    // set the Y field of view angle to 60 degrees, width/height ratio to 1.0, and a near plane of 3.5, far plane of 6.5
    // try to play with the FoV
    //matProj = glm::perspective(glm::radians(60.0f), 1.0f, 2.0f, 8.0f);
    // setting to a close near plane and a farway far plane to test collision detection
    matProj = glm::perspective(glm::radians(60.0f), 1.0f, 0.5f, 20.0f);

    //----------------------------------------------------
    // Procedural floor grid (triangles)
    // Generates a simple XZ grid made of triangles centred at origin.
    std::shared_ptr<Mesh> floor = std::make_shared<Mesh>();
    {
        const int N = 40;            // number of squares per side
        const float size = 20.0f;    // world size
        const float half = size * 0.5f;
        const float step = size / (float)N;

        std::vector<Vertex> v;
        std::vector<unsigned int> idx;
        v.reserve((N + 1) * (N + 1));
        idx.reserve(N * N * 6);

        // vertices
        for (int z = 0; z <= N; z++)
        {
            for (int x = 0; x <= N; x++)
            {
                Vertex vert;
                vert.pos = glm::vec3(-half + x * step, 0.0f, -half + z * step);
                vert.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                // tile UVs so a texture could be used later (optional)
                vert.texCoord = glm::vec2((float)x / (float)N, (float)z / (float)N);
                v.push_back(vert);
            }
        }

        // indices (two triangles per cell)
        auto vid = [N](int x, int z) { return (unsigned int)(z * (N + 1) + x); };
        for (int z = 0; z < N; z++)
        {
            for (int x = 0; x < N; x++)
            {
                unsigned int i0 = vid(x, z);
                unsigned int i1 = vid(x + 1, z);
                unsigned int i2 = vid(x, z + 1);
                unsigned int i3 = vid(x + 1, z + 1);

                idx.push_back(i0);
                idx.push_back(i2);
                idx.push_back(i1);

                idx.push_back(i1);
                idx.push_back(i2);
                idx.push_back(i3);
            }
        }

        // Use dedicated lit floor shader (grey)
        floor->initFromData(v, idx, floorShader);
    }

    glm::mat4 mat = glm::mat4(1.0);

    
    std::shared_ptr<Mesh> teapot = std::make_shared<Mesh>();
    // Untextured mesh (demonstrates Blinn-Phong lighting without texture)
    teapot->init("models/teapot.obj", blinnShader);
    meshList.push_back(teapot);
    mat = glm::translate(glm::vec3(-2.0f, 1.0f, 0.0f));
    meshMatList.push_back(mat); // TRS
    teapot->initSpatial(true, mat);
    
    std::shared_ptr<Mesh> bunny = std::make_shared<Mesh>();
    bunny->init("models/bunny_normal.obj", texblinnShader);
    meshList.push_back(bunny);
    mat = glm::translate(glm::vec3(1.5f, 1.5f, 0.0f)) *
          glm::scale(glm::vec3(0.005f, 0.005f, 0.005f));
    meshMatList.push_back( mat ); // TRS
    bunny->initSpatial(true, mat);

    // NOTE: We don't include the floor in meshList, because meshList is used for
    // picking/collision demos. We draw the floor separately.
  

    //----------------------------------------------------
    // Nodes
    // std::shared_ptr<Node> scene = std::make_shared<Node>();
    // std::shared_ptr<Node> teapotNode = std::make_shared<Node>();
    // std::shared_ptr<Node> cubeNode = std::make_shared<Node>();
    // std::shared_ptr<Node> bunnyNode = std::make_shared<Node>();
    
    //----------------------------------------------------
    // Build the tree
    //teapotNode->addMesh(teapot);
    //cubeNode->addMesh(cube, glm::mat4(1.0), glm::mat4(1.0), glm::scale(glm::vec3(2.0f, 0.25f, 1.5f)));
    //bunnyNode->addMesh(bunny, glm::mat4(1.0), glm::mat4(1.0), glm::scale(glm::vec3(0.005f, 0.005f, 0.005f)));


    // cubeNode->addChild(teapotNode, glm::translate(glm::vec3(-1.5f, 0.5f, 0.0f)));
    // cubeNode->addChild(bunnyNode, glm::translate(glm::vec3(1.0f, 1.5f, 0.0f)));
    // cubeNode->addChild(teapotNode, glm::translate(glm::vec3(0.0f, 1.0f, 0.0f)), glm::rotate(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
    
    //----------------------------------------------------
    // Add the tree to the world space
    //scene->addChild(cubeNode);
    //scene->addChild(bunnyNode);
    // scene->addChild(cubeNode, glm::translate(glm::vec3(1.0f, 0.0f, 0.0f)), glm::rotate(glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f)));

    // setting the background colour, you can change the value
    glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
    
    glEnable(GL_DEPTH_TEST);
    //glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

    // setting the event loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Keep lighting correct when the camera moves
        setViewPositionForProgram(phongShader, viewPos);
        setViewPositionForProgram(blinnShader, viewPos);
        setViewPositionForProgram(texblinnShader, viewPos);
        setViewPositionForProgram(floorShader, viewPos);

        //scene->draw(matModelRoot, matView, matProj);
        // bunny->draw(glm::scale(glm::vec3(0.005f, 0.005f, 0.005f)), matView, matProj);


        // Draw floor (filled or grid mode)
        if (gFloorWireframe)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // Optional: set floor base colour
        glUseProgram(floorShader);
        GLint colLoc = glGetUniformLocation(floorShader, "baseColor");
        if (colLoc >= 0) glUniform3f(colLoc, 0.6f, 0.6f, 0.6f);

        floor->draw(matModelRoot, matView, matProj);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // Draw models
        for (int i = 0; i < (int)meshList.size(); i++ ) {
            std::shared_ptr<Mesh> pMesh = meshList[i];
            pMesh->draw(matModelRoot * meshMatList[i], matView, matProj);
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();

    return 0;
}


void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    glm::mat4 mat = glm::mat4(1.0);

    float angleStep = 5.0f;
    float transStep = 1.0f;
    float objStep = 0.25f; // object movement step

    if (action == GLFW_PRESS)
    {
        // Toggle floor wireframe (grid mode)
        if (GLFW_KEY_G == key)
        {
            gFloorWireframe = !gFloorWireframe;
            return;
        }

        /*
        // we don't allow objects to move for picking and collision detection
        if (mods & GLFW_MOD_CONTROL) {
            // translation in world space
            if (GLFW_KEY_LEFT == key) {
                mat = glm::translate(glm::mat4(1.0f), glm::vec3(transStep, 0.0f, 0.0f));
                matModelRoot = mat * matModelRoot;
            } else if (GLFW_KEY_RIGHT == key) {
                mat = glm::translate(glm::mat4(1.0f), glm::vec3(-transStep, 0.0f, 0.0f));
            } else if (GLFW_KEY_UP == key) {
                mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, transStep, 0.0f));
                matModelRoot = mat * matModelRoot;
            } else if (GLFW_KEY_DOWN == key) {
                mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -transStep, 0.0f));
            }
            matModelRoot = mat * matModelRoot;
        }
        */

        if (GLFW_KEY_R == key)
        {
            // std::cout << "R pressed" << std::endl;
            //  reset
            viewPos = viewPos_default;
            matView = glm::lookAt(viewPos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
            matModelRoot = glm::mat4(1.0f);

            return;
        } 

        // ------------------------------------------------------------
        // Basic object movement: hold SHIFT and use arrow keys / +/- to
        // move the currently selected (picked) mesh.
        // This satisfies the "Model move" basic requirement.
        if ((mods & GLFW_MOD_SHIFT) && gPickedIndex >= 0 && gPickedIndex < (int)meshList.size())
        {
            glm::vec3 d(0.0f);
            if (GLFW_KEY_LEFT == key)       d.x -= objStep;
            else if (GLFW_KEY_RIGHT == key) d.x += objStep;
            else if (GLFW_KEY_UP == key)    d.z -= objStep; // forward (-Z)
            else if (GLFW_KEY_DOWN == key)  d.z += objStep; // backward (+Z)
            else if ((GLFW_KEY_KP_ADD == key) || ((GLFW_KEY_EQUAL == key) && (mods & GLFW_MOD_SHIFT))) d.y += objStep;
            else if ((GLFW_KEY_KP_SUBTRACT == key) || (GLFW_KEY_MINUS == key)) d.y -= objStep;

            if (d != glm::vec3(0.0f))
            {
                glm::mat4 t = glm::translate(glm::mat4(1.0f), d);
                meshMatList[gPickedIndex] = t * meshMatList[gPickedIndex];
                // Rebuild spatial structure so picking/collision stays correct after movement
                meshList[gPickedIndex]->initSpatial(true, meshMatList[gPickedIndex]);
                return;
            }
        }

        glm::mat4 nextMatView = matView;
        glm::vec3 nextViewPos = viewPos;

        // camera control
        if (mods & GLFW_MOD_CONTROL) {
            if (GLFW_KEY_LEFT == key) {
                // pan left, rotate around Y, CCW
                mat = glm::rotate(glm::radians(-angleStep), glm::vec3(0.0, 1.0, 0.0));
                nextMatView = mat * matView;
            }
            else if (GLFW_KEY_RIGHT == key) {
                // pan right, rotate around Y, CW
                mat = glm::rotate(glm::radians(angleStep), glm::vec3(0.0, 1.0, 0.0));
                nextMatView = mat * matView;
            }
            else if (GLFW_KEY_UP == key) {
                // tilt up, rotate around X, CCW
                mat = glm::rotate(glm::radians(-angleStep), glm::vec3(1.0, 0.0, 0.0));
                nextMatView = mat * matView;
            }
            else if (GLFW_KEY_DOWN == key) {
                // tilt down, rotate around X, CW
                mat = glm::rotate(glm::radians(angleStep), glm::vec3(1.0, 0.0, 0.0));
                nextMatView = mat * matView;
            }
            else if ((GLFW_KEY_KP_ADD == key) ||
                (GLFW_KEY_EQUAL == key) && (mods & GLFW_MOD_SHIFT)) {
                // zoom in, move along -Z
                mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, transStep));
                nextMatView = mat * matView;
            }
            else if ((GLFW_KEY_KP_SUBTRACT == key) || (GLFW_KEY_MINUS == key)) {
                // zoom out, move along -Z
                mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -transStep));
                nextMatView = mat * matView;
            }
        }

        // translation along camera axis (first person view)
        if (GLFW_KEY_A == key) {
            //  move left along -X
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(transStep, 0.0f, 0.0f));
            nextMatView = mat * matView;
        } else if (GLFW_KEY_D == key) {
            // move right along X
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(-transStep, 0.0f, 0.0f));
            nextMatView = mat * matView;
        } else if (GLFW_KEY_W == key) {
            // move forward along -Z
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, transStep));
            nextMatView = mat * matView;
        } else if (GLFW_KEY_S == key) {
            // move backward along Z
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -transStep)); 
            nextMatView = mat * matView;
        }


        // translation along world axis
        if (GLFW_KEY_LEFT == key) {
            //  move left along -X
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(transStep, 0.0f, 0.0f));
            nextMatView = matView * mat;
            nextViewPos.x -= transStep;
        } else if (GLFW_KEY_RIGHT == key) {
            // move right along X
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(-transStep, 0.0f, 0.0f));
            nextMatView = matView * mat;
            nextViewPos.x += transStep;
        } else if (GLFW_KEY_UP == key) {
            // move up along Y
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -transStep, 0.0f));
            nextMatView = matView * mat;
            nextViewPos.y += transStep;
        } else if (GLFW_KEY_DOWN == key) {
            // move down along -Y
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, transStep, 0.0f));
            nextMatView = matView * mat;
            nextViewPos.y -= transStep;
        } else  if ((GLFW_KEY_KP_SUBTRACT == key) || (GLFW_KEY_MINUS == key))  {
            // move backward along +Z
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -transStep));
            nextMatView = matView * mat;
            nextViewPos.z += transStep;
        } else if ((GLFW_KEY_KP_ADD == key) ||
            (GLFW_KEY_EQUAL == key) && (mods & GLFW_MOD_SHIFT))  {
            // move forward along -Z
            mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, transStep));
            nextMatView = matView * mat;
            nextViewPos.z -= transStep;
        }


        // check collision detection
        AABB mybox{ nextViewPos - 0.2f, nextViewPos + 0.2f };
        std::vector<int> out;

        bool bCollide = false;
        for (std::shared_ptr<Mesh> pMesh : meshList)
        {
            pMesh->pSpatial->QueryAABB(mybox, out);
            
            if (out.empty()) {
                std::cout << "No collision" << std::endl;
            } else {
                bCollide = true;
                std::cout << "Collision detected: " << out.size() << std::endl;
            }
        }

        if (!bCollide) {
            matView = nextMatView;
            viewPos = nextViewPos;
        }
    }
}