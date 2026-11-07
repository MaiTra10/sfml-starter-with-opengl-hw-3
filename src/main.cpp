#include <glad/gl.h> // Must be first!
#include <SFML/Window.hpp>
#include <SFML/System/Clock.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "geometry.h"
#include "mesh.h"

namespace {

// Sizes of every part of the arm
constexpr float baseHeight = 0.70f;
constexpr float baseRadius = 1.10f;

constexpr float shoulderRadius = 0.24f;
constexpr float upperArmLength = 2.80f;
constexpr float upperArmRadius = 0.22f;

constexpr float elbowRadius = 0.20f;

constexpr float forearmLength = 2.25f;
constexpr float forearmRadius = 0.17f;

constexpr float wristRadius = 0.17f;

constexpr float palmLength = 0.82f;
constexpr float palmWidth = 0.36f;
constexpr float palmHeight = 0.68f;
constexpr float palmGap = 0.10f;

constexpr float stemLength = 0.58f;
constexpr float stemRadius = 0.045f;
constexpr float stemOffset = 0.28f;

constexpr float fingerJointRadius = 0.06f;
constexpr float fingerLength = 0.80f;
constexpr float fingerWidth = 0.085f;
constexpr float fingerThickness = 0.07f;

// How many triangles are used for round shapes
constexpr unsigned int cylinderSlices = 64;
constexpr unsigned int sphereSlices = 48;
constexpr unsigned int sphereStacks = 24;

// Turn speed for the joints at degrees per second
constexpr float turnSpeed = 60.0f;

// Limits to how far a joint can turn in degrees
constexpr float shoulderLimit = 90.0f;

constexpr float elbowLimit = 150.0f;
constexpr float wristLimit = 120.0f;

constexpr float gripMinimum = -30.0f;
constexpr float gripMaximum = 15.0f;

// Camera and light
constexpr float fieldOfView = glm::radians(45.0f);
constexpr float armReach = 7.4f;
constexpr float cameraPadding = 0.8f;

const glm::vec3 armCenter(0.0f, baseHeight, 0.0f);
const glm::vec3 cameraDirection = glm::normalize(glm::vec3(0.6f, 0.45f, 1.0f));
const glm::vec3 lightWorldPosition(4.0f, 10.0f, 6.0f);

// All joint angles are in degrees
struct JointAngles {

    float base = 0.0f;
    float shoulder = -15.0f;
    float elbow = -60.0f;
    float wrist = -40.0f;
    float grip = 8.0f;

};

// The pose that the reset key goes back to, every joint straight at zero degrees
const JointAngles resetAngles{0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

struct Camera {

    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};

};

struct AssetPaths {

    std::string vertexShader;
    std::string fragmentShader;
    std::string armTexture;
    std::string baseTexture;

};

Material makeMaterial(const glm::vec3& color, float specularStrength, float shininess)
{

    Material material;
    material.ambient = color * 0.25f;
    material.diffuse = color;
    material.specular = glm::vec3(specularStrength);
    material.shininess = shininess;

    return material;

}

Mesh makeMesh(
    const Geometry& geometry,
    const AssetPaths& paths,
    const std::string& texturePath,
    const Material& material
) {

    return Mesh(
        geometry.vertices,
        geometry.indices,
        paths.vertexShader,
        paths.fragmentShader,
        texturePath,
        material
    );

}

Geometry makeBaseBodyGeometry()
{

    // The top of the base is its own textured mesh, so only the wall and the bottom go here
    Geometry baseBody = makeCylinderSide(baseRadius, baseHeight, cylinderSlices, false);
    append(baseBody, makeDisk(baseRadius, 0.0f, false, cylinderSlices));

    return baseBody;

}

glm::mat4 makeTranslation(float x, float y, float z)
{

    return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));

}

glm::mat4 makeRotationAroundY(float degrees)
{

    return glm::rotate(glm::mat4(1.0f), glm::radians(degrees), glm::vec3(0.0f, 1.0f, 0.0f));

}

glm::mat4 makeRotationAroundZ(float degrees)
{

    return glm::rotate(glm::mat4(1.0f), glm::radians(degrees), glm::vec3(0.0f, 0.0f, 1.0f));

}

// Owns every mesh of the arm and places them using the joint angles
class RobotArm {
public:
    RobotArm(const AssetPaths& paths)
        : baseBody(makeMesh(makeBaseBodyGeometry(),
                            paths,
                            "",
                            makeMaterial(glm::vec3(0.22f, 0.24f, 0.28f), 0.4f, 48.0f))),
        baseTop(makeMesh(makeDisk(baseRadius, baseHeight, true, cylinderSlices),
                        paths,
                        paths.baseTexture,
                        makeMaterial(glm::vec3(0.9f), 0.3f, 32.0f))),
        shoulderSphere(makeMesh(makeSphere(shoulderRadius, sphereSlices, sphereStacks),
                                paths,
                                "",
                                makeMaterial(glm::vec3(0.95f, 0.75f, 0.20f), 0.5f, 64.0f))),
        upperArm(makeMesh(makeCappedCylinder(upperArmRadius, upperArmLength, cylinderSlices, true),
                        paths,
                        paths.armTexture,
                        makeMaterial(glm::vec3(0.9f), 0.3f, 32.0f))),
        elbowSphere(makeMesh(makeSphere(elbowRadius, sphereSlices, sphereStacks),
                            paths,
                            "",
                            makeMaterial(glm::vec3(0.95f, 0.75f, 0.20f), 0.5f, 64.0f))),
        forearm(makeMesh(makeCappedCylinder(forearmRadius, forearmLength, cylinderSlices, true),
                        paths,
                        paths.armTexture,
                        makeMaterial(glm::vec3(0.9f), 0.3f, 32.0f))),
        wristSphere(makeMesh(makeSphere(wristRadius, sphereSlices, sphereStacks),
                            paths,
                            "",
                            makeMaterial(glm::vec3(0.95f, 0.50f, 0.25f), 0.5f, 64.0f))),
        palm(makeMesh(makeCuboid(palmLength, palmHeight, palmWidth),
                    paths,
                    "",
                    makeMaterial(glm::vec3(0.55f, 0.55f, 0.62f), 0.4f, 48.0f))),
        stem(makeMesh(makeCappedCylinder(stemRadius, stemLength, 24, false),
                    paths,
                    "",
                    makeMaterial(glm::vec3(0.85f), 0.8f, 96.0f))),
        fingerJoint(makeMesh(makeSphere(fingerJointRadius, sphereSlices, sphereStacks),
                            paths,
                            "",
                            makeMaterial(glm::vec3(0.95f, 0.50f, 0.25f), 0.5f, 64.0f))),
        finger(makeMesh(makeCuboid(fingerWidth, fingerLength, fingerThickness),
                        paths,
                        "",
                        makeMaterial(glm::vec3(0.15f, 0.15f, 0.18f), 0.5f, 64.0f)))
    {
    }

    void draw(const JointAngles& angles, const Camera& camera)
    {

        // Places a mesh and draws it right away
        auto drawPart = [&](Mesh& mesh, const glm::mat4& transform) {
            mesh.setTransform(transform);
            mesh.draw(camera.view, camera.projection, lightWorldPosition);
        };

        // The base turns around the world's vertical axis
        const glm::mat4 baseMatrix = makeRotationAroundY(angles.base);
        drawPart(baseBody, baseMatrix);
        drawPart(baseTop, baseMatrix);

        // The shoulder sits on top of the base and carries the upper arm
        const glm::mat4 shoulderMatrix = baseMatrix
            * makeTranslation(0.0f, baseHeight, 0.0f)
            * makeRotationAroundZ(angles.shoulder);
        drawPart(shoulderSphere, shoulderMatrix);
        drawPart(upperArm, shoulderMatrix);

        // The elbow sits at the end of the upper arm and carries the forearm
        const glm::mat4 elbowMatrix = shoulderMatrix
            * makeTranslation(0.0f, upperArmLength, 0.0f)
            * makeRotationAroundZ(angles.elbow);
        drawPart(elbowSphere, elbowMatrix);
        drawPart(forearm, elbowMatrix);

        // The wrist sits at the end of the forearm and carries the whole hand
        const glm::mat4 wristMatrix = elbowMatrix
            * makeTranslation(0.0f, forearmLength, 0.0f)
            * makeRotationAroundZ(angles.wrist);
        drawPart(wristSphere, wristMatrix);

        // The palm is a box just past the wrist
        const float palmCenter = palmGap + palmHeight * 0.5f;
        drawPart(palm, wristMatrix * makeTranslation(0.0f, palmCenter, 0.0f));

        // Two stems stick out of the top of the palm, one on each side
        const float stemBase = palmGap + palmHeight;
        drawPart(stem, wristMatrix * makeTranslation(stemOffset, stemBase, 0.0f));
        drawPart(stem, wristMatrix * makeTranslation(-stemOffset, stemBase, 0.0f));

        // A finger joint sits on the end of each stem
        const float jointHeight = stemBase + stemLength;
        const glm::mat4 rightJointMatrix = wristMatrix * makeTranslation(stemOffset, jointHeight, 0.0f);
        const glm::mat4 leftJointMatrix = wristMatrix * makeTranslation(-stemOffset, jointHeight, 0.0f);
        drawPart(fingerJoint, rightJointMatrix);
        drawPart(fingerJoint, leftJointMatrix);

        // Each finger swings around its joint, in opposite directions so they open and close together
        const glm::mat4 fingerOffset = makeTranslation(0.0f, fingerLength * 0.5f, 0.0f);
        drawPart(finger, rightJointMatrix * makeRotationAroundZ(angles.grip) * fingerOffset);
        drawPart(finger, leftJointMatrix * makeRotationAroundZ(-angles.grip) * fingerOffset);

    }

private:
    Mesh baseBody;
    Mesh baseTop;
    Mesh shoulderSphere;
    Mesh upperArm;
    Mesh elbowSphere;
    Mesh forearm;
    Mesh wristSphere;
    Mesh palm;
    Mesh stem;
    Mesh fingerJoint;
    Mesh finger;
};

float getKeyDirection(sf::Keyboard::Key increaseKey, sf::Keyboard::Key decreaseKey)
{

    float direction = 0.0f;

    if (sf::Keyboard::isKeyPressed(increaseKey)) {
        direction += 1.0f;
    }

    if (sf::Keyboard::isKeyPressed(decreaseKey)) {
        direction -= 1.0f;
    }

    return direction;

}

void updateJointAngles(JointAngles& angles, float deltaTime)
{

    // How far a joint can turn during this frame
    const float turnAmount = turnSpeed * deltaTime;

    // The base can spin all the way around
    angles.base += getKeyDirection(sf::Keyboard::Key::D, sf::Keyboard::Key::A) * turnAmount;

    // The pitch joints stop at their limits
    angles.shoulder += getKeyDirection(sf::Keyboard::Key::S, sf::Keyboard::Key::W) * turnAmount;
    angles.shoulder = std::clamp(angles.shoulder, -shoulderLimit, shoulderLimit);

    angles.elbow += getKeyDirection(sf::Keyboard::Key::F, sf::Keyboard::Key::R) * turnAmount;
    angles.elbow = std::clamp(angles.elbow, -elbowLimit, elbowLimit);

    angles.wrist += getKeyDirection(sf::Keyboard::Key::G, sf::Keyboard::Key::T) * turnAmount;
    angles.wrist = std::clamp(angles.wrist, -wristLimit, wristLimit);

    // The gripper closes when the angle goes up
    angles.grip += getKeyDirection(sf::Keyboard::Key::C, sf::Keyboard::Key::O) * turnAmount;
    angles.grip = std::clamp(angles.grip, gripMinimum, gripMaximum);

}

Camera makeCamera(float aspectRatio)
{

    // A narrow window limits the view by its width, a wide one by its height
    const float limitingAspect = std::min(aspectRatio, 1.0f);
    const float limitingHalfFieldOfView = std::atan(std::tan(fieldOfView * 0.5f) * limitingAspect);

    // Back up until a sphere around the whole arm fits in the view
    const float cameraDistance = armReach / std::sin(limitingHalfFieldOfView) * cameraPadding;
    const glm::vec3 cameraPosition = armCenter + cameraDirection * cameraDistance;

    // Clip planes hug the sphere so depth precision stays good
    const float nearPlane = std::max(0.1f, cameraDistance - armReach);
    const float farPlane = cameraDistance + armReach;

    Camera camera;
    camera.view = glm::lookAt(cameraPosition, armCenter, glm::vec3(0.0f, 1.0f, 0.0f));
    camera.projection = glm::perspective(fieldOfView, aspectRatio, nearPlane, farPlane);

    return camera;

}

}

int main(int, char* argv[])
{

    try {
        // Ask for an OpenGL 4.0 core window with a depth buffer
        sf::ContextSettings settings;
        settings.majorVersion = 4;
        settings.minorVersion = 0;
        settings.attributeFlags = sf::ContextSettings::Core;
        settings.depthBits = 24;

        sf::Window window(
            sf::VideoMode({900, 700}),
            "Robot arm",
            sf::Style::Default,
            sf::State::Windowed,
            settings
        );

        const sf::ContextSettings actualSettings = window.getSettings();
        if (actualSettings.majorVersion < 4) {

            throw std::runtime_error(
                "OpenGL 4.0 Core is required, but the created context is " +
                std::to_string(actualSettings.majorVersion) + "." +
                std::to_string(actualSettings.minorVersion)
            );

        }

        // Load the OpenGL functions
        if (!gladLoadGL(sf::Context::getFunction)) {

            std::cerr << "Failed to initialize GLAD" << std::endl;
            return 1;

        }

        glEnable(GL_DEPTH_TEST);
        glClearColor(0.85f, 0.85f, 0.88f, 1.0f);

        // Shaders and textures are copied next to the executable
        const std::filesystem::path executableDirectory = std::filesystem::absolute(argv[0]).parent_path();
        AssetPaths paths;
        paths.vertexShader = (executableDirectory / "shaders" / "mesh.vert").string();
        paths.fragmentShader = (executableDirectory / "shaders" / "mesh.frag").string();
        paths.armTexture = (executableDirectory / "assets" / "texture_arm_linkage.png").string();
        paths.baseTexture = (executableDirectory / "assets" / "texture_base_top.png").string();

        RobotArm robotArm(paths);
        JointAngles angles;
        sf::Clock frameClock;

        while (window.isOpen()) {

            // Close on the window button or Escape, and reset the pose on Space
            while (const std::optional event = window.pollEvent()) {

                if (event->is<sf::Event::Closed>()) {

                    window.close();

                } else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {

                    if (keyPressed->code == sf::Keyboard::Key::Escape) {

                        window.close();

                    } else if (keyPressed->code == sf::Keyboard::Key::Space) {

                        angles = resetAngles;

                    }
                }
            }

            if (!window.isOpen()) {
                break;
            }

            // Only react to the keyboard while this window is the active one
            const float deltaTime = frameClock.restart().asSeconds();
            if (window.hasFocus()) {
                updateJointAngles(angles, deltaTime);
            }

            // The camera is rebuilt every frame so it follows the window size
            const sf::Vector2u size = window.getSize();
            glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
            
            const bool hasArea = size.x > 0 && size.y > 0;
            const float aspectRatio = hasArea
                ? static_cast<float>(size.x) / static_cast<float>(size.y)
                : 1.0f;
            const Camera camera = makeCamera(aspectRatio);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            robotArm.draw(angles, camera);
            window.display();

        }
    } catch (const std::exception& error) {

        std::cerr << "Application failed: " << error.what() << std::endl;
        return 1;

    }

    return 0;

}
