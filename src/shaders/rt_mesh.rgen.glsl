#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : require

#include "common.glsl"

#define RGEN_SHADER
#include "rt_utils.glsl"

layout(push_constant) uniform Push_Constants {
      layout(offset = 0) uint spp4;
};

layout(binding = 0, rgba8) uniform image2D image;
layout(set=0, binding = 1) uniform accelerationStructureEXT accel;

layout(std140, binding=2) uniform Uniform_Block {
    mat4x3 camera_to_world;
};

layout(location = 0) rayPayloadEXT Ray_Payload payload;

const float tmin = 1e-3f;
const float tmax = 1e+3f;

vec3 trace_ray(vec2 sample_pos) {
    Ray ray = generate_ray(camera_to_world, sample_pos);
    payload.rx_dir = ray.rx_dir;
    payload.ry_dir = ray.ry_dir;
    payload.color = vec3(0);
    traceRayEXT(accel, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, ray.origin, tmin, ray.dir, tmax, 0);
    return payload.color;
}

void main() {
    const vec2 sample_origin = vec2(gl_LaunchIDEXT.xy);
    vec3 color = vec3(0);

    if (spp4 != 0) {
        color += trace_ray(sample_origin + vec2(0.125, 0.375));
        color += trace_ray(sample_origin + vec2(0.375, 0.875));
        color += trace_ray(sample_origin + vec2(0.625, 0.125));
        color += trace_ray(sample_origin + vec2(0.875, 0.625));
        color *= 0.25;
    } else
        color = trace_ray(sample_origin + vec2(0.5));

    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(color, 1.0));
}
