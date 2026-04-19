// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "geometry.h"
#include "../../common/log.h"
#include <cstring>
#include <algorithm>

namespace GPU {
namespace Geometry {

// ============================================================================
// Vertex Shader Implementation
// ============================================================================

VertexShader::VertexShader() {
    uniforms.resize(MAX_UNIFORMS);
    std::fill(uniforms.begin(), uniforms.end(), Vec4{0, 0, 0, 0});
}

void VertexShader::LoadBinary(const u8* data, u32 size) {
    shader_code.clear();
    shader_code.resize(size / sizeof(u32));
    std::memcpy(shader_code.data(), data, size);
    NOTICE_LOG(GPU, "Vertex Shader loaded: %u instructions", (u32)shader_code.size());
}

Vertex VertexShader::Execute(const Vertex& input, const Mat44& model_matrix,
                             const Mat44& view_matrix, const Mat44& proj_matrix) {
    Vertex output = input;
    
    // In real implementation, would execute shader bytecode
    // For now, just apply transformations
    
    // Transform position: output_pos = proj * view * model * input_pos
    Vec3 world_pos;
    world_pos.x = input.position.x * model_matrix.m[0] + input.position.y * model_matrix.m[4] + 
                  input.position.z * model_matrix.m[8] + model_matrix.m[12];
    world_pos.y = input.position.x * model_matrix.m[1] + input.position.y * model_matrix.m[5] + 
                  input.position.z * model_matrix.m[9] + model_matrix.m[13];
    world_pos.z = input.position.x * model_matrix.m[2] + input.position.y * model_matrix.m[6] + 
                  input.position.z * model_matrix.m[10] + model_matrix.m[14];
    
    output.position = world_pos;
    
    return output;
}

void VertexShader::SetUniform(u32 index, const Vec4& value) {
    if (index < MAX_UNIFORMS) {
        uniforms[index] = value;
    }
}

Vec4 VertexShader::GetUniform(u32 index) const {
    if (index < MAX_UNIFORMS) {
        return uniforms[index];
    }
    return Vec4{0, 0, 0, 0};
}

// ============================================================================
// Geometry Processor Implementation
// ============================================================================

GeometryProcessor::GeometryProcessor() {
    // Identity matrix transformation
    model_matrix.m[0] = 1.0f; model_matrix.m[1] = 0; model_matrix.m[2] = 0; model_matrix.m[3] = 0;
    model_matrix.m[4] = 0; model_matrix.m[5] = 1.0f; model_matrix.m[6] = 0; model_matrix.m[7] = 0;
    model_matrix.m[8] = 0; model_matrix.m[9] = 0; model_matrix.m[10] = 1.0f; model_matrix.m[11] = 0;
    model_matrix.m[12] = 0; model_matrix.m[13] = 0; model_matrix.m[14] = 0; model_matrix.m[15] = 1.0f;
    
    view_matrix = model_matrix;
    projection_matrix = model_matrix;
    
    NOTICE_LOG(GPU, "GeometryProcessor created");
}

void GeometryProcessor::SetModelMatrix(const Mat44& mat) {
    model_matrix = mat;
    NOTICE_LOG(GPU, "Model matrix updated");
}

void GeometryProcessor::SetViewMatrix(const Mat44& mat) {
    view_matrix = mat;
    NOTICE_LOG(GPU, "View matrix updated");
}

void GeometryProcessor::SetProjectionMatrix(const Mat44& mat) {
    projection_matrix = mat;
    NOTICE_LOG(GPU, "Projection matrix updated");
}

std::vector<Vertex> GeometryProcessor::ProcessVertices(const std::vector<Vertex>& input_vertices) {
    std::vector<Vertex> output_vertices;
    output_vertices.reserve(input_vertices.size());
    
    for (const auto& vertex : input_vertices) {
        Vertex processed = vertex_shader.Execute(vertex, model_matrix, view_matrix, projection_matrix);
        output_vertices.push_back(processed);
    }
    
    return output_vertices;
}

// ============================================================================
// Primitive Assembler Implementation
// ============================================================================

PrimitiveAssembler::PrimitiveAssembler() {
    NOTICE_LOG(GPU, "PrimitiveAssembler created");
}

std::vector<Primitive> PrimitiveAssembler::AssemblePrimitives(
    const std::vector<Vertex>& vertices,
    PrimitiveType type,
    const std::vector<u16>& indices) {
    
    std::vector<Primitive> primitives;
    
    if (type == PrimitiveType::TriangleList) {
        primitives = AssembleTriangleList(vertices, indices);
    } else if (type == PrimitiveType::TriangleStrip) {
        primitives = AssembleTriangleStrip(vertices, indices);
    }
    
    NOTICE_LOG(GPU, "Assembled %lu primitives", primitives.size());
    
    return primitives;
}

std::vector<Primitive> PrimitiveAssembler::AssembleTriangleList(
    const std::vector<Vertex>& vertices,
    const std::vector<u16>& indices) {
    
    std::vector<Primitive> primitives;
    
    u32 vertex_count = indices.empty() ? vertices.size() : indices.size();
    
    for (u32 i = 0; i + 2 < vertex_count; i += 3) {
        Primitive prim;
        prim.type = PrimitiveType::TriangleList;
        
        if (indices.empty()) {
            prim.vertices.push_back(vertices[i]);
            prim.vertices.push_back(vertices[i + 1]);
            prim.vertices.push_back(vertices[i + 2]);
        } else {
            prim.vertices.push_back(vertices[indices[i]]);
            prim.vertices.push_back(vertices[indices[i + 1]]);
            prim.vertices.push_back(vertices[indices[i + 2]]);
        }
        
        primitives.push_back(prim);
    }
    
    return primitives;
}

std::vector<Primitive> PrimitiveAssembler::AssembleTriangleStrip(
    const std::vector<Vertex>& vertices,
    const std::vector<u16>& indices) {
    
    std::vector<Primitive> primitives;
    
    u32 vertex_count = indices.empty() ? vertices.size() : indices.size();
    bool flip = false;
    
    for (u32 i = 0; i + 2 < vertex_count; ++i) {
        Primitive prim;
        prim.type = PrimitiveType::TriangleStrip;
        
        if (indices.empty()) {
            if (flip) {
                prim.vertices.push_back(vertices[i + 1]);
                prim.vertices.push_back(vertices[i]);
                prim.vertices.push_back(vertices[i + 2]);
            } else {
                prim.vertices.push_back(vertices[i]);
                prim.vertices.push_back(vertices[i + 1]);
                prim.vertices.push_back(vertices[i + 2]);
            }
        } else {
            if (flip) {
                prim.vertices.push_back(vertices[indices[i + 1]]);
                prim.vertices.push_back(vertices[indices[i]]);
                prim.vertices.push_back(vertices[indices[i + 2]]);
            } else {
                prim.vertices.push_back(vertices[indices[i]]);
                prim.vertices.push_back(vertices[indices[i + 1]]);
                prim.vertices.push_back(vertices[indices[i + 2]]);
            }
        }
        
        primitives.push_back(prim);
        flip = !flip;
    }
    
    return primitives;
}

} // namespace Geometry
} // namespace GPU
