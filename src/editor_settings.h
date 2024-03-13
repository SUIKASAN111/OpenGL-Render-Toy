#pragma once
#include <string>
#include <vector>

struct WindowSize
{
    unsigned int width;
    unsigned int height;

    WindowSize(int x, int y)
    {
        width = x;
        height = y;
    }

    std::string to_string()
    {
        return std::to_string(width) + "x" + std::to_string(height);
    }
};

static class EditorSettings
{
public:
    static bool UsePolygonMode;
    static bool UsePostProcess;
    static bool DrawGizmos;
    static bool SkyboxEnabled;
    static std::vector<WindowSize> window_size_list;
};