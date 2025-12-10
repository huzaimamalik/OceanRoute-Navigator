#ifndef SHIP_ANIMATOR_H
#define SHIP_ANIMATOR_H

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>

/**
 * ShipAnimator - Sprite-based ship animation with directional sprites and wave effect
 * 
 * Sprite Sheet Layout (4x4 grid = 16 frames):
 * The sprite sheet contains 8 directions, each with 2 animation frames:
 * 
 * Row 0: E (0,1), SE (2,3)
 * Row 1: S (0,1), SW (2,3)
 * Row 2: W (0,1), NW (2,3)
 * Row 3: N (0,1), NE (2,3)
 * 
 * Direction mapping (angle in degrees, 0 = East, counter-clockwise):
 * E:  -22.5 to 22.5   (or 337.5 to 360 and 0 to 22.5)
 * NE: 22.5 to 67.5
 * N:  67.5 to 112.5
 * NW: 112.5 to 157.5
 * W:  157.5 to 202.5
 * SW: 202.5 to 247.5
 * S:  247.5 to 292.5
 * SE: 292.5 to 337.5
 */
class ShipAnimator {
public:
    ShipAnimator();
    ~ShipAnimator();
    
    // Initialize the animator with sprite sheet
    bool loadSpriteSheet(const std::string& path, int frameWidth, int frameHeight);
    
    // Set the route path as a polyline of points
    void setRoute(const std::vector<sf::Vector2f>& routePoints);
    
    // Start/stop animation
    void start();
    void stop();
    void reset();
    
    // Update animation state
    // dt: delta time in seconds
    void update(float dt);
    
    // Draw the ship to the render target
    void draw(sf::RenderWindow& window);
    
    // Check if animation is active
    bool isActive() const { return m_active; }
    
    // Check if animation has completed
    bool isComplete() const { return m_complete; }
    
    // Get current progress (0 to 1)
    float getProgress() const { return m_t; }
    
    // Set animation speed (units per second along the path, normalized)
    void setSpeed(float speed) { m_speed = speed; }
    
    // Set wave/bobbing parameters
    void setWaveAmplitude(float amplitude) { m_waveAmplitude = amplitude; }
    void setWaveFrequency(float frequency) { m_waveFrequency = frequency; }
    
    // Get current position (useful for trail effects)
    sf::Vector2f getPosition() const { return m_currentPosition; }
    float getAngle() const { return m_currentAngle; }
    
    // Set scale for the ship sprite
    void setScale(float scale) { m_scale = scale; }

private:
    // Sprite sheet and frames
    sf::Texture m_spriteSheet;
    std::vector<sf::IntRect> m_frameRects;  // All 16 frame rectangles
    sf::Sprite m_sprite;
    
    int m_frameWidth;
    int m_frameHeight;
    
    // Direction indices for each of 8 directions (each has 2 frames)
    // Index into m_frameRects: direction * 2 + animFrame
    enum Direction {
        DIR_E = 0,
        DIR_SE = 1,
        DIR_S = 2,
        DIR_SW = 3,
        DIR_W = 4,
        DIR_NW = 5,
        DIR_N = 6,
        DIR_NE = 7
    };
    
    // Animation state
    bool m_active;
    bool m_complete;
    float m_t;              // Progress along path (0 to 1)
    float m_globalTime;     // Total elapsed time for wave effect
    float m_speed;          // Animation speed (progress per second)
    
    // Wave/bobbing effect
    float m_waveAmplitude;  // Pixels of perpendicular movement
    float m_waveFrequency;  // Oscillations per second
    
    // Path data
    std::vector<sf::Vector2f> m_path;
    std::vector<float> m_segmentLengths;
    float m_totalLength;
    
    // Current state
    sf::Vector2f m_currentPosition;
    float m_currentAngle;       // Degrees, 0 = East (right), counter-clockwise
    int m_currentFrame;         // For 2-frame animation cycle
    float m_frameTimer;         // Timer for frame switching
    float m_frameInterval;      // Time between frame switches
    
    // Scale
    float m_scale;
    
    // Helper methods
    void calculateSegmentLengths();
    sf::Vector2f interpolatePosition(float t) const;
    float calculateAngle(float t) const;
    Direction angleToDirection(float angleDeg) const;
    void updateFrame();
    sf::Vector2f getPerpendicularOffset(float angleDeg, float offset) const;
};

#endif // SHIP_ANIMATOR_H
