#include "matrix.h"
#include "mesh.h"
#include "draw_mesh.h"
#include "vk_utils.h"

namespace {
struct Uniform_Buffer {
    Matrix4x4 model_view_proj;
    Matrix4x4 model_view;
};
}

void Draw_Mesh::create(VkRenderPass render_pass, VkImageView texture_view, VkSampler sampler) {
    uniform_buffer = vk_create_mapped_buffer(static_cast<VkDeviceSize>(sizeof(Uniform_Buffer)),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &mapped_uniform_buffer, "raster_uniform_buffer");

    descriptor_set_layout = Descriptor_Set_Layout()
        .uniform_buffer (0, VK_SHADER_STAGE_VERTEX_BIT)
        .sampled_image (1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .sampler (2, VK_SHADER_STAGE_FRAGMENT_BIT)
        .create ("raster_set_layout");

    // pipeline layout
    {
        VkPushConstantRange push_constant_range; // show_texture_lods value
        push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = 4;

        VkPipelineLayoutCreateInfo create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        create_info.setLayoutCount = 1;
        create_info.pSetLayouts = &descriptor_set_layout;
        create_info.pushConstantRangeCount = 1;
        create_info.pPushConstantRanges = &push_constant_range;

        VK_CHECK(vkCreatePipelineLayout(vk.device, &create_info, nullptr, &pipeline_layout));
        vk_set_debug_name(pipeline_layout, "raster_pipeline_layout");
    }

    // pipeline
    {
        VkShaderModule vertex_shader = vk_load_spirv("spirv/raster_mesh.vert.spv");
        VkShaderModule fragment_shader = vk_load_spirv("spirv/raster_mesh.frag.spv");

        Vk_Graphics_Pipeline_State state = get_default_graphics_pipeline_state();

        // VkVertexInputBindingDescription
        state.vertex_bindings[0].binding = 0;
        state.vertex_bindings[0].stride = sizeof(Vertex);
        state.vertex_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        state.vertex_binding_count = 1;

        // VkVertexInputAttributeDescription
        state.vertex_attributes[0].location = 0; // vertex
        state.vertex_attributes[0].binding = 0;
        state.vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        state.vertex_attributes[0].offset = 0;

        state.vertex_attributes[1].location = 1; // normal
        state.vertex_attributes[1].binding = 0;
        state.vertex_attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        state.vertex_attributes[1].offset = 12;

        state.vertex_attributes[2].location = 2; // uv
        state.vertex_attributes[2].binding = 0;
        state.vertex_attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
        state.vertex_attributes[2].offset = 24;
        state.vertex_attribute_count = 3;

        pipeline = vk_create_graphics_pipeline(state, pipeline_layout, render_pass, vertex_shader, fragment_shader);

        vkDestroyShaderModule(vk.device, vertex_shader, nullptr);
        vkDestroyShaderModule(vk.device, fragment_shader, nullptr);
    }

    // descriptor sets
    {
        VkDescriptorSetAllocateInfo desc { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        desc.descriptorPool = vk.descriptor_pool;
        desc.descriptorSetCount = 1;
        desc.pSetLayouts = &descriptor_set_layout;
        VK_CHECK(vkAllocateDescriptorSets(vk.device, &desc, &descriptor_set));

        Descriptor_Writes(descriptor_set)
            .uniform_buffer(0, uniform_buffer.handle, 0, sizeof(Uniform_Buffer))
            .sampled_image(1, texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .sampler(2, sampler);
    }
}

void Draw_Mesh::destroy() {
    uniform_buffer.destroy();
    vkDestroyDescriptorSetLayout(vk.device, descriptor_set_layout, nullptr);
    vkDestroyPipelineLayout(vk.device, pipeline_layout, nullptr);
    vkDestroyPipeline(vk.device, pipeline, nullptr);
    *this = Draw_Mesh{};
}

void Draw_Mesh::update(const Matrix3x4& model_transform, const Matrix3x4& view_transform) {
    float aspect_ratio = (float)vk.surface_size.width / (float)vk.surface_size.height;
    Matrix4x4 proj = perspective_transform_opengl_z01(radians(45.0f), aspect_ratio, 0.1f, 50.0f);
    Matrix4x4 model_view = Matrix4x4::identity * view_transform * model_transform;
    Matrix4x4 model_view_proj = proj * view_transform * model_transform;
    static_cast<Uniform_Buffer*>(mapped_uniform_buffer)->model_view_proj = model_view_proj;
    static_cast<Uniform_Buffer*>(mapped_uniform_buffer)->model_view = model_view;
}

void Draw_Mesh::dispatch(const GPU_Mesh& mesh, bool show_texture_lod) {
    const VkDeviceSize zero_offset = 0;
    vkCmdBindVertexBuffers(vk.command_buffer, 0, 1, &mesh.vertex_buffer.handle, &zero_offset);
    vkCmdBindIndexBuffer(vk.command_buffer, mesh.index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

    uint32_t show_texture_lod_uint = show_texture_lod;
    vkCmdPushConstants(vk.command_buffer, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4, &show_texture_lod_uint);

    vkCmdBindDescriptorSets(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
    vkCmdBindPipeline(vk.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDrawIndexed(vk.command_buffer, mesh.index_count, 1, 0, 0, 0);
}
