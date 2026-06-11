#pragma once
#include <string>
#include <vector>
#include <windows.h>

// Simple texture class for CPU-side texture sampling
class Texture
{
public:
    Texture();
    ~Texture();

    // Load texture from file (supports BMP, PNG via Windows API)
    bool LoadFromFile(const std::string& filepath);

    // Sample texture using bilinear filtering
    // uv coordinates should be in [0, 1] range
    uint32_t SampleBilinear(float u, float v) const;

    // Get raw texture dimensions
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    bool IsLoaded() const { return m_width > 0 && m_height > 0; }

private:
    // Helper for nearest neighbor sampling
    uint32_t SampleNearest(float u, float v) const;

    // Texture data in ARGB format
    std::vector<uint32_t> m_texels;
    int m_width = 0;
    int m_height = 0;
};

// Global texture manager
class TextureManager
{
public:
    static TextureManager& GetInstance();

    // Load or get cached texture
    Texture* LoadTexture(const std::string& filepath);

    // Clear all loaded textures
    void ClearAll();

    // Check if texture is loaded
    bool HasTexture(const std::string& filepath) const;

private:
    TextureManager() = default;
    ~TextureManager();

    std::vector<std::pair<std::string, Texture*>> m_textures;
};
