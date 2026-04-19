// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "../../common/common_types.h"
#include <vector>

namespace GPU {
namespace Geometry {

// ============================================================================
// Simple Math Types (to avoid external dependencies)
// ============================================================================

struct Vec2 {
    f32 x = 0.0f;
    f32 y = 0.0f;
    constexpr Vec2() = default;
    constexpr Vec2(f32 x_, f32 y_) : x(x_), y(y_) {}
};
struct Vec3 { f32 x, y, z; };
struct Vec4 {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 0.0f;
    constexpr Vec4() = default;
    constexpr Vec4(f32 x_, f32 y_, f32 z_, f32 w_) : x(x_), y(y_), z(z_), w(w_) {}
};
struct Mat44 { f32 m[16]; };

inline constexpr Vec2 operator+(const Vec2& lhs, const Vec2& rhs) {
    return Vec2{lhs.x + rhs.x, lhs.y + rhs.y};
}

inline constexpr Vec2 operator*(const Vec2& lhs, f32 rhs) {
    return Vec2{lhs.x * rhs, lhs.y * rhs};
}

inline constexpr Vec2 operator*(f32 lhs, const Vec2& rhs) {
    return rhs * lhs;
}

inline constexpr Vec4 operator+(const Vec4& lhs, const Vec4& rhs) {
    return Vec4{lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w};
}

inline constexpr Vec4 operator*(const Vec4& lhs, f32 rhs) {
    return Vec4{lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs};
}

inline constexpr Vec4 operator*(f32 lhs, const Vec4& rhs) {
    return rhs * lhs;
}

// ============================================================================
// Vertex Formats (3DS)
// ============================================================================

struct Vertex {
    Vec3 position;      // 3D position
    Vec3 normal;        // Normal vector
    Vec2 tex_coord;     // Texture coordinates
    Vec4 color;         // RGBA color
};

// ============================================================================
// Vertex Shader
// ============================================================================

class VertexShader {
public:
    VertexShader();
    ~VertexShader() = default;
    
    // Load shader code
    void LoadBinary(const u8* data, u32 size);
    
    // Execute vertex shader on input vertex
    Vertex Execute(const Vertex& input, const Mat44& model_matrix,
                   const Mat44& view_matrix, const Mat44& proj_matrix);
    
    // Vertex shader state
    void SetUniform(u32 index, const Vec4& value);
    Vec4 GetUniform(u32 index) const;

private:
    std::vector<u32> shader_code;
    std::vector<Vec4> uniforms;
    
    static constexpr u32 MAX_UNIFORMS = 96;
};

// ============================================================================
// Geometry Processing
// ============================================================================

class GeometryProcessor {
public:
    GeometryProcessor();
    ~GeometryProcessor() = default;
    
    // Set transformation matrices
    void SetModelMatrix(const Mat44& mat);
    void SetViewMatrix(const Mat44& mat);
    void SetProjectionMatrix(const Mat44& mat);
    
    // Process vertices
    std::vector<Vertex> ProcessVertices(const std::vector<Vertex>& input_vertices);
    
    // Get transformation matrices
    const Mat44& GetModelMatrix() const { return model_matrix; }
    const Mat44& GetViewMatrix() const { return view_matrix; }
    const Mat44& GetProjectionMatrix() const { return projection_matrix; }

private:
    Mat44 model_matrix;
    Mat44 view_matrix;
    Mat44 projection_matrix;
    
    VertexShader vertex_shader;
};

// ============================================================================
// Primitive Assembly
// ============================================================================

enum class PrimitiveType {
    PointList = 0,
    LineList = 1,
    LineStrip = 2,
    TriangleList = 3,
    TriangleStrip = 4,
    TriangleFan = 5,
};

struct Primitive {
    std::vector<Vertex> vertices;
    PrimitiveType type;
};

// ============================================================================
// Primitive Assembler
// ============================================================================

class PrimitiveAssembler {
public:
    PrimitiveAssembler();
    ~PrimitiveAssembler() = default;
    
    // Assemble primitives from processed vertices
    std::vector<Primitive> AssemblePrimitives(
        const std::vector<Vertex>& vertices,
        PrimitiveType type,
        const std::vector<u16>& indices = {});

private:
    // Helper functions for primitive assembly
    std::vector<Primitive> AssembleTriangleList(
        const std::vector<Vertex>& vertices,
        const std::vector<u16>& indices);
    
    std::vector<Primitive> AssembleTriangleStrip(
        const std::vector<Vertex>& vertices,
        const std::vector<u16>& indices);
};

} // namespace Geometry

using Vec2 = Geometry::Vec2;
using Vec3 = Geometry::Vec3;
using Vec4 = Geometry::Vec4;
using Mat44 = Geometry::Mat44;
} // namespace GPU
