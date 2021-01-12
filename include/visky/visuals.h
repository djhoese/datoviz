#ifndef VKL_VISUALS_HEADER
#define VKL_VISUALS_HEADER

#include "array.h"
#include "context.h"
#include "graphics.h"
#include "vklite.h"



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define VKL_MAX_GRAPHICS_PER_VISUAL 16
#define VKL_MAX_COMPUTES_PER_VISUAL 32
#define VKL_MAX_VISUAL_GROUPS       1024
// #define VKL_MAX_VISUAL_SOURCES      64
// #define VKL_MAX_VISUAL_RESOURCES    64
// #define VKL_MAX_VISUAL_PROPS        64
#define VKL_MAX_VISUAL_PRIORITY 4



/*************************************************************************************************/
/*  Enums                                                                                        */
/*************************************************************************************************/

// Pipeline types
typedef enum
{
    VKL_PIPELINE_GRAPHICS,
    VKL_PIPELINE_COMPUTE,
} VklPipelineType;



// Prop types.
typedef enum
{
    VKL_PROP_NONE,
    VKL_PROP_POS,
    VKL_PROP_COLOR,
    VKL_PROP_MARKER_SIZE,
    VKL_PROP_TEXT,
    VKL_PROP_TEXT_SIZE,
    VKL_PROP_LINE_WIDTH,
    VKL_PROP_TYPE,
    VKL_PROP_LENGTH,
    VKL_PROP_MARGIN,
    VKL_PROP_NORMAL,
    VKL_PROP_TEXCOORDS,
    VKL_PROP_TEXCOEFS,
    VKL_PROP_IMAGE,
    VKL_PROP_COLOR_TEXTURE,
    VKL_PROP_LIGHT_POS,
    VKL_PROP_LIGHT_PARAMS,
    VKL_PROP_VIEW_POS,
    VKL_PROP_MODEL,
    VKL_PROP_VIEW,
    VKL_PROP_PROJ,
    VKL_PROP_TIME,
    VKL_PROP_INDEX,
} VklPropType;



// Source kinds.
typedef enum
{
    VKL_SOURCE_KIND_NONE,
    VKL_SOURCE_KIND_VERTEX = 0x0010,
    VKL_SOURCE_KIND_INDEX = 0x0020,
    VKL_SOURCE_KIND_UNIFORM = 0x0030,
    VKL_SOURCE_KIND_STORAGE = 0x0040,
    VKL_SOURCE_KIND_TEXTURE_1D = 0x0050,
    VKL_SOURCE_KIND_TEXTURE_2D = 0x0060,
    VKL_SOURCE_KIND_TEXTURE_3D = 0x0070,
} VklSourceKind;



// Source types.
// NOTE: only 1 source type per pipeline is supported
typedef enum
{
    VKL_SOURCE_TYPE_NONE,
    VKL_SOURCE_TYPE_MVP,      // 1
    VKL_SOURCE_TYPE_VIEWPORT, // 2
    VKL_SOURCE_TYPE_PARAM,    // 3
    VKL_SOURCE_TYPE_VERTEX,   // 4
    VKL_SOURCE_TYPE_INDEX,    // 5
    VKL_SOURCE_TYPE_IMAGE,
    // HACK: should support multiple source types per pipeline instead
    // WARNING: VKL_SOURCE_TYPE_IMAGE must be immediately followed by VKL_SOURCE_TYPE_IMAGE_n in
    // the enumeration list
    VKL_SOURCE_TYPE_IMAGE_1,
    VKL_SOURCE_TYPE_IMAGE_2,
    VKL_SOURCE_TYPE_IMAGE_3,
    VKL_SOURCE_TYPE_IMAGE_4,
    VKL_SOURCE_TYPE_VOLUME,
    VKL_SOURCE_TYPE_COLOR_TEXTURE,
    VKL_SOURCE_TYPE_FONT_ATLAS,
    VKL_SOURCE_TYPE_OTHER,

    VKL_SOURCE_TYPE_COUNT,
} VklSourceType;



// Data source origin.
typedef enum
{
    VKL_SOURCE_ORIGIN_NONE,   // not set
    VKL_SOURCE_ORIGIN_LIB,    // the GPU buffer or texture is handled by visky's visual module
    VKL_SOURCE_ORIGIN_USER,   // the GPU buffer or texture is handled by the user
    VKL_SOURCE_ORIGIN_NOBAKE, // the GPU buffer or texture is handled by the library, but the user
                              // provides the baked data directly
} VklSourceOrigin;



// Source flags.
typedef enum
{
    VKL_SOURCE_FLAG_MAPPABLE = 0x0001,
} VklSourceFlags;



/*************************************************************************************************/
/*  Typedefs                                                                                     */
/*************************************************************************************************/

typedef struct VklVisual VklVisual;
typedef struct VklProp VklProp;

// typedef struct VklSourceBuffer VklSourceBuffer;
// typedef struct VklSourceTexture VklSourceTexture;
typedef union VklSourceUnion VklSourceUnion;
typedef struct VklSource VklSource;
typedef struct VklDataCoords VklDataCoords;

typedef struct VklVisualFillEvent VklVisualFillEvent;
typedef struct VklVisualDataEvent VklVisualDataEvent;

typedef uint32_t VklIndex;



/*************************************************************************************************/
/*  Callbacks                                                                                    */
/*************************************************************************************************/

typedef void (*VklVisualFillCallback)(VklVisual* visual, VklVisualFillEvent ev);
/*
called by the scene event callback in response to a REFILL event
default fill callback: viewport, bind vbuf, ibuf, etc. bind the first graphics only and no
compute...
*/



typedef void (*VklVisualDataCallback)(VklVisual* visual, VklVisualDataEvent ev);
/*
called by the scene event callback in response to a DATA event
baking process
visual data sources, item count, groups ==> bindings, vertex buffer, index buffer
enqueue data transfers
*/



/*************************************************************************************************/
/*  Source structs                                                                               */
/*************************************************************************************************/

struct VklDataCoords
{
    dvec4 data; // (blx, bly, trx, try)
    vec4 gpu;   // (blx, bly, trx, try)
};



union VklSourceUnion
{
    VklBufferRegions br;
    VklTexture* tex;
};



// Within a visual, a source is uniquely identified by (1) its type, (2) the pipeline_idx
// We assume there is no more than 1 source of a given type in a given pipeline.
struct VklSource
{
    VklObject obj;

    // Identifier of the prop
    VklPipelineType pipeline; // graphics or compute pipeline?
    uint32_t pipeline_idx;    // idx of the pipeline within the graphics or compute pipelines
    uint32_t other_count; // the same source may be shared by multiple pipelines of the same type,
                          // using the same slot_idx
    uint32_t other_idxs[VKL_MAX_GRAPHICS_PER_VISUAL];
    VklSourceType source_type; // Type of the source (MVP, viewport, vertex buffer, etc.)
    VklSourceKind source_kind; // Vertex, index, uniform, storage, or texture
    uint32_t slot_idx;         // Binding slot, or 0 for vertex/index
    int flags;
    VklArray arr; // array to be uploaded to that source

    VklSourceOrigin origin; // whether the underlying GPU object is handled by the user or visky
    VklSourceUnion u;
};



struct VklProp
{
    VklObject obj;

    VklPropType prop_type; // prop type
    uint32_t prop_idx;     // index within all props of that type

    VklSource* source;

    uint32_t field_idx;
    VklDataType dtype;
    VkDeviceSize offset;

    void* default_value;
    VklArray arr_orig;  // original data array
    VklArray arr_trans; // transformed data array
    // VklArray arr_triang; // triangulated data array

    VklArrayCopyType copy_type;
    uint32_t reps; // number of repeats when copying
    // bool is_set; // whether the user has set this prop
};



/*************************************************************************************************/
/*  Visual struct                                                                                */
/*************************************************************************************************/

struct VklVisual
{
    VklObject obj;
    VklCanvas* canvas;
    int flags;
    int priority;

    // Graphics.
    uint32_t graphics_count;
    VklGraphics* graphics[VKL_MAX_GRAPHICS_PER_VISUAL];

    // Keep track of the previous number of vertices/indices in each graphics pipeline, so that
    // we can automatically detect changes in vetex_count/index_count and trigger a full REFILL
    // in this case.
    uint32_t prev_vertex_count[VKL_MAX_GRAPHICS_PER_VISUAL];
    uint32_t prev_index_count[VKL_MAX_GRAPHICS_PER_VISUAL];

    // Computes.
    uint32_t compute_count;
    VklCompute* computes[VKL_MAX_COMPUTES_PER_VISUAL];

    // Fill callbacks.
    VklVisualFillCallback callback_fill;

    // Data callbacks.
    VklVisualDataCallback callback_transform;
    VklVisualDataCallback callback_bake;

    // Sources.
    VklContainer sources;

    // Props.
    VklContainer props;

    // User data
    uint32_t group_count;
    uint32_t group_sizes[VKL_MAX_VISUAL_GROUPS];

    // Viewport.
    VklTransformAxis transform[VKL_MAX_GRAPHICS_PER_VISUAL];
    VklViewportClip clip[VKL_MAX_GRAPHICS_PER_VISUAL];
    VklViewport viewport; // usually the visual's panel viewport, but may be customized

    // GPU data
    VklContainer bindings;
    VklContainer bindings_comp;
};



/*************************************************************************************************/
/*  Event structs                                                                                */
/*************************************************************************************************/

// passed to visual callback when it needs to refill the command buffers
struct VklVisualFillEvent
{
    VklCommands* cmds;
    uint32_t cmd_idx;
    VkClearColorValue clear_color;
    VklViewport viewport;
    void* user_data;
};



struct VklVisualDataEvent
{
    VklViewport viewport;
    VklDataCoords coords;
    const void* user_data;
};



/*************************************************************************************************/
/*  Visual creation                                                                              */
/*************************************************************************************************/

VKY_EXPORT VklVisual vkl_visual(VklCanvas* canvas);

VKY_EXPORT void vkl_visual_destroy(VklVisual* visual);



// Define a new source. (source_type, pipeline_idx) completely identifies a source within all
// pipelines
VKY_EXPORT void vkl_visual_source(
    VklVisual* visual, VklSourceType type, VklPipelineType pipeline, uint32_t pipeline_idx,
    uint32_t slot_idx, VkDeviceSize item_size, int flags);

VKY_EXPORT void vkl_visual_source_share(
    VklVisual* visual, VklSourceType source_type, uint32_t pipeline_idx, uint32_t other_idx);

VKY_EXPORT void vkl_visual_prop(
    VklVisual* visual, VklPropType prop_type, uint32_t prop_idx, VklDataType dtype,
    VklSourceType source_type, uint32_t pipeline_idx);

VKY_EXPORT void vkl_visual_prop_default(
    VklVisual* visual, VklPropType prop_type, uint32_t prop_idx, void* default_value);

VKY_EXPORT void vkl_visual_prop_copy(
    VklVisual* visual, VklPropType prop_type, uint32_t prop_idx, //
    uint32_t field_idx, VkDeviceSize offset,                     //
    VklArrayCopyType copy_type, uint32_t reps);

VKY_EXPORT void vkl_visual_graphics(VklVisual* visual, VklGraphics* graphics);

VKY_EXPORT void vkl_visual_compute(VklVisual* visual, VklCompute* compute);



/*************************************************************************************************/
/*  User-facing functions                                                                        */
/*************************************************************************************************/

VKY_EXPORT void vkl_visual_group(VklVisual* visual, uint32_t group_idx, uint32_t size);

VKY_EXPORT void vkl_visual_data(
    VklVisual* visual, VklPropType type, uint32_t prop_idx, uint32_t count, const void* data);

VKY_EXPORT void vkl_visual_data_partial(
    VklVisual* visual, VklPropType prop_type, uint32_t prop_idx, //
    uint32_t first_item, uint32_t item_count, uint32_t data_item_count, const void* data);

VKY_EXPORT void vkl_visual_data_append(
    VklVisual* visual, VklPropType prop_type, uint32_t prop_idx, uint32_t count, const void* data);

VKY_EXPORT void vkl_visual_data_full(
    VklVisual* visual, VklSourceType source_type, uint32_t idx, //
    uint32_t first_item, uint32_t item_count, uint32_t data_item_count, const void* data);

// VKY_EXPORT void vkl_visual_data_texture(
//     VklVisual* visual, VklPropType type, uint32_t prop_idx, //
//     uint32_t width, uint32_t height, uint32_t depth, const void* data);

VKY_EXPORT void
vkl_visual_buffer(VklVisual* visual, VklSourceType source_type, uint32_t idx, VklBufferRegions br);

VKY_EXPORT void vkl_visual_texture(
    VklVisual* visual, VklSourceType source_type, uint32_t idx, VklTexture* texture);



/*************************************************************************************************/
/*  Visual events                                                                                */
/*************************************************************************************************/

VKY_EXPORT void vkl_visual_fill_callback(VklVisual* visual, VklVisualFillCallback callback);

VKY_EXPORT void vkl_visual_fill_event(
    VklVisual* visual, VkClearColorValue clear_color, VklCommands* cmds, uint32_t cmd_idx,
    VklViewport viewport, void* user_data);

VKY_EXPORT void vkl_visual_fill_begin(VklCanvas* canvas, VklCommands* cmds, uint32_t idx);

VKY_EXPORT void vkl_visual_fill_end(VklCanvas* canvas, VklCommands* cmds, uint32_t idx);

VKY_EXPORT void vkl_visual_callback_transform(VklVisual* visual, VklVisualDataCallback callback);

VKY_EXPORT void vkl_visual_callback_bake(VklVisual* visual, VklVisualDataCallback callback);



/*************************************************************************************************/
/*  Baking helpers                                                                               */
/*************************************************************************************************/

VKY_EXPORT VklSource*
vkl_bake_source(VklVisual* visual, VklSourceType source_type, uint32_t pipeline_idx);

VKY_EXPORT VklProp* vkl_bake_prop(VklVisual* visual, VklPropType prop_type, uint32_t idx);

VKY_EXPORT void* vkl_bake_prop_item(VklProp* prop, uint32_t idx);

VKY_EXPORT uint32_t vkl_bake_max_prop_size(VklVisual* visual, VklSource* source);

VKY_EXPORT void vkl_bake_prop_copy(VklVisual* visual, VklProp* prop);

VKY_EXPORT void vkl_bake_source_alloc(VklVisual* visual, VklSource* source, uint32_t count);

VKY_EXPORT void vkl_bake_source_fill(VklVisual* visual, VklSource* source);

VKY_EXPORT void vkl_visual_buffer_alloc(VklVisual* visual, VklSource* source);

VKY_EXPORT void vkl_visual_texture_alloc(VklVisual* visual, VklSource* source);



/*************************************************************************************************/
/*  Data update                                                                                  */
/*************************************************************************************************/

VKY_EXPORT void vkl_visual_update(
    VklVisual* visual, VklViewport viewport, VklDataCoords coords, const void* user_data);



/*************************************************************************************************/
/*  Default callbacks                                                                            */
/*************************************************************************************************/

static void _default_visual_fill(VklVisual* visual, VklVisualFillEvent ev)
{
    ASSERT(visual != NULL);
    VklCanvas* canvas = visual->canvas;
    ASSERT(canvas != NULL);

    VklCommands* cmds = ev.cmds;
    uint32_t idx = ev.cmd_idx;
    VkViewport viewport = ev.viewport.viewport;

    ASSERT(viewport.width > 0);
    ASSERT(viewport.height > 0);

    // Draw all valid graphics pipelines.
    VklBindings* bindings = NULL;
    for (uint32_t pipeline_idx = 0; pipeline_idx < visual->graphics_count; pipeline_idx++)
    {
        ASSERT(is_obj_created(&visual->graphics[pipeline_idx]->obj));

        bindings = vkl_container_get(&visual->bindings, pipeline_idx);
        ASSERT(is_obj_created(&bindings->obj));

        VklSource* vertex_source = vkl_bake_source(visual, VKL_SOURCE_TYPE_VERTEX, pipeline_idx);
        ASSERT(vertex_source != NULL);
        uint32_t vertex_count = vertex_source->arr.item_count;
        if (vertex_count == 0)
        {
            log_warn("skip this graphics pipeline as the vertex buffer is empty");
            continue;
        }
        ASSERT(vertex_count > 0);

        // Bind the vertex buffer.
        VklBufferRegions* vertex_buf = &vertex_source->u.br;
        ASSERT(vertex_buf != NULL);
        vkl_cmd_bind_vertex_buffer(cmds, idx, *vertex_buf, 0);

        // Index buffer?
        VklSource* index_source = vkl_bake_source(visual, VKL_SOURCE_TYPE_INDEX, pipeline_idx);
        uint32_t index_count = 0;
        VklBufferRegions* index_buf = NULL;
        if (index_source != NULL)
        {
            index_count = index_source->arr.item_count;
            if (index_count > 0)
            {
                index_buf = &index_source->u.br;
                ASSERT(index_buf != NULL);
                vkl_cmd_bind_index_buffer(cmds, idx, *index_buf, 0);
            }
        }

        // Draw command.
        vkl_cmd_bind_graphics(cmds, idx, visual->graphics[pipeline_idx], bindings, 0);

        if (index_count == 0)
        {
            log_debug("draw %d vertices", vertex_count);
            // Make sure the bound vertex buffer is large enough.
            ASSERT(vertex_buf->size >= vertex_count * vertex_source->arr.item_size);
            vkl_cmd_draw(cmds, idx, 0, vertex_count);
        }
        else
        {
            log_debug("draw %d indices", index_count);
            // Make sure the bound index buffer is large enough.
            ASSERT(index_buf->size >= index_count * sizeof(VklIndex));
            vkl_cmd_draw_indexed(cmds, idx, 0, 0, index_count);
        }
    }
}



static void _bake_source(VklVisual* visual, VklSource* source)
{
    ASSERT(visual != NULL);
    if (source == NULL)
        return;

    // The baking function doesn't run if the VERTEX source is handled by the user.
    if (source->origin != VKL_SOURCE_ORIGIN_LIB)
        return;
    if (source->obj.status != VKL_OBJECT_STATUS_NEED_UPDATE)
    {
        log_trace(
            "skip bake source for source %d that doesn't need updating", source->source_kind);
        return;
    }
    log_debug("baking source %d", source->source_kind);

    // The number of vertices corresponds to the largest prop.
    uint32_t count = vkl_bake_max_prop_size(visual, source);
    if (count == 0)
    {
        log_warn("empty source %d", source->source_type);
        return;
    }

    // Allocate the source array.
    vkl_bake_source_alloc(visual, source, count);

    // Copy all corresponding props to the array.
    vkl_bake_source_fill(visual, source);
}



static void _bake_uniforms(VklVisual* visual)
{
    VklSource* source = vkl_container_iter_init(&visual->sources);
    // UNIFORM sources.

    while (source != NULL)
    {
        if (source->obj.status != VKL_OBJECT_STATUS_NEED_UPDATE)
        {
            log_trace("skip bake source for uniform source that doesn't need updating");
            source = vkl_container_iter(&visual->sources);
            continue;
        }

        // Allocate the UNIFORM sources, using the number of items in the props, and fill them
        // with the props.
        if (source->source_kind == VKL_SOURCE_KIND_UNIFORM &&
            source->origin == VKL_SOURCE_ORIGIN_LIB)
        {
            uint32_t count = vkl_bake_max_prop_size(visual, source);
            ASSERT(count > 0);
            vkl_bake_source_alloc(visual, source, count);
            vkl_bake_source_fill(visual, source);
        }
        source = vkl_container_iter(&visual->sources);
    }
}



static void _default_visual_bake(VklVisual* visual, VklVisualDataEvent ev)
{
    // The default baking function assumes all props have the same number of items, which
    // also corresponds to the number of vertices.

    ASSERT(visual != NULL);

    // VERTEX source.
    VklSource* source = vkl_bake_source(visual, VKL_SOURCE_TYPE_VERTEX, 0);
    _bake_source(visual, source);

    // INDEX source.
    source = vkl_bake_source(visual, VKL_SOURCE_TYPE_INDEX, 0);
    _bake_source(visual, source);
}



#endif
