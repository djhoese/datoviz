#include "../src/vklite2_utils.h"


/*************************************************************************************************/
/*  Utils                                                                                        */
/*************************************************************************************************/

#define TEST_END                                                                                  \
    vkl_app_destroy(app);                                                                         \
    return app->n_errors != 0;


#define TEST_WIDTH  640
#define TEST_HEIGHT 480

static const VkClearColorValue bgcolor = {{.4f, .6f, .8f, 1.0f}};
#define TEST_FORMAT VK_FORMAT_B8G8R8A8_UNORM



static VklRenderpass* default_renderpass(
    VklGpu* gpu, VkClearColorValue clear_color_value, uint32_t width, uint32_t height,
    VkFormat format)
{
    VklRenderpass* renderpass = vkl_renderpass(gpu, width, height);

    VkClearValue clear_color = {0};
    clear_color.color = clear_color_value;

    VkClearValue clear_depth = {0};
    clear_depth.depthStencil.depth = 1.0f;

    vkl_renderpass_clear(renderpass, clear_color);
    vkl_renderpass_clear(renderpass, clear_depth);

    vkl_renderpass_attachment(
        renderpass, 0, //
        VKL_RENDERPASS_ATTACHMENT_COLOR, format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkl_renderpass_attachment_layout(
        renderpass, 0, //
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkl_renderpass_attachment_ops(
        renderpass, 0, //
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

    vkl_renderpass_subpass_attachment(renderpass, 0, 0);

    vkl_renderpass_subpass_dependency(renderpass, 0, VK_SUBPASS_EXTERNAL, 0);
    vkl_renderpass_subpass_dependency_stage(
        renderpass, 0, //
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    vkl_renderpass_subpass_dependency_access(
        renderpass, 0, 0,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    return renderpass;
}



static void make_renderpass_offscreen(VklRenderpass* renderpass)
{
    // Framebuffer images.
    VklImages* images = vkl_images(renderpass->gpu, VK_IMAGE_TYPE_2D, 1);
    // first attachment is color attachment
    vkl_images_format(images, renderpass->attachments[0].format);
    vkl_images_size(images, renderpass->width, renderpass->height, 1);
    vkl_images_tiling(images, VK_IMAGE_TILING_OPTIMAL);
    vkl_images_usage(
        images, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    vkl_images_memory(images, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkl_images_layout(images, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkl_images_queue_access(images, 0);
    vkl_images_create(images);

    // Create renderpass.
    vkl_renderpass_framebuffers(renderpass, 0, images);
    vkl_renderpass_create(renderpass);
}



static VklRenderpass* offscreen_renderpass(VklGpu* gpu)
{
    VklRenderpass* renderpass =
        default_renderpass(gpu, bgcolor, TEST_WIDTH, TEST_HEIGHT, TEST_FORMAT);
    make_renderpass_offscreen(renderpass);
    return renderpass;
}



static uint8_t* screenshot(VklImages* images)
{
    // NOTE: the caller must free the output

    VklGpu* gpu = images->gpu;

    // Create the staging image.
    log_debug("starting creation of staging image");
    VklImages* staging = vkl_images(gpu, VK_IMAGE_TYPE_2D, 1);
    vkl_images_format(staging, images->format);
    vkl_images_size(staging, images->width, images->height, images->depth);
    vkl_images_tiling(staging, VK_IMAGE_TILING_LINEAR);
    vkl_images_usage(staging, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    vkl_images_layout(staging, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkl_images_memory(
        staging, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkl_images_create(staging);

    // Start the image transition command buffers.
    VklCommands* cmds = vkl_commands(gpu, 0, 1);
    vkl_cmd_begin(cmds);

    VklBarrier barrier = vkl_barrier(gpu);
    vkl_barrier_stages(&barrier, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    vkl_barrier_images(&barrier, staging);
    vkl_barrier_images_layout(
        &barrier, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkl_barrier_images_access(&barrier, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    vkl_cmd_barrier(cmds, &barrier);

    // Copy the image to the staging image.
    vkl_cmd_copy_image(cmds, images, staging);

    vkl_barrier_images_layout(
        &barrier, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    vkl_barrier_images_access(&barrier, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT);
    vkl_cmd_barrier(cmds, &barrier);

    // End the commands and submit them.
    vkl_cmd_end(cmds);
    vkl_cmd_submit_sync(cmds, 0);

    // Now, copy the staging image into CPU memory.
    uint8_t* rgba = calloc(images->width * images->height, 3);
    vkl_images_download(staging, 0, true, rgba);

    return rgba;
}



static void empty_commands(VklCommands* commands, VklRenderpass* renderpass)
{
    vkl_cmd_begin(commands);
    vkl_cmd_begin_renderpass(commands, renderpass);
    vkl_cmd_end_renderpass(commands);
    vkl_cmd_end(commands);
}



typedef struct
{
    vec3 pos;
    vec4 color;
} VklVertex;



/*************************************************************************************************/
/*  Unit tests                                                                                   */
/*************************************************************************************************/

static int vklite2_app(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    ASSERT(app->obj.status == VKL_OBJECT_STATUS_CREATED);
    ASSERT(app->gpu_count >= 1);
    ASSERT(app->gpus[0].name != NULL);
    ASSERT(app->gpus[0].obj.status == VKL_OBJECT_STATUS_INIT);

    VklGpu* gpu = vkl_gpu(app, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_TRANSFER, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_GRAPHICS | VKL_QUEUE_COMPUTE, 1);
    vkl_gpu_queue(gpu, VKL_QUEUE_COMPUTE, 2);
    vkl_gpu_create(gpu, 0);

    TEST_END
}

static int vklite2_surface(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    VklGpu* gpu = vkl_gpu(app, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_ALL, 0);

    // Create a GLFW window and surface.
    VkSurfaceKHR surface = 0;
    GLFWwindow* window = (GLFWwindow*)backend_window(
        app->instance, VKL_BACKEND_GLFW, 100, 100, true, NULL, &surface);
    vkl_gpu_create(gpu, surface);

    backend_window_destroy(app->instance, VKL_BACKEND_GLFW, window, surface);

    TEST_END
}

static int vklite2_window(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    VklWindow* window = vkl_window(app, 100, 100);
    ASSERT(window != NULL);

    TEST_END
}

static int vklite2_swapchain(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    VklWindow* window = vkl_window(app, 100, 100);
    VklGpu* gpu = vkl_gpu(app, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_RENDER, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_PRESENT, 1);
    vkl_gpu_create(gpu, window->surface);
    VklSwapchain* swapchain = vkl_swapchain(gpu, window, 3);
    vkl_swapchain_format(swapchain, VK_FORMAT_B8G8R8A8_UNORM);
    vkl_swapchain_present_mode(swapchain, VK_PRESENT_MODE_FIFO_KHR);
    vkl_swapchain_create(swapchain);
    vkl_swapchain_destroy(swapchain);
    vkl_window_destroy(window);

    TEST_END
}

static int vklite2_commands(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    VklGpu* gpu = vkl_gpu(app, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_RENDER, 0);
    vkl_gpu_create(gpu, 0);
    VklCommands* commands = vkl_commands(gpu, 0, 3);
    vkl_cmd_begin(commands);
    vkl_cmd_end(commands);
    vkl_cmd_reset(commands);
    vkl_cmd_free(commands);

    TEST_END
}

static int vklite2_buffer(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    VklGpu* gpu = vkl_gpu(app, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_RENDER, 0);
    vkl_gpu_create(gpu, 0);

    VklBuffer* buffer = vkl_buffer(gpu);
    const VkDeviceSize size = 256;
    vkl_buffer_size(buffer, size, 0);
    vkl_buffer_usage(buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    vkl_buffer_memory(
        buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkl_buffer_queue_access(buffer, 0);
    vkl_buffer_create(buffer);

    // Send some data to the GPU.
    uint8_t* data = calloc(size, 1);
    for (uint32_t i = 0; i < size; i++)
        data[i] = i;
    vkl_buffer_upload(buffer, 0, size, data);

    // Recover the data.
    void* data2 = calloc(size, 1);
    vkl_buffer_download(buffer, 0, size, data2);

    // Check that the data downloaded from the GPU is the same.
    ASSERT(memcmp(data2, data, size) == 0);

    FREE(data);
    FREE(data2);

    TEST_END
}

static int vklite2_compute(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    VklGpu* gpu = vkl_gpu(app, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_COMPUTE, 0);
    vkl_gpu_create(gpu, 0);

    // Create the compute pipeline.
    char path[1024];
    snprintf(path, sizeof(path), "%s/spirv/pow2.comp.spv", DATA_DIR);
    VklCompute* compute = vkl_compute(gpu, path);

    // Create the buffers
    VklBuffer* buffer = vkl_buffer(gpu);
    const uint32_t n = 20;
    const VkDeviceSize size = n * sizeof(float);
    vkl_buffer_size(buffer, size, 0);
    vkl_buffer_usage(
        buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    vkl_buffer_memory(
        buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkl_buffer_queue_access(buffer, 0);
    vkl_buffer_create(buffer);

    // Send some data to the GPU.
    float* data = calloc(n, sizeof(float));
    for (uint32_t i = 0; i < n; i++)
        data[i] = (float)i;
    vkl_buffer_upload(buffer, 0, size, data);

    // Create the bindings.
    VklBindings* bindings = vkl_bindings(gpu);
    vkl_bindings_slot(bindings, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
    vkl_bindings_create(bindings, 1);
    VklBufferRegions br = {.buffer = buffer, .size = size, .count = 1};
    vkl_bindings_buffer(bindings, 0, &br);

    vkl_bindings_update(bindings);

    // Link the bindings to the compute pipeline and create it.
    vkl_compute_bindings(compute, bindings);
    vkl_compute_create(compute);

    // Command buffers.
    VklCommands* cmds = vkl_commands(gpu, 0, 1);
    vkl_cmd_begin(cmds);
    vkl_cmd_compute(cmds, compute, (uvec3){20, 1, 1});
    vkl_cmd_end(cmds);
    vkl_cmd_submit_sync(cmds, 0);

    // Get back the data.
    float* data2 = calloc(n, sizeof(float));
    vkl_buffer_download(buffer, 0, size, data2);
    for (uint32_t i = 0; i < n; i++)
        ASSERT(data2[i] == 2 * data[i]);

    TEST_END
}

static int vklite2_images(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    VklGpu* gpu = vkl_gpu(app, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_RENDER, 0);
    vkl_gpu_create(gpu, 0);

    VklImages* images = vkl_images(gpu, VK_IMAGE_TYPE_2D, 1);
    vkl_images_format(images, VK_FORMAT_R8G8B8A8_UINT);
    vkl_images_size(images, 16, 16, 1);
    vkl_images_tiling(images, VK_IMAGE_TILING_OPTIMAL);
    vkl_images_usage(images, VK_IMAGE_USAGE_STORAGE_BIT);
    vkl_images_memory(images, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkl_images_queue_access(images, 0);
    vkl_images_create(images);

    TEST_END
}

static int vklite2_sampler(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    VklGpu* gpu = vkl_gpu(app, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_RENDER, 0);
    vkl_gpu_create(gpu, 0);

    VklSampler* sampler = vkl_sampler(gpu);
    vkl_sampler_min_filter(sampler, VK_FILTER_LINEAR);
    vkl_sampler_mag_filter(sampler, VK_FILTER_LINEAR);
    vkl_sampler_address_mode(sampler, VKL_TEXTURE_AXIS_U, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    vkl_sampler_create(sampler);

    TEST_END
}

static int vklite2_barrier(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    VklGpu* gpu = vkl_gpu(app, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_RENDER, 0);
    vkl_gpu_create(gpu, 0);

    // Image.
    const uint32_t img_size = 16;
    VklImages* images = vkl_images(gpu, VK_IMAGE_TYPE_2D, 1);
    vkl_images_format(images, VK_FORMAT_R8G8B8A8_UINT);
    vkl_images_size(images, img_size, img_size, 1);
    vkl_images_tiling(images, VK_IMAGE_TILING_OPTIMAL);
    vkl_images_usage(images, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    vkl_images_memory(images, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkl_images_queue_access(images, 0);
    vkl_images_create(images);

    // Staging buffer.
    VklBuffer* buffer = vkl_buffer(gpu);
    const VkDeviceSize size = img_size * img_size * 4;
    vkl_buffer_size(buffer, size, 0);
    vkl_buffer_usage(buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    vkl_buffer_memory(
        buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkl_buffer_queue_access(buffer, 0);
    vkl_buffer_create(buffer);

    // Send some data to the staging buffer.
    uint8_t* data = calloc(size, 1);
    for (uint32_t i = 0; i < size; i++)
        data[i] = i % 256;
    vkl_buffer_upload(buffer, 0, size, data);

    // Image transition.
    VklBarrier barrier = vkl_barrier(gpu);
    vkl_barrier_stages(&barrier, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    vkl_barrier_images(&barrier, images);
    vkl_barrier_images_layout(
        &barrier, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Transfer the data from the staging buffer to the image.
    VklCommands* cmds = vkl_commands(gpu, 0, 1);
    vkl_cmd_begin(cmds);
    vkl_cmd_barrier(cmds, &barrier);
    vkl_cmd_copy_buffer_to_image(cmds, buffer, images);
    vkl_cmd_end(cmds);
    vkl_cmd_submit_sync(cmds, 0);

    TEST_END
}

static int vklite2_submit(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    VklGpu* gpu = vkl_gpu(app, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_COMPUTE, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_COMPUTE, 1);
    vkl_gpu_create(gpu, 0);

    // Create the compute pipeline.
    char path[1024];
    snprintf(path, sizeof(path), "%s/spirv/pow2.comp.spv", DATA_DIR);
    VklCompute* compute1 = vkl_compute(gpu, path);

    snprintf(path, sizeof(path), "%s/spirv/sum.comp.spv", DATA_DIR);
    VklCompute* compute2 = vkl_compute(gpu, path);

    // Create the buffer
    VklBuffer* buffer = vkl_buffer(gpu);
    const uint32_t n = 20;
    const VkDeviceSize size = n * sizeof(float);
    vkl_buffer_size(buffer, size, 0);
    vkl_buffer_usage(
        buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    vkl_buffer_memory(
        buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkl_buffer_queue_access(buffer, 0);
    vkl_buffer_queue_access(buffer, 1);
    vkl_buffer_create(buffer);

    // Send some data to the GPU.
    float* data = calloc(n, sizeof(float));
    for (uint32_t i = 0; i < n; i++)
        data[i] = (float)i;
    vkl_buffer_upload(buffer, 0, size, data);

    // Create the bindings.
    VklBindings* bindings1 = vkl_bindings(gpu);
    vkl_bindings_slot(bindings1, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
    vkl_bindings_create(bindings1, 1);
    VklBufferRegions br1 = {.buffer = buffer, .size = size, .count = 1};
    vkl_bindings_buffer(bindings1, 0, &br1);

    VklBindings* bindings2 = vkl_bindings(gpu);
    vkl_bindings_slot(bindings2, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0);
    vkl_bindings_create(bindings2, 1);
    VklBufferRegions br2 = {.buffer = buffer, .size = size, .count = 1};
    vkl_bindings_buffer(bindings2, 0, &br2);

    vkl_bindings_update(bindings1);
    vkl_bindings_update(bindings2);

    // Link the bindings1 to the compute1 pipeline and create it.
    vkl_compute_bindings(compute1, bindings1);
    vkl_compute_create(compute1);

    // Link the bindings1 to the compute2 pipeline and create it.
    vkl_compute_bindings(compute2, bindings2);
    vkl_compute_create(compute2);

    // Command buffers.
    VklCommands* cmds1 = vkl_commands(gpu, 0, 1);
    vkl_cmd_begin(cmds1);
    vkl_cmd_compute(cmds1, compute1, (uvec3){20, 1, 1});
    vkl_cmd_end(cmds1);

    VklCommands* cmds2 = vkl_commands(gpu, 0, 1);
    vkl_cmd_begin(cmds2);
    vkl_cmd_compute(cmds2, compute2, (uvec3){20, 1, 1});
    vkl_cmd_end(cmds2);

    // Semaphores
    VklSemaphores* semaphores = vkl_semaphores(gpu, 1);

    // Submit.
    VklSubmit submit1 = vkl_submit(gpu);
    vkl_submit_commands(&submit1, cmds1, 0);
    vkl_submit_signal_semaphores(&submit1, semaphores, 0);
    vkl_submit_send(&submit1, 0, NULL, 0);

    VklSubmit submit2 = vkl_submit(gpu);
    vkl_submit_commands(&submit2, cmds2, 0);
    vkl_submit_wait_semaphores(&submit2, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, semaphores, 0);
    vkl_submit_send(&submit2, 0, NULL, 0);

    vkl_gpu_wait(gpu);

    // Get back the data.
    float* data2 = calloc(n, sizeof(float));
    vkl_buffer_download(buffer, 0, size, data2);
    for (uint32_t i = 0; i < n; i++)
        ASSERT(data2[i] == 2 * i + 1);

    TEST_END
}

static int vklite2_renderpass(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    VklGpu* gpu = vkl_gpu(app, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_RENDER, 0);
    vkl_gpu_create(gpu, 0);

    VklRenderpass* renderpass = offscreen_renderpass(gpu);
    ASSERT(renderpass != NULL);
    ASSERT(renderpass->obj.status == VKL_OBJECT_STATUS_CREATED);

    TEST_END
}

static int vklite2_blank(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    VklGpu* gpu = vkl_gpu(app, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_RENDER, 0);
    vkl_gpu_create(gpu, 0);

    VklRenderpass* renderpass = offscreen_renderpass(gpu);
    ASSERT(renderpass != NULL);
    ASSERT(renderpass->obj.status == VKL_OBJECT_STATUS_CREATED);

    VklCommands* commands = vkl_commands(gpu, 0, 1);
    empty_commands(commands, renderpass);
    vkl_cmd_submit_sync(commands, 0);

    uint8_t* rgba = screenshot(renderpass->framebuffer_info[0].images);

    for (uint32_t i = 0; i < TEST_WIDTH * TEST_HEIGHT * 3; i++)
        ASSERT(rgba[i] >= 100);

    FREE(rgba);
    TEST_END
}

static int vklite2_graphics(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    VklGpu* gpu = vkl_gpu(app, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_RENDER, 0);
    vkl_gpu_create(gpu, 0);

    VklRenderpass* renderpass = offscreen_renderpass(gpu);
    ASSERT(renderpass != NULL);
    ASSERT(renderpass->obj.status == VKL_OBJECT_STATUS_CREATED);

    VklGraphics* graphics = vkl_graphics(gpu);
    ASSERT(graphics != NULL);

    vkl_graphics_renderpass(graphics, renderpass, 0);
    vkl_graphics_topology(graphics, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    vkl_graphics_polygon_mode(graphics, VK_POLYGON_MODE_FILL);

    char path[1024];
    snprintf(path, sizeof(path), "%s/spirv/default.vert.spv", DATA_DIR);
    vkl_graphics_shader(graphics, VK_SHADER_STAGE_VERTEX_BIT, path);
    snprintf(path, sizeof(path), "%s/spirv/default.frag.spv", DATA_DIR);
    vkl_graphics_shader(graphics, VK_SHADER_STAGE_FRAGMENT_BIT, path);
    vkl_graphics_vertex_binding(graphics, 0, sizeof(VklVertex));
    vkl_graphics_vertex_attr(graphics, 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VklVertex, pos));
    vkl_graphics_vertex_attr(
        graphics, 0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VklVertex, color));

    // Create the bindings.
    VklBindings* bindings = vkl_bindings(gpu);
    vkl_bindings_create(bindings, 1);
    vkl_bindings_update(bindings);
    vkl_graphics_bindings(graphics, bindings);

    // Create the graphics pipeline.
    vkl_graphics_create(graphics);

    // Create the buffer.
    VklBuffer* buffer = vkl_buffer(gpu);
    VkDeviceSize size = 3 * sizeof(VklVertex);
    vkl_buffer_size(buffer, size, 0);
    vkl_buffer_usage(buffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    vkl_buffer_memory(
        buffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkl_buffer_create(buffer);

    // Upload the triangle data.
    VklVertex data[3] = {
        {{-1, +1, 0}, {1, 0, 0, 1}},
        {{+1, +1, 0}, {0, 1, 0, 1}},
        {{+0, -1, 0}, {0, 0, 1, 1}},
    };
    vkl_buffer_upload(buffer, 0, size, data);

    VklBufferRegions br = {0};
    br.buffer = buffer;
    br.size = size;
    br.count = 1;

    // Commands.
    VklCommands* commands = vkl_commands(gpu, 0, 1);
    vkl_cmd_begin(commands);
    vkl_cmd_begin_renderpass(commands, renderpass);
    vkl_cmd_viewport(commands, (VkViewport){0, 0, TEST_WIDTH, TEST_HEIGHT, 0, 1});
    vkl_cmd_bind_vertex_buffer(commands, &br, 0);
    vkl_cmd_bind_graphics(commands, graphics, 0);
    vkl_cmd_draw(commands, 0, 3);
    vkl_cmd_end_renderpass(commands);
    vkl_cmd_end(commands);
    vkl_cmd_submit_sync(commands, 0);

    uint8_t* rgba = screenshot(renderpass->framebuffer_info[0].images);

    write_ppm("a.ppm", TEST_WIDTH, TEST_HEIGHT, rgba);

    FREE(rgba);
    TEST_END
}

static int vklite2_canvas_basic(VkyTestContext* context)
{
    VklApp* app = vkl_app(VKL_BACKEND_GLFW);
    VklWindow* window = vkl_window(app, TEST_WIDTH, TEST_HEIGHT);
    VklGpu* gpu = vkl_gpu(app, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_RENDER, 0);
    vkl_gpu_queue(gpu, VKL_QUEUE_PRESENT, 1);
    vkl_gpu_create(gpu, window->surface);

    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;

    uint32_t framebuffer_width, framebuffer_height;
    vkl_window_get_size(window, &framebuffer_width, &framebuffer_height);
    ASSERT(framebuffer_width > 0);
    ASSERT(framebuffer_height > 0);

    VklRenderpass* renderpass = vkl_renderpass(gpu, framebuffer_width, framebuffer_height);

    VkClearValue clear_depth = {0};
    clear_depth.depthStencil.depth = 1.0f;

    vkl_renderpass_clear(renderpass, (VkClearValue){.color = bgcolor});
    vkl_renderpass_clear(renderpass, clear_depth);

    vkl_renderpass_attachment(
        renderpass, 0, //
        VKL_RENDERPASS_ATTACHMENT_COLOR, format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkl_renderpass_attachment_layout(
        renderpass, 0, //
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    vkl_renderpass_attachment_ops(
        renderpass, 0, //
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE);

    vkl_renderpass_subpass_attachment(renderpass, 0, 0);

    vkl_renderpass_subpass_dependency(renderpass, 0, VK_SUBPASS_EXTERNAL, 0);
    vkl_renderpass_subpass_dependency_stage(
        renderpass, 0, //
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    vkl_renderpass_subpass_dependency_access(
        renderpass, 0, 0,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

    VklSwapchain* swapchain = vkl_swapchain(gpu, window, 3);
    vkl_swapchain_format(swapchain, VK_FORMAT_B8G8R8A8_UNORM);
    vkl_swapchain_present_mode(swapchain, VK_PRESENT_MODE_FIFO_KHR);
    vkl_swapchain_create(swapchain);

    // Create renderpass.
    vkl_renderpass_framebuffers(renderpass, 0, swapchain->images);
    vkl_renderpass_create(renderpass);

    VklCommands* commands = vkl_commands(gpu, 0, swapchain->img_count);
    empty_commands(commands, renderpass);



    // Sync objects.
    VklSemaphores* sem_img_available = vkl_semaphores(gpu, VKY_MAX_FRAMES_IN_FLIGHT);
    VklSemaphores* sem_render_finished = vkl_semaphores(gpu, VKY_MAX_FRAMES_IN_FLIGHT);
    VklFences* fences = vkl_fences(gpu, VKY_MAX_FRAMES_IN_FLIGHT);
    VklFences bak_fences = {0};
    bak_fences.count = swapchain->img_count;
    bak_fences.gpu = gpu;
    bak_fences.obj = fences->obj;
    uint32_t cur_frame = 0;
    VklBackend backend = VKL_BACKEND_GLFW;


    for (uint32_t frame = 0; frame < 5 * 60; frame++)
    {
        log_info("iteration %d", frame);

        glfwPollEvents();

        if (backend_window_show_close(backend, window->backend_window) ||
            window->obj.status == VKL_OBJECT_STATUS_NEED_DESTROY)
            break;

        // Wait for fence.
        vkl_fences_wait(fences, cur_frame);

        // We acquire the next swapchain image.
        vkl_swapchain_acquire(swapchain, sem_img_available, cur_frame, NULL, 0);
        if (swapchain->obj.status != VKL_OBJECT_STATUS_NEED_RECREATE)
        {
            // Wait for previous fence if needed.
            if (bak_fences.fences[swapchain->img_idx] != 0)
                vkl_fences_wait(&bak_fences, swapchain->img_idx);
            bak_fences.fences[swapchain->img_idx] = fences->fences[cur_frame];

            // Then, we submit the commands on that image
            VklSubmit submit = vkl_submit(gpu);
            vkl_submit_commands(&submit, commands, (int32_t)swapchain->img_idx);
            vkl_submit_wait_semaphores(
                &submit, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, sem_img_available,
                (int32_t)cur_frame);
            // Once the render is finished, we signal another semaphore.
            vkl_submit_signal_semaphores(&submit, sem_render_finished, (int32_t)cur_frame);
            vkl_submit_send(&submit, 0, fences, cur_frame);

            // Once the image is rendered, we present the swapchain image.
            vkl_swapchain_present(swapchain, 1, sem_render_finished, cur_frame);

            cur_frame = (cur_frame + 1) % VKY_MAX_FRAMES_IN_FLIGHT;
        }

        // Handle resizing.
        else
        {
            log_trace("recreating the swapchain");

            // Wait until the device is ready and the window fully resized.
            backend_window_get_size(
                backend, window->backend_window, //
                &window->width, &window->height, //
                &renderpass->width, &renderpass->height);
            vkl_gpu_wait(gpu);

            // Recreate the framebuffers and swapchain.
            vkl_renderpass_framebuffers_destroy(renderpass);
            vkl_swapchain_destroy(swapchain);

            vkl_swapchain_create(swapchain);
            vkl_renderpass_framebuffers(renderpass, 0, swapchain->images);
            vkl_renderpass_framebuffers_create(renderpass);

            // Need to refill the command buffers.
            vkl_cmd_reset(commands);
            empty_commands(commands, renderpass);
        }
    }
    vkl_gpu_wait(gpu);
    vkl_swapchain_destroy(swapchain);
    vkl_window_destroy(window);
    TEST_END
}



static int vklite2_test_compute_only(VkyTestContext* context)
{
    // VkyApp* app = vky_app();
    // VkyCompute* compute = vky_compute(app->gpu, "compute.spv");
    // VkyBuffer* buffer =
    // VkyCommands* commands = vky_commands(app->gpu, VKY_COMMAND_COMPUTE);
    // vky_cmd_begin(commands);
    // vky_cmd_compute(commands, compute, uvec3 size);
    // vky_cmd_end(commands);
    // VkySubmit* sub = vky_submit(app-> gpu, VKY_QUEUE_COMPUTE);
    // vky_submit_send(sub, NULL);
    // vky_app_destroy(app);
    return 0;
}

static int vklite2_test_offscreen(VkyTestContext* context) { return 0; }

static int vklite2_test_offscreen_gui(VkyTestContext* context) { return 0; }

static int vklite2_test_offscreen_compute(VkyTestContext* context) { return 0; }
