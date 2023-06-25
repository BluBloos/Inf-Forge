#version 450

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0, rgba32f) uniform image2D image_output;

void main()
{
    ivec2 pixelCoords = ivec2(gl_GlobalInvocationID.xy);

    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = vec2(pixelCoords) / vec2(imageSize(image_output));
    
    float peak = abs(sin( 10.f*uv.x)); // already 0->1

    float t  = max(0.f,peak - uv.y);
    vec3 col = vec3(1.0,0.0,0.0)*t;

    // Output to screen
    vec4 color = vec4(col, 1.0);
    imageStore(image_output, pixelCoords, color);
}