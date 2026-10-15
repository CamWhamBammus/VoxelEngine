#include "graphics/hud.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace voxel {
namespace {

constexpr const char* vertexShader = R"glsl(#version 410 core
layout(location = 0) in vec2 position;
layout(location = 1) in float ink;
out float textPixel;
void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    textPixel = ink;
}
)glsl";

constexpr const char* fragmentShader = R"glsl(#version 410 core
in float textPixel;
out vec4 fragmentColor;
void main() {
    fragmentColor = textPixel > 0.5 ? vec4(1.0) : vec4(0.0, 0.0, 0.0, 0.75);
}
)glsl";

// Seven rows of five bits, starting at the top-left pixel.
using Glyph = std::array<std::uint8_t, 7>;

Glyph glyph(char character)
{
    if (character >= 'a' && character <= 'z') {
        character = static_cast<char>(character - 'a' + 'A');
    }
    switch (character) {
    case ' ': case '\t': case '\r': return {};
    case 'A': return {14, 17, 17, 31, 17, 17, 17};
    case 'B': return {30, 17, 17, 30, 17, 17, 30};
    case 'C': return {14, 17, 16, 16, 16, 17, 14};
    case 'D': return {30, 17, 17, 17, 17, 17, 30};
    case 'E': return {31, 16, 16, 30, 16, 16, 31};
    case 'F': return {31, 16, 16, 30, 16, 16, 16};
    case 'G': return {14, 17, 16, 23, 17, 17, 15};
    case 'H': return {17, 17, 17, 31, 17, 17, 17};
    case 'I': return {14, 4, 4, 4, 4, 4, 14};
    case 'J': return {7, 2, 2, 2, 18, 18, 12};
    case 'K': return {17, 18, 20, 24, 20, 18, 17};
    case 'L': return {16, 16, 16, 16, 16, 16, 31};
    case 'M': return {17, 27, 21, 21, 17, 17, 17};
    case 'N': return {17, 25, 25, 21, 19, 19, 17};
    case 'O': return {14, 17, 17, 17, 17, 17, 14};
    case 'P': return {30, 17, 17, 30, 16, 16, 16};
    case 'Q': return {14, 17, 17, 17, 21, 18, 13};
    case 'R': return {30, 17, 17, 30, 20, 18, 17};
    case 'S': return {15, 16, 16, 14, 1, 1, 30};
    case 'T': return {31, 4, 4, 4, 4, 4, 4};
    case 'U': return {17, 17, 17, 17, 17, 17, 14};
    case 'V': return {17, 17, 17, 17, 17, 10, 4};
    case 'W': return {17, 17, 17, 21, 21, 21, 10};
    case 'X': return {17, 17, 10, 4, 10, 17, 17};
    case 'Y': return {17, 17, 10, 4, 4, 4, 4};
    case 'Z': return {31, 1, 2, 4, 8, 16, 31};
    case '0': return {14, 17, 19, 21, 25, 17, 14};
    case '1': return {4, 12, 4, 4, 4, 4, 14};
    case '2': return {14, 17, 1, 2, 4, 8, 31};
    case '3': return {30, 1, 1, 14, 1, 1, 30};
    case '4': return {2, 6, 10, 18, 31, 2, 2};
    case '5': return {31, 16, 16, 30, 1, 1, 30};
    case '6': return {14, 16, 16, 30, 17, 17, 14};
    case '7': return {31, 1, 2, 4, 8, 8, 8};
    case '8': return {14, 17, 17, 14, 17, 17, 14};
    case '9': return {14, 17, 17, 15, 1, 1, 14};
    case '.': return {0, 0, 0, 0, 0, 12, 12};
    case ',': return {0, 0, 0, 0, 0, 4, 8};
    case ':': return {0, 4, 4, 0, 4, 4, 0};
    case ';': return {0, 4, 4, 0, 4, 4, 8};
    case '!': return {4, 4, 4, 4, 4, 0, 4};
    case '-': return {0, 0, 0, 31, 0, 0, 0};
    case '+': return {0, 4, 4, 31, 4, 4, 0};
    case '=': return {0, 0, 31, 0, 31, 0, 0};
    case '_': return {0, 0, 0, 0, 0, 0, 31};
    case '/': return {1, 2, 2, 4, 8, 8, 16};
    case '\\': return {16, 8, 8, 4, 2, 2, 1};
    case '|': return {4, 4, 4, 4, 4, 4, 4};
    case '(': return {2, 4, 8, 8, 8, 4, 2};
    case ')': return {8, 4, 2, 2, 2, 4, 8};
    case '[': return {14, 8, 8, 8, 8, 8, 14};
    case ']': return {14, 2, 2, 2, 2, 2, 14};
    case '<': return {1, 2, 4, 8, 4, 2, 1};
    case '>': return {16, 8, 4, 2, 4, 8, 16};
    case '%': return {25, 25, 2, 4, 8, 19, 19};
    case '#': return {10, 10, 31, 10, 31, 10, 10};
    case '*': return {0, 21, 14, 31, 14, 21, 0};
    case '"': return {10, 10, 10, 0, 0, 0, 0};
    case '\'': return {4, 4, 8, 0, 0, 0, 0};
    default: return {14, 17, 1, 2, 4, 0, 4};
    }
}

struct Vertex {
    float x;
    float y;
    float ink;
};

class SavedState {
public:
    SavedState()
    {
        depthTest_ = glIsEnabled(GL_DEPTH_TEST);
        cullFace_ = glIsEnabled(GL_CULL_FACE);
        blend_ = glIsEnabled(GL_BLEND);
        scissorTest_ = glIsEnabled(GL_SCISSOR_TEST);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask_);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao_);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer_);
        glGetIntegerv(GL_POLYGON_MODE, polygonMode_.data());
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSourceRgb_);
        glGetIntegerv(GL_BLEND_DST_RGB, &blendDestinationRgb_);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSourceAlpha_);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDestinationAlpha_);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb_);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha_);
    }

    ~SavedState()
    {
        restore(GL_DEPTH_TEST, depthTest_);
        restore(GL_CULL_FACE, cullFace_);
        restore(GL_BLEND, blend_);
        restore(GL_SCISSOR_TEST, scissorTest_);
        glDepthMask(depthMask_);
        glBlendFuncSeparate(blendSourceRgb_, blendDestinationRgb_,
                            blendSourceAlpha_, blendDestinationAlpha_);
        glBlendEquationSeparate(blendEquationRgb_, blendEquationAlpha_);
        // Core OpenGL applies one polygon mode to both faces.
        glPolygonMode(GL_FRONT_AND_BACK, polygonMode_[0]);
        glUseProgram(static_cast<GLuint>(program_));
        glBindVertexArray(static_cast<GLuint>(vao_));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer_));
    }

private:
    static void restore(GLenum capability, GLboolean enabled)
    {
        if (enabled) glEnable(capability);
        else glDisable(capability);
    }

    GLboolean depthTest_ = GL_FALSE;
    GLboolean cullFace_ = GL_FALSE;
    GLboolean blend_ = GL_FALSE;
    GLboolean scissorTest_ = GL_FALSE;
    GLboolean depthMask_ = GL_TRUE;
    GLint program_ = 0;
    GLint vao_ = 0;
    GLint buffer_ = 0;
    std::array<GLint, 2> polygonMode_{};
    GLint blendSourceRgb_ = 0;
    GLint blendDestinationRgb_ = 0;
    GLint blendSourceAlpha_ = 0;
    GLint blendDestinationAlpha_ = 0;
    GLint blendEquationRgb_ = 0;
    GLint blendEquationAlpha_ = 0;
};

} // namespace

Hud::Hud() : shader_(vertexShader, fragmentShader)
{
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &buffer_);
    if (!vao_ || !buffer_) {
        glDeleteBuffers(1, &buffer_);
        glDeleteVertexArrays(1, &vao_);
        throw std::runtime_error("Failed to allocate HUD resources");
    }
    GLint previousVao = 0;
    GLint previousBuffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousBuffer);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, ink)));
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousBuffer));
}

Hud::~Hud()
{
    glDeleteBuffers(1, &buffer_);
    glDeleteVertexArrays(1, &vao_);
}

void Hud::draw(std::string_view text, int framebufferWidth, int framebufferHeight,
               float contentScale)
{
    if (text.empty() || framebufferWidth <= 0 || framebufferHeight <= 0) return;
    if (!std::isfinite(contentScale) || contentScale <= 0.0f) contentScale = 1.0f;

    const float pixel = std::round(2.0f * std::clamp(contentScale, 0.5f, 4.0f));
    const float margin = 6.0f * pixel;
    const float padding = 4.0f * pixel;
    const float advance = 6.0f * pixel;
    const float lineHeight = 9.0f * pixel;
    const float availableWidth = framebufferWidth - 2.0f * (margin + padding);
    const float availableHeight = framebufferHeight - 2.0f * (margin + padding);
    if (availableWidth < advance || availableHeight < 7.0f * pixel) return;

    // Bound geometry even for unusually large framebuffers or input strings.
    const auto columns = static_cast<std::size_t>(std::min(160.0f, availableWidth / advance));
    const auto rows = static_cast<std::size_t>(std::min(32.0f,
        (availableHeight + 2.0f * pixel) / lineHeight));
    std::vector<std::string_view> lines;
    std::size_t widest = 0;
    while (!text.empty() && lines.size() < rows) {
        const std::size_t end = text.find('\n');
        const std::size_t length = end == std::string_view::npos ? text.size() : end;
        const auto line = text.substr(0, std::min(length, columns));
        widest = std::max(widest, line.size());
        lines.push_back(line);
        if (end == std::string_view::npos) break;
        text.remove_prefix(end + 1);
    }
    if (lines.empty()) return;

    std::vector<Vertex> vertices;
    vertices.reserve(6 + lines.size() * widest * 18 * 6);
    const auto quad = [&](float x, float y, float width, float height, float ink) {
        const float left = 2.0f * x / framebufferWidth - 1.0f;
        const float right = 2.0f * (x + width) / framebufferWidth - 1.0f;
        const float top = 1.0f - 2.0f * y / framebufferHeight;
        const float bottom = 1.0f - 2.0f * (y + height) / framebufferHeight;
        vertices.insert(vertices.end(), {{left, top, ink}, {right, top, ink},
            {right, bottom, ink}, {left, top, ink}, {right, bottom, ink}, {left, bottom, ink}});
    };
    quad(margin, margin, widest * advance + 2.0f * padding,
         lines.size() * lineHeight - 2.0f * pixel + 2.0f * padding, 0.0f);
    for (std::size_t line = 0; line < lines.size(); ++line) {
        for (std::size_t character = 0; character < lines[line].size(); ++character) {
            const Glyph bitmap = glyph(lines[line][character]);
            for (int y = 0; y < 7; ++y) {
                for (int x = 0; x < 5; ++x) {
                    if ((bitmap[y] & (1u << (4 - x))) != 0) {
                        quad(margin + padding + character * advance + x * pixel,
                             margin + padding + line * lineHeight + y * pixel,
                             pixel, pixel, 1.0f);
                    }
                }
            }
        }
    }

    const SavedState savedState;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    shader_.use();
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, buffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
}

} // namespace voxel
