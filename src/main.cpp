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
static float gYaw = 0.0f;
static float gPitch = 0.0f;
glm::mat4 matProj = glm::ortho(-2.0f,2.0f,-2.0f,2.0f, -2.0f,2.0f);

glm::vec3 lightPos = glm::vec3(5.0f, 5.0f, 10.0f);
glm::vec3 viewPos_default = glm::vec3(0.0f, 2.0f, 6.0f);
glm::vec3 viewPos = viewPos_default;

// New Camera Movement Implementation 
glm::vec3 gCamTarget = glm::vec3(0.0f, 0.8f, 0.0f);
float gCamDistance = 10.0f; // zoom distance
float gCamYaw = -90.0f;     // left/right rotation
float gCamPitch = -15.0f;   // up/down rotation

bool gRMBDown = false;
double gLastMouseX = 0.0, gLastMouseY = 0.0;
float gMouseSensitivity = 0.15f;



// Floor render mode: filled (false) or grid/wire (true)
static bool gFloorWireframe = false;

// Current picked mesh index (for basic object movement)
static int gPickedIndex = -1;

// We are using mesh list instead of scene graph to demo our picking and collision detection
std::vector< std::shared_ptr <Mesh> > meshList;
std::vector< glm::mat4 > meshMatList;

// GLuint flatShader;
GLuint blinnShader;
GLuint phongShader;
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

// New Camera Movement Implementation 
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

static void BuildOrbitCamera(const glm::vec3& target, float distance, float yawDeg, float pitchDeg,
    glm::vec3& outPos, glm::mat4& outView)
{
    // clamp the pitch so you can't flip upside down
    float pitchClamped = glm::clamp(pitchDeg, -89.0f, 89.0f);

    // convert yaw/pitch to a direction
    float yawRad = glm::radians(gCamYaw);
    float pitchRad = glm::radians(gCamPitch);

    glm::vec3 dir;
    dir.x = cosf(pitchRad) * cosf(yawRad);
    dir.y = sinf(pitchRad);
    dir.z = cosf(pitchRad) * sinf(yawRad);
    dir = glm::normalize(dir);

    // camera position is target minus direction * distance
    outPos = target - dir * distance;

    // rebuild view matrix every frame
    outView = glm::lookAt(outPos, target, glm::vec3(0, 1, 0));
}
static void UpdateOrbitCamera()
{
    BuildOrbitCamera(gCamTarget, gCamDistance, gCamYaw, gCamPitch, viewPos, matView);
}

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

glm::vec3 GetCameraWorldPosFromView(const glm::mat4& view)
{
    glm::mat4 invView = glm::inverse(view);
    return glm::vec3(invView[3]); // camera position in world space
}




void mouse_button_callback(GLFWwindow *win, int button, int action, int mods)
{
    
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        if (action == GLFW_PRESS)
        {
            gRMBDown = true;
            glfwGetCursorPos(win, &gLastMouseX, &gLastMouseY);
        }
        else if (action == GLFW_RELEASE)
        {
            gRMBDown = false;
        }
        return; // stop RMB from also doing picking
    }
    
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        double mx, my;
        glfwGetCursorPos(win, &mx, &my);

        std::cout << "Mouse click at: (" << mx <<", " << my << ")" << std::endl;

        /*int w, h;
        glfwGetWindowSize(win, &w, &h);

        glm::vec3 rayOrig = viewPos;
        glm::vec3 rayDir = screenPosToRay((int)mx, (int)my, w, h, matProj, matView);*/

        int fbW, fbH;
        glfwGetFramebufferSize(win, &fbW, &fbH);
        glm::vec3 rayOrig = GetCameraWorldPosFromView(matView);
        glm::vec3 rayDir = screenPosToRay((int)mx, (int)my, fbW, fbH, matProj, matView);
        
    
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

// New Camera Movement Implementation ------------------------------------
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (!gRMBDown)
    {
        gLastMouseX = xpos;
        gLastMouseY = ypos;
        return;
    }

    double dx = xpos - gLastMouseX;
    double dy = ypos - gLastMouseY;
    gLastMouseX = xpos;
    gLastMouseY = ypos;

    gCamYaw += (float)dx * gMouseSensitivity;
    gCamPitch -= (float)dy * gMouseSensitivity; // minus so moving mouse up looks up
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    // scroll up = zoom in
    gCamDistance -= (float)yoffset * 0.7f;
    gCamDistance = glm::clamp(gCamDistance, 2.0f, 60.0f);
}
// -----------------------------------------------------

static void ApplyLookRotation(glm::mat4& viewMat, float yawDeg, float pitchDeg)
{
    // Rotate view: yaw around world Y, pitch around camera X
    glm::mat4 yawMat = glm::rotate(glm::mat4(1.0f), glm::radians(yawDeg), glm::vec3(0, 1, 0));
    glm::mat4 pitchMat = glm::rotate(glm::mat4(1.0f), glm::radians(pitchDeg), glm::vec3(1, 0, 0));

    // Apply rotations to current view matrix (pre-multiply)
    viewMat = pitchMat * yawMat * viewMat;
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
    window = glfwCreateWindow(1920, 1080, "My Main Assignment", NULL, NULL);
    glfwMakeContextCurrent(window);

    // register the key event callback function
    glfwSetKeyCallback(window, key_callback);
    
    // register the mouse button event callback function
    glfwSetMouseButtonCallback(window, mouse_button_callback);

	// new camera movement callbacks
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);

	// stop mouse leaving window if i set up a new cursor inside

    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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
   // OLD INITIAL VIEW SETUP
    /* viewPos = glm::vec3(0.0f, 2.0f, 5.0f);
    matView = glm::lookAt(viewPos, glm::vec3(0, 0, -10), glm::vec3(0, 1, 0));*/ 

    gCamTarget = glm::vec3(0.0f, 0.8f, 0.0f);
    gCamDistance = 10.0f;
    gCamYaw = -90.0f;
    gCamPitch = -15.0f;

    UpdateOrbitCamera();

    // set the Y field of view angle to 90 degrees, width/height ratio to 1.0, and a near plane of 0.5, far plane of 40.0
    // try to play with the FoV
    //matProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    int fbW, fbH;
    glfwGetFramebufferSize(window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    float aspect = (fbH == 0) ? 1.0f : (float)fbW / (float)fbH;
    matProj = glm::perspective(glm::radians(90.0f), aspect, 0.1f, 100.0f);
    // Procedural floor grid (triangles)
    // Generates a simple XZ grid made of triangles centred at origin.
    std::shared_ptr<Mesh> floor = std::make_shared<Mesh>();
    {
        const int N = 40;
        const float size = 20.0f;
        const float half = size * 0.5f;
        const float step = size / (float)N;

        std::vector<Vertex> v;
        std::vector<unsigned int> idx;
        v.reserve((N + 1) * (N + 1));
        idx.reserve(N * N * 6);

        for (int z = 0; z <= N; z++)
        {
            for (int x = 0; x <= N; x++)
            {
                Vertex vert;
                vert.pos = glm::vec3(-half + x * step, 0.0f, -half + z * step);
                vert.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                vert.texCoord = glm::vec2((float)x / (float)N, (float)z / (float)N);
                v.push_back(vert);
            }
        }

        auto vid = [N](int x, int z) { return (unsigned int)(z * (N + 1) + x); };
        for (int z = 0; z < N; z++)
        {
            for (int x = 0; x < N; x++)
            {
                unsigned int i0 = vid(x, z);
                unsigned int i1 = vid(x + 1, z);
                unsigned int i2 = vid(x, z + 1);
                unsigned int i3 = vid(x + 1, z + 1);

                idx.push_back(i0); idx.push_back(i2); idx.push_back(i1);
                idx.push_back(i1); idx.push_back(i2); idx.push_back(i3);
            }
        }

        floor->initFromData(v, idx, floorShader);
    }

    // store indices of mugs so we can colour them differently
    std::vector<int> mugIndices;

    glm::mat4 mat = glm::mat4(1.0f);

    // Teapot
    /*std::shared_ptr<Mesh> teapot = std::make_shared<Mesh>();
    teapot->init("models/teapot.obj", blinnShader);
    meshList.push_back(teapot);

    mat = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 1.0f, 0.0f));
    meshMatList.push_back(mat);
    teapot->initSpatial(true, mat);*/
    // 4 Mugs
    for (int i = 0; i < 4; i++)
    {
        std::shared_ptr<Mesh> mug = std::make_shared<Mesh>();
        mug->init("models/Winter_Mug_Low_Poly.obj", texblinnShader);
        meshList.push_back(mug);

        mugIndices.push_back((int)meshList.size() - 1);

        glm::mat4 mugMat = glm::mat4(1.0f);
        mugMat = glm::translate(mugMat, glm::vec3(1.0f + i * 2.5f, 0.0f, 0.0f)); // spacing
        mugMat = glm::scale(mugMat, glm::vec3(10.0f));
        meshMatList.push_back(mugMat);

        mug->initSpatial(true, mugMat);
    }


    // ---------- Medieval House ---------------------------------------------
    std::vector<int> wallLRIndices;
    // how high the 2nd row sits
    float yStep = 1.0f;
    // Back walls: 2 rows (bottom + top)
    for (int row = 0; row < 2; row++)        // 0 = bottom, 1 = top
    {
        for (int i = 0; i < 3; i++)          // 3 walls across
        {
            std::shared_ptr<Mesh> wallLR = std::make_shared<Mesh>();
            wallLR->init("models/MedievalHouse/wall-paint.obj", texblinnShader);
            meshList.push_back(wallLR);

            wallLRIndices.push_back((int)meshList.size() - 1);

            glm::mat4 wallMat = glm::mat4(1.0f);

            float x = -1.0f + i * 1.0f;      // -1, 0, 1
            float y = 0.0f + row * yStep;    // 0 (bottom), yStep (top)
            float z = 1.0f;                  // back row

            wallMat = glm::translate(wallMat, glm::vec3(x, y, z));
            wallMat = glm::scale(wallMat, glm::vec3(1.0f, 1.0f, 1.0f));

            meshMatList.push_back(wallMat);
            wallLR->initSpatial(true, wallMat);
        }
    }
    // Extra wall in front of the MIDDLE TOP one (above the door)
    std::shared_ptr<Mesh> wallFrontTopMid = std::make_shared<Mesh>();
    wallFrontTopMid->init("models/MedievalHouse/wall-paint.obj", texblinnShader);
    meshList.push_back(wallFrontTopMid);

    glm::mat4 extraMat = glm::mat4(1.0f);

    // middle = x 0.0, top row = yStep, and "front" = z 2.0 (match your door z)
    extraMat = glm::translate(extraMat, glm::vec3(0.0f, yStep, 2.0f));

    // if it needs flipping like your front pieces, uncomment this rotate:
    // extraMat = extraMat * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0,1,0));

    meshMatList.push_back(extraMat);
    wallFrontTopMid->initSpatial(true, extraMat);

    
    // window front walls
    std::vector<int> wallPWindowIndices;

    for (int row = 0; row < 2; row++)          // 0 = bottom, 1 = top
    {
        for (int i = 0; i < 2; i++)            // 0 = left, 1 = right
        {
            // window walls at front
            std::shared_ptr<Mesh> wallPWindow = std::make_shared<Mesh>();
            wallPWindow->init("models/MedievalHouse/wall-paint-window.obj", texblinnShader);
            meshList.push_back(wallPWindow);

            wallPWindowIndices.push_back((int)meshList.size() - 1);

            glm::mat4 windowMat = glm::mat4(1.0f);

            float x = -1.0f + i * 2.0f;        // left/right: -1, +1
            float y = 0.0f + row * 1.0f;       // bottom/top: 0, 1 
            float z = 2.0f;                    // front

            windowMat = glm::translate(windowMat, glm::vec3(x, y, z));
            windowMat = glm::scale(windowMat, glm::vec3(1.0f, 1.0f, 1.0f));
            windowMat = windowMat * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            meshMatList.push_back(windowMat);
            wallPWindow->initSpatial(true, windowMat);
        }
    }
    // wall door
    std::shared_ptr<Mesh> wallPDoor = std::make_shared<Mesh>();
    wallPDoor->init("models/medievalHouse/wall-paint-door.obj", texblinnShader);
    meshList.push_back(wallPDoor);

    glm::mat4 doorMat = glm::mat4(1.0f);

    doorMat = glm::translate(doorMat, glm::vec3(0.0f, 0.0f, 2.0f));
	doorMat = doorMat * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    
    meshMatList.push_back(doorMat);
    wallPDoor->initSpatial(true, doorMat);

	// Roof pieces
    std::vector<int> roofIndices;

    // TWEAK THESE 3 NUMBERS to fit your building
    float roofY = 2.0f;          // how high the roof sits (top of building)
    float roofZFront = 2.0f;     // front row z (match your front walls/door)
    float roofZBack = 1.0f;     // back row z (match your back walls)

    for (int row = 0; row < 2; row++)          // 0 = back, 1 = front (or vice versa)
    {
        for (int i = 0; i < 3; i++)            // 3 across: x = -1, 0, 1
        {
            std::shared_ptr<Mesh> roof = std::make_shared<Mesh>();
            roof->init("models/MedievalHouse/roof.obj", texblinnShader);   // <-- change if your roof file name differs
            meshList.push_back(roof);
            roofIndices.push_back((int)meshList.size() - 1);

            float x = -1.0f + i * 1.0f;        // -1, 0, 1
            float z = (row == 0) ? roofZBack : roofZFront;

            glm::mat4 roofMat = glm::mat4(1.0f);
            roofMat = glm::translate(roofMat, glm::vec3(x, roofY, z));
            roofMat = glm::scale(roofMat, glm::vec3(1.0f));

            // If the roof faces the wrong way, uncomment:
            // roofMat = roofMat * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0,1,0));

            meshMatList.push_back(roofMat);
            roof->initSpatial(true, roofMat);
        }
    }


    // ---------- End Of Medieval House ----------
    
    // Background 
    glClearColor(0.12f, 0.05f, 0.18f, 1.0f); // dark purple
    glEnable(GL_DEPTH_TEST);

    // Render loop 
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        UpdateOrbitCamera();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        setViewPositionForProgram(phongShader, viewPos);
        setViewPositionForProgram(blinnShader, viewPos);
        setViewPositionForProgram(texblinnShader, viewPos);
        setViewPositionForProgram(floorShader, viewPos);

        // -------- Floor (wireframe toggle) --------
        if (gFloorWireframe)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        glUseProgram(floorShader);
        GLint floorColLoc = glGetUniformLocation(floorShader, "baseColour");
        if (floorColLoc >= 0) glUniform3f(floorColLoc, 0.05f, 0.25f, 0.12f); // dark green

        floor->draw(matModelRoot, matView, matProj);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // Models
        // get uniform location once per frame (cheaper)
        //glUseProgram(blinnShader);
        //GLint blinnColLoc = glGetUniformLocation(blinnShader, "baseColour");

        // OLD SHADERS TO USE FOR WORLD
        //for (int i = 0; i < (int)meshList.size(); i++)
        //{
        //    glUseProgram(blinnShader);
        //    
        //    bool isMug = std::find(mugIndices.begin(), mugIndices.end(), i) != mugIndices.end();

        //    if (blinnColLoc >= 0)
        //    {
        //        if (isMug)
        //            glUniform3f(blinnColLoc, 0.08f, 0.12f, 0.35f); // dark blue
        //        else
        //            glUniform3f(blinnColLoc, 0.08f, 0.12f, 0.35f);    // default (teapot)
        //    }

        //    meshList[i]->draw(matModelRoot * meshMatList[i], matView, matProj);
        //

        //    std::shared_ptr<Mesh> pMesh = meshList[i];
        //    
        //}

        // Models (use texblinn shader colour uniform)
        glUseProgram(texblinnShader);
        GLint texColLoc = glGetUniformLocation(texblinnShader, "baseColour");

        for (int i = 0; i < (int)meshList.size(); i++)
        {
            bool isMug = std::find(mugIndices.begin(), mugIndices.end(), i) != mugIndices.end();

            
            if (texColLoc >= 0)
            {
                if (isMug)
                    glUniform3f(texColLoc, 0.08f, 0.12f, 0.35f);  // mug tint
                else
                    glUniform3f(texColLoc, 1.0f, 1.0f, 1.0f);     // default white (no tint)
            }

            meshList[i]->draw(matModelRoot * meshMatList[i], matView, matProj);
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

        
         //we don't allow objects to move for picking and collision detection
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
                gYaw -= angleStep;
                nextMatView = matView;
                ApplyLookRotation(nextMatView, -angleStep, 0.0f);
            }
            else if (GLFW_KEY_RIGHT == key) {
                // pan right, rotate around Y, CW
                gYaw -= angleStep;
                nextMatView = matView;
                ApplyLookRotation(nextMatView, +angleStep, 0.0f);
            }
            else if (GLFW_KEY_UP == key) {
                // tilt up, rotate around X, CCW
                gYaw -= angleStep;
                nextMatView = matView;
                ApplyLookRotation(nextMatView, 0.0f, -angleStep);
            }
            else if (GLFW_KEY_DOWN == key) {
                // tilt down, rotate around X, CW
                gYaw -= angleStep;
                nextMatView = matView;
                ApplyLookRotation(nextMatView, 0.0f, +angleStep);
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

        // --- WASD moves the orbit target (camera follows), with collision ---
        if (key == GLFW_KEY_W || key == GLFW_KEY_A || key == GLFW_KEY_S || key == GLFW_KEY_D)
        {
            // Build current camera first (so our directions match what you see)
            UpdateOrbitCamera();

            // Camera basis from current view
            glm::vec3 forward = glm::normalize(gCamTarget - viewPos);         // towards target
            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

            // Flatten to XZ so W/S doesn't fly up/down
            forward.y = 0.0f;
            if (glm::length(forward) > 0.0001f) forward = glm::normalize(forward);

            glm::vec3 move(0.0f);
            if (key == GLFW_KEY_W) move += forward * transStep;
            if (key == GLFW_KEY_S) move -= forward * transStep;
            if (key == GLFW_KEY_D) move += right * transStep;
            if (key == GLFW_KEY_A) move -= right * transStep;

            // Propose a new target position
            glm::vec3 proposedTarget = gCamTarget + move;

            // Build the *proposed* camera position from that target
            glm::vec3 proposedCamPos;
            glm::mat4 proposedView;
            BuildOrbitCamera(proposedTarget, gCamDistance, gCamYaw, gCamPitch, proposedCamPos, proposedView);

            // Collision test uses the proposed camera position
            AABB mybox{ proposedCamPos - glm::vec3(0.2f), proposedCamPos + glm::vec3(0.2f) };

            bool bCollide = false;
            for (const std::shared_ptr<Mesh>& pMesh : meshList)
            {
                if (!pMesh || !pMesh->pSpatial) continue;

                std::vector<int> out;
                pMesh->pSpatial->QueryAABB(mybox, out);
                if (!out.empty())
                {
                    bCollide = true;
                    break;
                }
            }

            // Only commit move if no collision
            if (!bCollide)
            {
                gCamTarget = proposedTarget;
                
                UpdateOrbitCamera();
            }

            return; 
        }



        // translation along world axis
        //if (GLFW_KEY_LEFT == key) {
        //    //  move left along -X
        //    mat = glm::translate(glm::mat4(1.0f), glm::vec3(transStep, 0.0f, 0.0f));
        //    nextMatView = matView * mat;
        //    nextViewPos.x -= transStep;
        //} else if (GLFW_KEY_RIGHT == key) {
        //    // move right along X
        //    mat = glm::translate(glm::mat4(1.0f), glm::vec3(-transStep, 0.0f, 0.0f));
        //    nextMatView = matView * mat;
        //    nextViewPos.x += transStep;
        //} else if (GLFW_KEY_UP == key) {
        //    // move up along Y
        //    mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -transStep, 0.0f));
        //    nextMatView = matView * mat;
        //    nextViewPos.y += transStep;
        //} else if (GLFW_KEY_DOWN == key) {
        //    // move down along -Y
        //    mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, transStep, 0.0f));
        //    nextMatView = matView * mat;
        //    nextViewPos.y -= transStep;
        //} else  if ((GLFW_KEY_KP_SUBTRACT == key) || (GLFW_KEY_MINUS == key))  {
        //    // move backward along +Z
        //    mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -transStep));
        //    nextMatView = matView * mat;
        //    nextViewPos.z += transStep;
        //} else if ((GLFW_KEY_KP_ADD == key) ||
        //    (GLFW_KEY_EQUAL == key) && (mods & GLFW_MOD_SHIFT))  {
        //    // move forward along -Z
        //    mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, transStep));
        //    nextMatView = matView * mat;
        //    nextViewPos.z -= transStep;
        //}


        // check collision detection
        AABB mybox{ nextViewPos - glm::vec3(0.2f), nextViewPos + glm::vec3(0.2f) };

        bool bCollide = false;

        for (const std::shared_ptr<Mesh>& pMesh : meshList)
        {
            if (!pMesh || !pMesh->pSpatial) continue;

            std::vector<int> out;
            pMesh->pSpatial->QueryAABB(mybox, out);

            if (!out.empty())
            {
                bCollide = true;
                break; // stop as soon as we hit something
            }
        }

        if (!bCollide) {
            matView = nextMatView;
            viewPos = nextViewPos;
        }
    }
}