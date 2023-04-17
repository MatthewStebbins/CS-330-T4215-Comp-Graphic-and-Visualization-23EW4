#include <cstdlib>
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include "Debug/camera.h"
#define STB_IMAGE_IMPLEMENTATION
#include "Debug/stb_image.h"  
#include <glm/gtx/string_cast.hpp>

using namespace std;

#ifndef GLSL
#define GLSL(Version, Source) "#version " #Version " core \n" #Source
#endif

namespace
{
    const char* const WINDOW_TITLE = "Module 6: Lights"; // window title

    // window width and height
    int WINDOW_WIDTH = 800;
    int WINDOW_HEIGHT = 600;

    // Camera
    Camera gCamera(glm::vec3(7.0f, 8.0f, 15.0f));
    float gLastX = WINDOW_WIDTH / 2.0f;
    float gLastY = WINDOW_HEIGHT / 2.0f;
    bool gFirstMouse = true;
    float gDeltaTime = 0.0f; // Time between frames
    float gLastFrame = 0.0f;

    // mesh
    struct GLMesh
    {
        GLuint vao;
        GLuint vbo;
        GLuint nVertices;
    };

    // View mode
    enum View_Mode {
        PERSPEC,
        ORTHO
    };
    View_Mode VIEW;
    float aspectRatio; // for p matrix aspect ratio

    // Large Cylinder - battery
    GLuint LargeCylinderVBO, LargeCylinderVAO; // Large Cylinder variables
    float LCLocX, LCLocY, LCLocZ; // Large Cylinder location (x,y,z)
    glm::mat4 pMat, vMat, mMat, mvMat; // mvp variables
    // Small Cylinder - battery
    GLuint SmallCylinderVBO, SmallCylinderVAO; // Small Cylinder variables
    float SCLocX, SCLocY, SCLocZ; // Small Cylinder location (x, y, z)
    // Plane - Table
    GLuint PlaneVBO[2], PlaneVAO; // Plane variables
    GLuint nIndices;
    // Large Cylinder - weight
    float wLCLocX, wLCLocY, wLCLocZ;
    // Small Cylinder - weight
    float wSCLocX, wSCLocY, wSCLocZ;
    // Small Box 
    GLuint BoxVBO, BoxVAO, boxVertices;
    float BLocX, BLocY, BLocZ;
    // Large Cylinder - tape
    float TLCLocX, TLCLocY, TLCLocZ;
    // Small Cylinder - tape
    float TSCLocX, TSCLocY, TSCLocZ;

    // Array for triangle rotations
    glm::float32 triRotations[] = { 0.0f, 60.0f, 120.0f, 180.0f, 240.0f, 300.0f };

    GLFWwindow* gWindow = nullptr;

    GLMesh gMesh;
    GLuint gTextureId[7];
    GLuint gProgramId;
    GLuint gLampProgramId;

    // colors
    glm::vec3 gObjectColor(1.0f, 0.2f, 0.0f);
    glm::vec3 gLightColor(0.90f, 0.94f, 0.97f);

    // Light position and scale
    glm::vec3 gLightPosition(5.5f, 12.5f, 13.0f);
    glm::vec3 gLightScale(3.3f);
}

// Function defintions 
bool UInitialize(int, char* [], GLFWwindow** window);
void UResizeWindow(GLFWwindow* window, int width, int height);
void UProcessInput(GLFWwindow* window);
void UCreateMesh(GLMesh& mesh);
void UDestroyMesh(GLMesh& mesh);
void URender();
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId);
void UDestroyShaderProgram(GLuint programId);
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos);
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
bool UCreateTexture(const char* filename, GLuint& textureId);
void UDestroyTexture(GLuint textureId);

// Vertex shader
const GLchar* vertexShaderSource = GLSL(440,
    layout(location = 0) in vec3 position; 
    layout(location = 1) in vec3 normal;
    layout(location = 2) in vec2 textureCoordinate;  

    out vec3 vertexNormal; 
    out vec3 vertexFragmentPos;
    out vec2 vertexTextureCoordinate;


    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main()
    {
        gl_Position = projection * view * model * vec4(position, 1.0f);

        vertexFragmentPos = vec3(model * vec4(position, 1.0f));

        vertexNormal = mat3(transpose(inverse(model))) * normal;
        vertexTextureCoordinate = textureCoordinate;
    }
);

// Fragment shader
const GLchar* fragmentShaderSource = GLSL(440,

    in vec3 vertexNormal;
    in vec3 vertexFragmentPos; 
    in vec2 vertexTextureCoordinate;

    out vec4 fragmentColor; 

    // Uniform 
    uniform vec3 objectColor;
    uniform vec3 lightColor;
    uniform vec3 lightPos;
    uniform vec3 viewPosition;
    uniform sampler2D uTexture;

    void main()
    {
        float ambientStrength = 0.1f; 
        vec3 ambient = ambientStrength * lightColor; 

        //Calculate Diffuse lighting*/
        vec3 norm = normalize(vertexNormal);
        vec3 lightDirection = normalize(lightPos - vertexFragmentPos);
        float impact = max(dot(norm, lightDirection), 0.0);
        vec3 diffuse = impact * lightColor; 

        //Calculate Specular lighting*/
        float specularIntensity = 0.8f; 
        float highlightSize = 16.0f; 
        vec3 viewDir = normalize(viewPosition - vertexFragmentPos); 
        vec3 reflectDir = reflect(-lightDirection, norm);

        //Calculate specular component
        float specularComponent = pow(max(dot(viewDir, reflectDir), 0.0), highlightSize);
        vec3 specular = specularIntensity * specularComponent * lightColor;


        vec4 textureColor = texture(uTexture, vertexTextureCoordinate);

        // Calculate phong result
        vec3 phong = (ambient + diffuse + specular) * textureColor.xyz;

        fragmentColor = vec4(phong, 1.0);
    }
);

/* Lamp Shader */
const GLchar* lampVertexShaderSource = GLSL(440,

    layout(location = 0) in vec3 position; 

        //Uniform 
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main()
    {
        gl_Position = projection * view * model * vec4(position, 1.0f); // Transforms vertices into clip coordinates
    }
);

/* Fragment Shader Source Code*/
const GLchar* lampFragmentShaderSource = GLSL(440,

    out vec4 fragmentColor;

    void main()
    {
        fragmentColor = vec4(1.0f); 
    }
);

void flipImageVertically(unsigned char* image, int width, int height, int channels)
{
    for (int j = 0; j < height / 2; ++j)
    {
        int index1 = j * width * channels;
        int index2 = (height - 1 - j) * width * channels;

        for (int i = width * channels; i > 0; --i)
        {
            unsigned char tmp = image[index1];
            image[index1] = image[index2];
            image[index2] = tmp;
            ++index1;
            ++index2;
        }
    }
}

void initPositions()
{
    // Large Battery 
    LCLocX = 0.0f;
    LCLocY = 0.0f;
    LCLocZ = 0.0f;
    // Small Battery
    SCLocX = 0.0f;
    SCLocY = 0.2f;
    SCLocZ = 0.0f;
    // Small Weight
    wSCLocX = 4.0f;
    wSCLocY = -1.0f;
    wSCLocZ = 4.7f;
    // Large Weight
    wLCLocX = 4.0f;
    wLCLocY = -1.5f;
    wLCLocZ = 4.7f;
    // Small Box
    BLocX = 10.0f;
    BLocY = -1.22f;
    BLocZ = 1.0f;
    // Outer Tape 
    TLCLocX = 5.0f;
    TLCLocY = -0.21f;
    TLCLocZ = 1.85f;
    // Inner Tape
    TSCLocX = 5.0f;
    TSCLocY = -0.2f;
    TSCLocZ = 1.85f;
}

// Main function for program
int main(int argc, char* argv[])
{
    // Intialize GLFW, GLEW, and window
    if (!UInitialize(argc, argv, &gWindow))
    {
        return EXIT_FAILURE;
    }
    initPositions();
    // Create mesh
    UCreateMesh(gMesh);

    // Create shader
    if (!UCreateShaderProgram(vertexShaderSource, fragmentShaderSource, gProgramId))
    {
        return EXIT_FAILURE;
    }
    if (!UCreateShaderProgram(lampVertexShaderSource, lampFragmentShaderSource, gLampProgramId))
    {
        return EXIT_FAILURE;
    }

    // Load textures
    const char* texFilename = "Debug/Brick.jpg";
    if (!UCreateTexture(texFilename, gTextureId[0]))
    {
        cout << "Failed to load texture " << texFilename << endl;
        return EXIT_FAILURE;
    }
    const char* texFilenameWood = "Debug/black-wood.jpg";
    if (!UCreateTexture(texFilenameWood, gTextureId[1]))
    {
        cout << "Failed to load texture " << texFilenameWood << endl;
        return EXIT_FAILURE;
    }
    const char* texFilenameBattery = "Debug/AmazonBattery2.png";
    if (!UCreateTexture(texFilenameBattery, gTextureId[2]))
    {
        cout << "Failed to load texture " << texFilenameBattery << endl;
        return EXIT_FAILURE;
    }
    const char* texFilenameBatteryCap = "Debug/Chrome.jpg";
    if (!UCreateTexture(texFilenameBatteryCap, gTextureId[3]))
    {
        cout << "Failed to load texture " << texFilenameBatteryCap << endl;
        return EXIT_FAILURE;
    }
    const char* texFilenameBoxTop = "Debug/BoxTop.jpg";
    if (!UCreateTexture(texFilenameBoxTop, gTextureId[4]))
    {
        cout << "Failed to load texture " << texFilenameBoxTop << endl;
        return EXIT_FAILURE;
    }
    const char* texFilenameTapeTop = "Debug/tape_t_p2.jpg";
    if (!UCreateTexture(texFilenameTapeTop, gTextureId[5]))
    {
        cout << "Failed to load texture " << texFilenameTapeTop << endl;
        return EXIT_FAILURE;
    }
    const char* texFilenameTapeSide = "Debug/white_plastic.png";
    if (!UCreateTexture(texFilenameTapeSide, gTextureId[6]))
    {
        cout << "Failed to load texture " << texFilenameTapeSide << endl;
        return EXIT_FAILURE;
    }

    glUseProgram(gProgramId);

    // texture unit 0
    glUniform1i(glGetUniformLocation(gProgramId, "Texture1"), 0);
    glUniform1i(glGetUniformLocation(gProgramId, "Texture2"), 1);
   

    // Set background to black
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);


    // render loop
    while (!glfwWindowShouldClose(gWindow))
    {
        UProcessInput(gWindow);

        URender();

        glfwPollEvents();
    }

    // Clean up
    UDestroyMesh(gMesh);
    UDestroyTexture(gTextureId[0]);
    UDestroyTexture(gTextureId[1]);
    UDestroyTexture(gTextureId[2]);
    UDestroyTexture(gTextureId[3]);
    UDestroyTexture(gTextureId[4]);
    UDestroyTexture(gTextureId[5]);
    UDestroyTexture(gTextureId[6]);
    UDestroyShaderProgram(gProgramId);
    UDestroyShaderProgram(gLampProgramId);

    // successful exit
    exit(EXIT_SUCCESS);
}

// Intialize GLFW, GLEW, and window
bool UInitialize(int argc, char* argv[], GLFWwindow** window)
{
    // Intialize and configure
    // -------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif // __APPLE__

    // Window Creation
    // ----------------------
    * window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (*window == NULL)
    {
        cout << "Failed to create GLFW window" << endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(*window);
    glfwSetFramebufferSizeCallback(*window, UResizeWindow);

    // Mouse input
    glfwSetCursorPosCallback(*window, UMousePositionCallback);
    glfwSetScrollCallback(*window, UMouseScrollCallback);
    glfwSetMouseButtonCallback(*window, UMouseButtonCallback);
    glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // GLEW 
    // ----------------------
    glewExperimental = GL_TRUE;
    GLenum GlewInitResult = glewInit();

    if (GLEW_OK != GlewInitResult)
    {
        cerr << glewGetErrorString(GlewInitResult) << endl;
        return false;
    }

    // Displays GPU OpenGL version
    cout << "INFO: OpenGL Version: " << glGetString(GL_VERSION) << endl;

    return true;
}

// handle inputs
void UProcessInput(GLFWwindow* window)
{
    static const float cameraSpeed = 2.5f;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        gCamera.ProcessKeyboard(FORWARD, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        gCamera.ProcessKeyboard(BACKWARD, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        gCamera.ProcessKeyboard(LEFT, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        gCamera.ProcessKeyboard(RIGHT, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        gCamera.Position += gCamera.Up * (gCamera.MovementSpeed * gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        gCamera.Position -= gCamera.Up * (gCamera.MovementSpeed * gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (VIEW == ORTHO)
            VIEW = PERSPEC;
        else
            VIEW = ORTHO;
    }
}

// handles resized window
void UResizeWindow(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// Handles Mouse movement
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (gFirstMouse)
    {
        gLastX = xpos;
        gLastY = ypos;
        gFirstMouse = false;
    }

    float xoffset = xpos - gLastX;
    float yoffset = gLastY - ypos; 

    gLastX = xpos;
    gLastY = ypos;

    gCamera.ProcessMouseMovement(xoffset, yoffset);
}

// Handles Mouse scroll wheel
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    gCamera.MovementSpeed += yoffset;
}

// Handle Mouse buttons
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
    {
        if (action == GLFW_PRESS)
            cout << "Left mouse button pressed" << endl;
        else
            cout << "Left mouse button released" << endl;
    }
    break;

    case GLFW_MOUSE_BUTTON_MIDDLE:
    {
        if (action == GLFW_PRESS)
            cout << "Middle mouse button pressed" << endl;
        else
            cout << "Middle mouse button released" << endl;
    }
    break;

    case GLFW_MOUSE_BUTTON_RIGHT:
    {
        if (action == GLFW_PRESS)
            cout << "Right mouse button pressed" << endl;
        else
            cout << "Right mouse button released" << endl;
    }
    break;

    default:
        cout << "Unhandled mouse button event" << endl;
        break;
    }
}

// renders frames
void URender()
{
    glEnable(GL_DEPTH_TEST);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Move and adjust the object
    glm::mat4 scale = glm::scale(glm::vec3(1.0f, 1.0f, 1.0f));
    glm::mat4 rotation = glm::rotate(0.0f, glm::vec3(1.0f, 1.0f, 0.0f));
    glm::mat4 translation = glm::translate(glm::vec3(5.0f, 0.0f, 0.0f));
    glm::mat4 model = translation * rotation * scale;
    
    // Camera
    glm::mat4 view = gCamera.GetViewMatrix();
    float currentFrame = glfwGetTime();
    gDeltaTime = currentFrame - gLastFrame;
    gLastFrame = currentFrame;


    // Perspective projection
    // glm::mat4 projection = glm::perspective(glm::radians(gCamera.Zoom), (GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT, 0.1f, 100.0f);

    // Build Perspective matrix
    glfwGetFramebufferSize(gWindow, &WINDOW_WIDTH, &WINDOW_HEIGHT);
    aspectRatio = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
    glm::mat4 projection;
    if (VIEW == PERSPEC)
        projection = glm::perspective(glm::radians(gCamera.Zoom), aspectRatio, 0.1f, 1000.0f);
    else
        projection = glm::ortho(-aspectRatio, aspectRatio, -1.0f, 1.0f, 0.1f, 100.0f);


    glUseProgram(gProgramId);

    GLint modelLoc = glGetUniformLocation(gProgramId, "model");
    GLint viewLoc = glGetUniformLocation(gProgramId, "view");
    GLint projLoc = glGetUniformLocation(gProgramId, "projection");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(model));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, value_ptr(projection));

    GLint objectColorLoc = glGetUniformLocation(gProgramId, "objectColor");
    GLint lightColorLoc = glGetUniformLocation(gProgramId, "lightColor");
    GLint lightPositionLoc = glGetUniformLocation(gProgramId, "lightPos");
    GLint viewPositionLoc = glGetUniformLocation(gProgramId, "viewPosition");

    glUniform3f(objectColorLoc, gObjectColor.r, gObjectColor.g, gObjectColor.b);
    glUniform3f(lightColorLoc, gLightColor.r, gLightColor.g, gLightColor.b);
    glUniform3f(lightPositionLoc, gLightPosition.x, gLightPosition.y, gLightPosition.z);
    const glm::vec3 cameraPosition = gCamera.Position;
    glUniform3f(viewPositionLoc, cameraPosition.x, cameraPosition.y, cameraPosition.z);


    /*-----------------------------  PLANE  -------------------------------*/

    glBindVertexArray(PlaneVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureId[1]);
    glDrawArrays(GL_TRIANGLES, 0, gMesh.nVertices);
    glBindVertexArray(0);

    /*-----------------------------  End of Plane ------------------------*/
   
   /*------------------------       BATTERY       ---------------------------------*/
    /*-----------------------   Large Cylinder    ---------------------------------*/

    glBindVertexArray(LargeCylinderVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureId[2]);

    // Use loop to build Model matrix for Large Cylinder
    for (int i = 0; i < 6; i++) {
        // Apply Transform to model // Build model matrix for LC
        scale = glm::scale(glm::vec3(0.8f, 1.0f, 0.8f));
        mMat = glm::translate(glm::mat4(1.0f), glm::vec3(LCLocX, LCLocY, LCLocZ));
        mMat = glm::rotate(mMat, glm::radians(-70.f), glm::vec3(1.0f, 90.0f, 0.5f));
        mMat = glm::rotate(mMat, glm::radians(triRotations[i]), glm::vec3(0.0f, 1.0f, 0.0f));
        mvMat =  mMat * scale;
 
        //Copy perspective and MV matrices to uniforms
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mvMat));


        glDrawArrays(GL_TRIANGLES, 0, 9);

    }

    glBindVertexArray(0);

    /*-------------------  End of Large Cylinder  ---------------------------*/

     /*-----------------------   Small Cylinder    ---------------------------------*/

    glBindVertexArray(SmallCylinderVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureId[3]);

    // Use loop to build Model matrix for Large Cylinder
    for (int i = 0; i < 6; i++) {
        // Apply Transform to model // Build model matrix for LC
        scale = glm::scale(glm::vec3(0.3f, 0.3f, 0.3f));
        mMat = glm::translate(glm::mat4(1.0f), glm::vec3(SCLocX, SCLocY, SCLocZ));
        mMat = glm::rotate(mMat, glm::radians(-70.f), glm::vec3(1.0f, 90.0f, 0.5f));
        mMat = glm::rotate(mMat, glm::radians(triRotations[i]), glm::vec3(0.0f, 1.0f, 0.0f));
        mvMat = mMat * scale;

        //Copy perspective and MV matrices to uniforms
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mvMat));


        glDrawArrays(GL_TRIANGLES, 0, 9);

    }

    glBindVertexArray(0);

    /*-------------------  End of Small Cylinder  ---------------------------*/
    /*-------------------  End of Battery         ---------------------------*/


    /*---------------------------       Weight        ----------------------------------*/
        /*-----------------------   Large Cylinder    ---------------------------------*/

    glBindVertexArray(LargeCylinderVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureId[3]);

    // Use loop to build Model matrix for Large Cylinder
    for (int i = 0; i < 6; i++) {
        // Apply Transform to model // Build model matrix for LC
        scale = glm::scale(glm::vec3(2.0f, 0.25f, 2.0f));
        mMat = glm::translate(glm::mat4(1.0f), glm::vec3(wLCLocX, wLCLocY, wLCLocZ));
        mMat = glm::rotate(mMat, glm::radians(-70.f), glm::vec3(1.0f, 90.0f, 0.5f));
        mMat = glm::rotate(mMat, glm::radians(triRotations[i]), glm::vec3(0.0f, 1.0f, 0.0f));
        mvMat = mMat * scale;

        //Copy perspective and MV matrices to uniforms
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mvMat));


        glDrawArrays(GL_TRIANGLES, 0, 9);

    }

    glBindVertexArray(0);

    /*-------------------  End of Large Cylinder  ---------------------------*/

     /*-----------------------   Small Cylinder    ---------------------------------*/

    glBindVertexArray(SmallCylinderVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureId[3]);

    // Use loop to build Model matrix for Large Cylinder
    for (int i = 0; i < 6; i++) {
        // Apply Transform to model // Build model matrix for LC
        scale = glm::scale(glm::vec3(0.5f, 0.5f, 0.5f));
        mMat = glm::translate(glm::mat4(1.0f), glm::vec3(wSCLocX, wSCLocY, wSCLocZ));
        mMat = glm::rotate(mMat, glm::radians(-70.f), glm::vec3(1.0f, 90.0f, 0.5f));
        mMat = glm::rotate(mMat, glm::radians(triRotations[i]), glm::vec3(0.0f, 1.0f, 0.0f));
        mvMat = mMat * scale;

        //Copy perspective and MV matrices to uniforms
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mvMat));


        glDrawArrays(GL_TRIANGLES, 0, 9);

    }

    glBindVertexArray(0);

    /*-------------------  End of Small Cylinder  ---------------------------*/

    /*-----------------------------  BOX  -------------------------------*/

    glBindVertexArray(BoxVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureId[4]);

    glm::mat4 Bscale = glm::scale(glm::vec3(5.5f, 1.5f, 8.0f));
    glm::mat4 Brotation = glm::rotate(0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 Btranslation = glm::translate(glm::vec3(BLocX, BLocY, BLocZ));
    glm::mat4 Bmodel = Btranslation * Brotation * Bscale;

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(Bmodel));

    glDrawArrays(GL_TRIANGLES, 0, boxVertices);
    glBindVertexArray(0);

    /*-----------------------------  End of BOX ------------------------*/

        /*---------------------------       Tape        ----------------------------------*/
        /*-----------------------   Outer Cylinder    ---------------------------------*/

    glBindVertexArray(LargeCylinderVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureId[6]);

    // Use loop to build Model matrix for Large Cylinder
    for (int i = 0; i < 6; i++) {
        // Apply Transform to model // Build model matrix for LC
        scale = glm::scale(glm::vec3(1.9f, 0.7f, 1.9f));
        mMat = glm::translate(glm::mat4(1.0f), glm::vec3(TLCLocX, TLCLocY, TLCLocZ));
        mMat = glm::rotate(mMat, glm::radians(45.0f), glm::vec3(0.1f, 0.0f, 0.2f));
        mMat = glm::rotate(mMat, glm::radians(triRotations[i]), glm::vec3(0.0f, 1.0f, 0.0f));
        mvMat = mMat * scale;

        //Copy perspective and MV matrices to uniforms
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mvMat));


        glDrawArrays(GL_TRIANGLES, 0, 9);

    }

    glBindVertexArray(0);

    /*-------------------  End of Outer Cylinder  ---------------------------*/

     /*-----------------------   Inner Cylinder    ---------------------------------*/

    glBindVertexArray(SmallCylinderVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextureId[5]);

    // Use loop to build Model matrix for Large Cylinder
    for (int i = 0; i < 6; i++) {
        // Apply Transform to model // Build model matrix for LC
        scale = glm::scale(glm::vec3(1.8f, 0.7f, 1.8f));
        mMat = glm::translate(glm::mat4(1.0f), glm::vec3(TSCLocX, TSCLocY, TSCLocZ));
        mMat = glm::rotate(mMat, glm::radians(45.0f), glm::vec3(0.1f, 0.0f, 0.2f));
        mMat = glm::rotate(mMat, glm::radians(triRotations[i]), glm::vec3(0.0f, 1.0f, 0.0f));
        mvMat = mMat * scale;

        //Copy perspective and MV matrices to uniforms
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mvMat));


        glDrawArrays(GL_TRIANGLES, 0, 9);

    }

    glBindVertexArray(0);

    /*-------------------  End of Inner Cylinder  ---------------------------*/

    /****************************** Lamp ***********************************/
    
    glUseProgram(gLampProgramId);

    model = glm::translate(gLightPosition) * glm::scale(gLightScale);

    modelLoc = glGetUniformLocation(gLampProgramId, "model");
    viewLoc = glGetUniformLocation(gLampProgramId, "view");
    projLoc = glGetUniformLocation(gLampProgramId, "projection");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    glDrawArrays(GL_TRIANGLES, 0, gMesh.nVertices);

    glBindVertexArray(0);
    glUseProgram(0);

    /*********************** End of Lamp ***********************************/


    glfwSwapBuffers(gWindow);
}

// Create mesh
void UCreateMesh(GLMesh& mesh)
{
    GLfloat verts[] = {
        //Positions          // Normals           //Texture Coordinates
        // Front face
        0.0f,  1.0f,  0.0f,  0.0f,  0.45f, 0.9f,  1.0f, 0.0f,
        -1.0f, -1.0f, 1.0f,  0.0f,  0.45f, 0.9f,  1.0f, 1.0f,
        1.0f,  -1.0f, 1.0f,  0.0f,  0.45f, 0.9f,  0.0f, 1.0f,
        // Right face
        0.0f,  1.0f,  0.0f,  0.9f,  0.45f, 0.0f,  1.0f, 0.0f,
        1.0f, -1.0f,  1.0f,  0.9f,  0.45f, 0.0f,  1.0f, 1.0f,
        1.0f,  -1.0f, -1.0f,  0.9f,  0.45f, 0.0f,  0.0f, 1.0f,
        // Back face
        0.0f,  1.0f,  0.0f, 0.0f,  0.45f, -0.9f,  1.0f, 0.0f,
        1.0f,  -1.0f, -1.0f, 0.0f,  0.45f, -0.9f,  1.0f, 1.0f,
        -1.0f, -1.0f, -1.0f, 0.0f,  0.45f, -0.9f,  0.0f, 1.0f,
        // Left face
        0.0f,  1.0f,  0.0f,  -0.9f,  0.45f, 0.0f,  1.0f, 0.0f,
        -1.0f, -1.0f, -1.0f,  -0.9f,  0.45f, 0.0f,  1.0f, 1.0f,
        -1.0f, -1.0f, 1.0f,  -0.9f,  0.45f, 0.0f,  0.0f, 1.0f,
        // Bottom back left
        -1.0f, -1.0f, 1.0f,  0.0f, -0.9f,  0.0f,   1.0f, 1.0f,
        -1.0f, -1.0f, -1.0f,  0.0f, -0.9f,  0.0f,  1.0f, 0.0f,
        1.0f, -1.0f, -1.0f,  0.0f, -0.9f,  0.0f,   0.0f, 0.0f,
        // Bottom front right
        1.0f, -1.0f, -1.0f,  0.0f, -0.9f,  0.0f,   1.0f, 0.0f,
        1.0f, -1.0f, 1.0f,  0.0f, -0.9f,  0.0f,    1.0f, 1.0f,
        -1.0f, -1.0f, 1.0f,  0.0f, -0.9f,  0.0f,   0.0f, 1.0f,
    };

    // Define vertex data for Large Cylinder
    GLfloat cylinderVertices[] = {
        // positon attributes (x,y,z)
        // triangle 1
        0.0f, 0.0f, 0.0f,  // vert 1
        0.0f, 0.43f, 0.0, // Normals
        0.5f, 1.0f, // middle top

        0.5f, 0.0f, 0.866f, // vert 2
        0.0f, 0.43f, 0.0f, 
        0.0f, 0.0f, // left top

        1.0f, 0.0f, 0.0f, // vert 3
        0.0f, 0.43f, 0.0f,
        1.0f, 0.0f,  // right top
        // triangle 2
        0.5f, 0.0f, 0.866f, // vert 4
        0.87f, 0.0f, 0.5f,
        1.0f, 1.0f, // top right side

        0.5f, -2.0f, 0.866f, // vert 5
        0.87f, 0.0f, 0.5f,
        1.0f, 0.0f,  // bottom right side	

        1.0f, 0.0f, 0.0f, // vert 6
        0.87f, 0.0f, 0.5f,
        0.0f, 1.0f, // bottom left side
        // triangle 3
        1.0f, 0.0f, 0.0f, // vert 7
        0.87f, 0.0f, 0.5f,
        0.0f, 1.0f, // right top

        1.0f, -2.0f, 0.0f, // vert 8
        0.87f, 0.0f, 0.5f,
        0.0f, 0.0f,  // blue

        0.5f, -2.0f, 0.866f, // vert 9
        0.87f, 0.0f, 0.5f,
        1.0f, 0.0f, // bottom right side
    };

    // Define vertex data for Plane
    GLfloat PlaneVertices[] = {
        // Plane1
        24.0f,  -2.0f,  24.0f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f, // Top-Right Vertex 0
        24.0f,  -2.0f, -24.0f,  0.0f, 1.0f, 0.0f,  1.0f, 0.0f, // Bottom-Right Vertex 1
        -24.0f, -2.0f,  24.0f,  0.0f, 1.0f, 0.0f,  0.0f, 1.0f, // Top-Left Vertex 3

        24.0f,  -2.0f, -24.0f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f, // Bottom-Right Vertex 1
        -24.0f, -2.0f, -24.0f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f, // Bottom-Left Vertex 2
        -24.0f, -2.0f,  24.0f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f, // Top-Left Vertex 3

        // Plane 2
        24.0f,  -2.0f,  24.0f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, // Top-Right Vertex 0
        24.0f,  -2.0f, -24.0f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f, // Bottom-Right Vertex 1
        24.0f,  -9.0f,  -24.0f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, // 4 br  right

        24.0f,  -2.0f,  24.0f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, // Top-Right Vertex 0
        24.0f,  -9.0f,  -24.0f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, // 4 br  right
        24.0f,  -9.0f,   24.0f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f, //  5 tl  right

        // Plane 3
        24.0f,  -2.0f,  24.0f, 0.0f,  0.0f, -1.0f,  1.0f, 1.0f, // Top-Right Vertex 0
        24.0f,  -9.0f,   24.0f, 0.0f,  0.0f, -1.0f,  1.0f, 0.0f, //  5 tl  right
        -24.0f, -9.0f,   24.0f, 0.0f,  0.0f, -1.0f,  0.0f, 0.0f, //  6 tl  top

        24.0f,  -2.0f,  24.0f, 0.0f,  0.0f, -1.0f,  1.0f, 1.0f, // Top-Right Vertex 0
        -24.0f, -2.0f,  24.0f, 0.0f,  0.0f, -1.0f,  0.0f, 1.0f, // Top-Left Vertex 3
        -24.0f, -9.0f,  -24.0f, 0.0f,  0.0f, -1.0f,  0.0f, 1.0f, //  7 bl back

        // Plane 4
        24.0f,  -9.0f,  -24.0f, 0.0f, -1.0f,  0.0f,  1.0f, 1.0f, // 4 br  right
        24.0f,  -9.0f,   24.0f, 0.0f, -1.0f,  0.0f,  1.0f, 0.0f, //  5 tl  right
        -24.0f, -9.0f,   24.0f, 0.0f, -1.0f,  0.0f,  0.0f, 0.0f, //  6 tl  top

        24.0f,  -9.0f,  -24.0f, 0.0f, -1.0f,  0.0f,  1.0f, 1.0f, // 4 br  right
        -24.0f, -9.0f,   24.0f, 0.0f, -1.0f,  0.0f,  0.0f, 0.0f, //  6 tl  top
        -24.0f, -9.0f,  -24.0f, 0.0f, -1.0f,  0.0f,  0.0f, 1.0f, //  7 bl back

        // Plane 5
        -24.0f, -2.0f, -24.0f, 1.0f,  0.0f,  0.0f,  0.0f, 0.0f, // Bottom-Left Vertex 2
        -24.0f, -2.0f,  24.0f, 1.0f,  0.0f,  0.0f,  0.0f, 1.0f, // Top-Left Vertex 3
        -24.0f, -9.0f,   24.0f, 1.0f,  0.0f,  0.0f,  0.0f, 0.0f, //  6 tl  top

        -24.0f, -2.0f, -24.0f, 1.0f,  0.0f,  0.0f,  0.0f, 0.0f, // Bottom-Left Vertex 2
        -24.0f, -9.0f,   24.0f, 1.0f,  0.0f,  0.0f,  0.0f, 0.0f, //  6 tl  top
        -24.0f, -9.0f,  -24.0f, 1.0f,  0.0f,  0.0f,  0.0f, 1.0f, //  7 bl back

        // Plane 6
        24.0f,  -2.0f, -24.0f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f, // Bottom-Right Vertex 1
        24.0f,  -9.0f,  -24.0f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f, // 4 br  right
        -24.0f, -9.0f,  -24.0f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f, //  7 bl back

        24.0f,  -2.0f, -24.0f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f, // Bottom-Right Vertex 1
        -24.0f, -2.0f, -24.0f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f, // Bottom-Left Vertex 2
        -24.0f, -9.0f,  -24.0f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f, //  7 bl back
    };

    GLfloat BoxVertices[] = {
        //Positions          //Normals
        // ------------------------------------------------------
        //Back Face          //Negative Z Normal  Texture Coords.
       -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
        0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
        0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
       -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
       -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,

       //Front Face         //Positive Z Normal
      -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
       0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
       0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
       0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
      -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
      -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,

      //Left Face          //Negative X Normal
     -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
     -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
     -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
     -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
     -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
     -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

     //Right Face         //Positive X Normal
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

     //Bottom Face        //Negative Y Normal
     -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
     -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
     -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

    //Top Face           //Positive Y Normal
     -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
     -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
     -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f
    };


    const GLuint floatsPerVertex = 3;
    const GLuint floatsPerNormal = 3;
    const GLuint floatsPerUV = 2;

    boxVertices = sizeof(BoxVertices) / (sizeof(BoxVertices[0]) * (floatsPerVertex + floatsPerNormal + floatsPerUV));
    mesh.nVertices = sizeof(verts) / (sizeof(verts[0]) * (floatsPerVertex + floatsPerNormal + floatsPerUV));
    // Pyramind
    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    GLint stride = sizeof(float) * (floatsPerVertex + floatsPerNormal + floatsPerUV);
    glVertexAttribPointer(0, floatsPerVertex, GL_FLOAT, GL_FALSE, stride, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, floatsPerNormal, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)* floatsPerVertex));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, floatsPerUV, GL_FLOAT, GL_FALSE, stride, (char*)(sizeof(float) * (floatsPerVertex + floatsPerNormal)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    
    // Large Cynlinder 
    glGenVertexArrays(1, &LargeCylinderVAO); // Create VAO
    glGenBuffers(1, &LargeCylinderVBO); // Create VBO
    glBindVertexArray(LargeCylinderVAO); // Activate VAO for VBO association
    glBindBuffer(GL_ARRAY_BUFFER, LargeCylinderVBO); // Enable VBO	
    glBufferData(GL_ARRAY_BUFFER, sizeof(cylinderVertices), cylinderVertices, GL_STATIC_DRAW); // Copy Vertex data to VBO
    glVertexAttribPointer(0, floatsPerVertex, GL_FLOAT, GL_FALSE, stride, 0); // Associate VBO with VA (Vertex Attribute)
    glEnableVertexAttribArray(0); // Enable VA
    glVertexAttribPointer(1, floatsPerNormal, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)* floatsPerVertex));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, floatsPerUV, GL_FLOAT, GL_FALSE, stride, (char*)(sizeof(float)* (floatsPerVertex + floatsPerNormal))); // Associate VBO with VA
    glEnableVertexAttribArray(2); // Enable VA
    glBindVertexArray(0); // Unbind VAO (Optional but recommended)

     // Small Cynlinder
    glGenVertexArrays(1, &SmallCylinderVAO); // Create VAO
    glGenBuffers(1, &SmallCylinderVBO); // Create VBO
    glBindVertexArray(SmallCylinderVAO); // Activate VAO for VBO association
    glBindBuffer(GL_ARRAY_BUFFER, SmallCylinderVBO); // Enable VBO	
    glBufferData(GL_ARRAY_BUFFER, sizeof(cylinderVertices), cylinderVertices, GL_STATIC_DRAW); // Copy Vertex data to VBO
    glVertexAttribPointer(0, floatsPerVertex, GL_FLOAT, GL_FALSE, stride, 0); // Associate VBO with VA (Vertex Attribute)
    glEnableVertexAttribArray(0); // Enable VA
    glVertexAttribPointer(1, floatsPerNormal, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)* floatsPerVertex));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, floatsPerUV, GL_FLOAT, GL_FALSE, stride, (char*)(sizeof(float)* (floatsPerVertex + floatsPerNormal)));  // Associate VBO with VA
    glEnableVertexAttribArray(2); // Enable VA
    glBindVertexArray(0); // Unbind VAO (Optional but recommended)

    // Plane
    glGenVertexArrays(1, &PlaneVAO); // Create VAO
    glBindVertexArray(PlaneVAO);
    glGenBuffers(1, PlaneVBO);
    glBindBuffer(GL_ARRAY_BUFFER, PlaneVBO[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(PlaneVertices), PlaneVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, floatsPerVertex, GL_FLOAT, GL_FALSE, stride, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, floatsPerNormal, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)* floatsPerVertex));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, floatsPerUV, GL_FLOAT, GL_FALSE, stride, (char*)(sizeof(float) * (floatsPerVertex + floatsPerNormal)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    // Box
    glGenVertexArrays(1, &BoxVAO);
    glBindVertexArray(BoxVAO);
    glGenBuffers(1, &BoxVBO);
    glBindBuffer(GL_ARRAY_BUFFER, BoxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(BoxVertices), BoxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, floatsPerVertex, GL_FLOAT, GL_FALSE, stride, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, floatsPerNormal, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float)* floatsPerVertex));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, floatsPerUV, GL_FLOAT, GL_FALSE, stride, (char*)(sizeof(float)* (floatsPerVertex + floatsPerNormal)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

}
// Deletes mesh 
void UDestroyMesh(GLMesh& mesh)
{
    glDeleteVertexArrays(1, &mesh.vao);
    glDeleteBuffers(1, &mesh.vbo);
}

bool UCreateTexture(const char* filename, GLuint& textureId)
{
    int width, height, channels;
    unsigned char* image = stbi_load(filename, &width, &height, &channels, 0);
    if (image)
    {
        flipImageVertically(image, width, height, channels);

        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);

        // set the texture wrapping parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        // set texture filtering parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (channels == 3)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
        else if (channels == 4)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
        else
        {
            cout << "Not implemented to handle image with " << channels << " channels" << endl;
            return false;
        }

        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(image);
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

        return true;
    }

    // Error loading the image
    return false;
}

void UDestroyTexture(GLuint textureId)
{
    glGenTextures(1, &textureId);
}

// Create shaders
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId)
{
    int success = 0;
    char infoLog[512];

    programId = glCreateProgram();

    GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertexShaderId, 1, &vtxShaderSource, NULL);
    glShaderSource(fragmentShaderId, 1, &fragShaderSource, NULL);

    glCompileShader(vertexShaderId);

    glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShaderId, 512, NULL, infoLog);
        cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << endl;

        return false;
    }

    glCompileShader(fragmentShaderId);

    glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShaderId, sizeof(infoLog), NULL, infoLog);
        cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << endl;

        return false;
    }

    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);

    glLinkProgram(programId);

    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(programId, sizeof(infoLog), NULL, infoLog);
        cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << endl;

        return false;
    }

    glUseProgram(programId);

    return true;
}

// Deletes shader
void UDestroyShaderProgram(GLuint programId)
{
    glDeleteProgram(programId);
}