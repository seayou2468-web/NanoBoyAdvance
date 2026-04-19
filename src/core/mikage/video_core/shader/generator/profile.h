// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "../../../common/common_types.h"

namespace Pica::Shader {

struct VKFormatTraits {
    u8 transfer_support{};
    u8 blit_support{};
    u8 attachment_support{};
    u8 storage_support{};
    u8 needs_conversion{};
    u8 needs_emulation{};
    u32 usage_flags{};
    u32 aspect_flags{};
    u32 native_format{};

    bool operator==(const VKFormatTraits& rhs) const {
        return transfer_support == rhs.transfer_support && blit_support == rhs.blit_support &&
               attachment_support == rhs.attachment_support &&
               storage_support == rhs.storage_support &&
               needs_conversion == rhs.needs_conversion &&
               needs_emulation == rhs.needs_emulation && usage_flags == rhs.usage_flags &&
               aspect_flags == rhs.aspect_flags && native_format == rhs.native_format;
    }
};

struct Profile {
    u8 enable_accurate_mul{};
    u8 has_separable_shaders{};
    u8 has_clip_planes{};
    u8 has_geometry_shader{};
    u8 has_custom_border_color{};
    u8 has_fragment_shader_interlock{};
    u8 has_fragment_shader_barycentric{};
    u8 has_blend_minmax_factor{};
    u8 has_minus_one_to_one_range{};
    u8 has_logic_op{};

    u8 has_gl_ext_framebuffer_fetch{};
    u8 has_gl_arm_framebuffer_fetch{};
    u8 has_gl_nv_fragment_shader_interlock{};
    u8 has_gl_intel_fragment_shader_ordering{};
    u8 has_gl_nv_fragment_shader_barycentric{};

    u8 vk_disable_spirv_optimizer{};
    u8 vk_use_spirv_generator{};
    std::array<VKFormatTraits, 16> vk_format_traits{};

    u8 is_vulkan{};

    bool operator==(const Profile& rhs) const {
        return enable_accurate_mul == rhs.enable_accurate_mul &&
               has_separable_shaders == rhs.has_separable_shaders &&
               has_clip_planes == rhs.has_clip_planes &&
               has_geometry_shader == rhs.has_geometry_shader &&
               has_custom_border_color == rhs.has_custom_border_color &&
               has_fragment_shader_interlock == rhs.has_fragment_shader_interlock &&
               has_fragment_shader_barycentric == rhs.has_fragment_shader_barycentric &&
               has_blend_minmax_factor == rhs.has_blend_minmax_factor &&
               has_minus_one_to_one_range == rhs.has_minus_one_to_one_range &&
               has_logic_op == rhs.has_logic_op &&
               has_gl_ext_framebuffer_fetch == rhs.has_gl_ext_framebuffer_fetch &&
               has_gl_arm_framebuffer_fetch == rhs.has_gl_arm_framebuffer_fetch &&
               has_gl_nv_fragment_shader_interlock == rhs.has_gl_nv_fragment_shader_interlock &&
               has_gl_intel_fragment_shader_ordering ==
                   rhs.has_gl_intel_fragment_shader_ordering &&
               has_gl_nv_fragment_shader_barycentric == rhs.has_gl_nv_fragment_shader_barycentric &&
               vk_disable_spirv_optimizer == rhs.vk_disable_spirv_optimizer &&
               vk_use_spirv_generator == rhs.vk_use_spirv_generator &&
               vk_format_traits == rhs.vk_format_traits && is_vulkan == rhs.is_vulkan;
    }
};

} // namespace Pica::Shader
