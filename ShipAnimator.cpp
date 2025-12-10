#include "ShipAnimator.h"
#include <cmath>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ShipAnimator::ShipAnimator()
    : m_frameWidth(64)
    , m_frameHeight(64)
    , m_active(false)
    , m_complete(false)
    , m_t(0.0f)
    , m_globalTime(0.0f)
    , m_speed(0.15f)
    , m_waveAmplitude(3.0f)
    , m_waveFrequency(2.0f)
    , m_totalLength(0.0f)
    , m_currentPosition(0.0f, 0.0f)
    , m_currentAngle(0.0f)
    , m_currentFrame(0)
    , m_frameTimer(0.0f)
    , m_frameInterval(0.3f)
    , m_scale(1.0f)
{
}

ShipAnimator::~ShipAnimator()
{
}

bool ShipAnimator::loadSpriteSheet(const std::string& path, int frameWidth, int frameHeight)
{
    if (!m_spriteSheet.loadFromFile(path)) {
        std::cerr << "ShipAnimator: Failed to load sprite sheet: " << path << std::endl;
        return false;
    }
    
    m_frameWidth = frameWidth;
    m_frameHeight = frameHeight;
    
    // Parse the 4x4 grid into frame rectangles
    // Layout: 4 columns x 4 rows = 16 frames
    // We'll map them to 8 directions, 2 frames each
    m_frameRects.clear();
    
    sf::Vector2u texSize = m_spriteSheet.getSize();
    int cols = texSize.x / frameWidth;
    int rows = texSize.y / frameHeight;
    
    // Store all frames in row-major order
    for (int row = 0; row < rows && row < 4; row++) {
        for (int col = 0; col < cols && col < 4; col++) {
            sf::IntRect rect(col * frameWidth, row * frameHeight, frameWidth, frameHeight);
            m_frameRects.push_back(rect);
        }
    }
    
    // Set up the sprite
    m_sprite.setTexture(m_spriteSheet);
    m_sprite.setOrigin(frameWidth / 2.0f, frameHeight / 2.0f);
    
    if (!m_frameRects.empty()) {
        m_sprite.setTextureRect(m_frameRects[0]);
    }
    
    std::cout << "ShipAnimator: Loaded sprite sheet with " << m_frameRects.size() << " frames" << std::endl;
    
    return true;
}

void ShipAnimator::setRoute(const std::vector<sf::Vector2f>& routePoints)
{
    m_path = routePoints;
    calculateSegmentLengths();
    reset();
    
    // Calculate initial angle if we have at least 2 points
    if (m_path.size() >= 2) {
        m_currentAngle = calculateAngle(0.0f);
    }
}

void ShipAnimator::start()
{
    if (m_path.size() < 2) {
        std::cerr << "ShipAnimator: Cannot start - need at least 2 points in path" << std::endl;
        return;
    }
    m_active = true;
    m_complete = false;
}

void ShipAnimator::stop()
{
    m_active = false;
}

void ShipAnimator::reset()
{
    m_t = 0.0f;
    m_globalTime = 0.0f;
    m_complete = false;
    m_currentFrame = 0;
    m_frameTimer = 0.0f;
    
    if (!m_path.empty()) {
        m_currentPosition = m_path[0];
    }
}

void ShipAnimator::calculateSegmentLengths()
{
    m_segmentLengths.clear();
    m_totalLength = 0.0f;
    
    if (m_path.size() < 2) return;
    
    for (size_t i = 1; i < m_path.size(); i++) {
        float dx = m_path[i].x - m_path[i - 1].x;
        float dy = m_path[i].y - m_path[i - 1].y;
        float len = std::sqrt(dx * dx + dy * dy);
        m_segmentLengths.push_back(len);
        m_totalLength += len;
    }
}

sf::Vector2f ShipAnimator::interpolatePosition(float t) const
{
    if (m_path.empty()) return sf::Vector2f(0.0f, 0.0f);
    if (m_path.size() == 1) return m_path[0];
    if (t <= 0.0f) return m_path[0];
    if (t >= 1.0f) return m_path.back();
    
    // Find the distance along the path
    float targetDist = t * m_totalLength;
    float accumDist = 0.0f;
    
    for (size_t i = 0; i < m_segmentLengths.size(); i++) {
        if (accumDist + m_segmentLengths[i] >= targetDist) {
            // Interpolate within this segment
            float segmentT = (m_segmentLengths[i] > 0.0f) 
                ? (targetDist - accumDist) / m_segmentLengths[i]
                : 0.0f;
            
            sf::Vector2f p1 = m_path[i];
            sf::Vector2f p2 = m_path[i + 1];
            
            return sf::Vector2f(
                p1.x + (p2.x - p1.x) * segmentT,
                p1.y + (p2.y - p1.y) * segmentT
            );
        }
        accumDist += m_segmentLengths[i];
    }
    
    return m_path.back();
}

float ShipAnimator::calculateAngle(float t) const
{
    if (m_path.size() < 2) return 0.0f;
    
    // Find current segment
    float targetDist = t * m_totalLength;
    float accumDist = 0.0f;
    
    size_t segmentIdx = 0;
    for (size_t i = 0; i < m_segmentLengths.size(); i++) {
        if (accumDist + m_segmentLengths[i] >= targetDist || i == m_segmentLengths.size() - 1) {
            segmentIdx = i;
            break;
        }
        accumDist += m_segmentLengths[i];
    }
    
    // Calculate angle from segment direction
    sf::Vector2f p1 = m_path[segmentIdx];
    sf::Vector2f p2 = m_path[segmentIdx + 1];
    
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    
    // atan2 returns radians, convert to degrees
    // Note: SFML uses screen coordinates (Y increases downward)
    float angleRad = std::atan2(dy, dx);
    float angleDeg = angleRad * 180.0f / (float)M_PI;
    
    return angleDeg;
}

ShipAnimator::Direction ShipAnimator::angleToDirection(float angleDeg) const
{
    // Normalize angle to 0-360
    while (angleDeg < 0.0f) angleDeg += 360.0f;
    while (angleDeg >= 360.0f) angleDeg -= 360.0f;
    
    // Map angle to 8 directions
    // In screen coordinates (Y down), angles work like this:
    // 0° = Right (E)
    // 90° = Down (S)
    // 180° = Left (W)
    // 270° = Up (N)
    
    // 22.5 degree sectors for each direction
    if (angleDeg < 22.5f || angleDeg >= 337.5f) return DIR_E;
    if (angleDeg < 67.5f) return DIR_SE;
    if (angleDeg < 112.5f) return DIR_S;
    if (angleDeg < 157.5f) return DIR_SW;
    if (angleDeg < 202.5f) return DIR_W;
    if (angleDeg < 247.5f) return DIR_NW;
    if (angleDeg < 292.5f) return DIR_N;
    return DIR_NE;
}

sf::Vector2f ShipAnimator::getPerpendicularOffset(float angleDeg, float offset) const
{
    // Perpendicular is 90 degrees to the travel direction
    float perpAngleRad = (angleDeg + 90.0f) * (float)M_PI / 180.0f;
    return sf::Vector2f(
        std::cos(perpAngleRad) * offset,
        std::sin(perpAngleRad) * offset
    );
}

void ShipAnimator::updateFrame()
{
    // Map direction to frame index
    // Sprite sheet layout (assumed):
    // Row 0: E frame0, E frame1, SE frame0, SE frame1
    // Row 1: S frame0, S frame1, SW frame0, SW frame1
    // Row 2: W frame0, W frame1, NW frame0, NW frame1
    // Row 3: N frame0, N frame1, NE frame0, NE frame1
    
    Direction dir = angleToDirection(m_currentAngle);
    
    // Calculate frame index based on direction layout
    int frameIndex = 0;
    switch (dir) {
        case DIR_E:  frameIndex = 0; break;  // Row 0, col 0-1
        case DIR_SE: frameIndex = 2; break;  // Row 0, col 2-3
        case DIR_S:  frameIndex = 4; break;  // Row 1, col 0-1
        case DIR_SW: frameIndex = 6; break;  // Row 1, col 2-3
        case DIR_W:  frameIndex = 8; break;  // Row 2, col 0-1
        case DIR_NW: frameIndex = 10; break; // Row 2, col 2-3
        case DIR_N:  frameIndex = 12; break; // Row 3, col 0-1
        case DIR_NE: frameIndex = 14; break; // Row 3, col 2-3
    }
    
    // Add animation frame (0 or 1)
    frameIndex += m_currentFrame;
    
    // Clamp to valid range
    if (frameIndex >= 0 && frameIndex < (int)m_frameRects.size()) {
        m_sprite.setTextureRect(m_frameRects[frameIndex]);
    }
}

void ShipAnimator::update(float dt)
{
    if (!m_active || m_complete) return;
    
    m_globalTime += dt;
    
    // Update progress along path
    m_t += m_speed * dt;
    
    if (m_t >= 1.0f) {
        m_t = 1.0f;
        m_complete = true;
        m_active = false;
    }
    
    // Update frame animation timer
    m_frameTimer += dt;
    if (m_frameTimer >= m_frameInterval) {
        m_frameTimer -= m_frameInterval;
        m_currentFrame = (m_currentFrame + 1) % 2;
    }
    
    // Calculate base position on path
    sf::Vector2f basePos = interpolatePosition(m_t);
    
    // Calculate angle for current segment
    m_currentAngle = calculateAngle(m_t);
    
    // Apply wave/bobbing effect (perpendicular oscillation)
    float waveOffset = m_waveAmplitude * std::sin(m_globalTime * m_waveFrequency * 2.0f * (float)M_PI);
    sf::Vector2f perpOffset = getPerpendicularOffset(m_currentAngle, waveOffset);
    
    m_currentPosition = basePos + perpOffset;
    
    // Update sprite frame based on direction
    updateFrame();
}

void ShipAnimator::draw(sf::RenderWindow& window)
{
    if (m_path.empty()) return;
    if (!m_active && !m_complete && m_t <= 0.0f) return;
    
    // Position and scale the sprite
    m_sprite.setPosition(m_currentPosition);
    m_sprite.setScale(m_scale, m_scale);
    
    // Draw the ship sprite
    window.draw(m_sprite);
}
