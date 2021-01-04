#ifndef VKL_CANVAS_HEADER
#define VKL_CANVAS_HEADER

#include "../include/visky/context.h"
#include "keycode.h"
#include "vklite.h"

#ifdef __cplusplus
extern "C" {
#endif



/*************************************************************************************************/
/*  Constants                                                                                    */
/*************************************************************************************************/

#define VKL_MAX_EVENT_CALLBACKS 32
// Maximum acceptable duration for the pending events in the event queue, in seconds
#define VKL_MAX_EVENT_DURATION .5
#define VKL_DEFAULT_BACKGROUND                                                                    \
    (VkClearColorValue)                                                                           \
    {                                                                                             \
        {                                                                                         \
            0, .03, .07, 1.0f                                                                     \
        }                                                                                         \
    }
#define VKL_DEFAULT_IMAGE_FORMAT VK_FORMAT_B8G8R8A8_UNORM
// #define VKL_DEFAULT_PRESENT_MODE VK_PRESENT_MODE_FIFO_KHR
#define VKL_DEFAULT_DPI_SCALING 1.0f
#define VKL_DEFAULT_PRESENT_MODE                                                                  \
    (getenv("VKL_FPS") != NULL ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_FIFO_KHR)
#define VKL_MIN_SWAPCHAIN_IMAGE_COUNT 3
#define VKL_SEMAPHORE_IMG_AVAILABLE   0
#define VKL_SEMAPHORE_RENDER_FINISHED 1
#define VKL_FENCE_RENDER_FINISHED     0
#define VKL_FENCES_FLIGHT             1
#define VKL_DEFAULT_COMMANDS_TRANSFER 0
#define VKL_DEFAULT_COMMANDS_RENDER   1
#define VKL_MAX_FRAMES_IN_FLIGHT      2



/*************************************************************************************************/
/*  Enums                                                                                        */
/*************************************************************************************************/

// Canvas creation flags.
typedef enum
{
    VKL_CANVAS_FLAGS_NONE = 0x0000,
    VKL_CANVAS_FLAGS_IMGUI = 0x0001,
    VKL_CANVAS_FLAGS_FPS = 0x0003, // NOTE: 1 bit for ImGUI, 1 bit for FPS
} VklCanvasFlags;



/**
 * Private event types.
 *
 * Private events are emitted and consumed in the main thread, typically by the canvas itself.
 * They are rarely used by end-users.
 *
 */
// Private event types
typedef enum
{
    VKL_PRIVATE_EVENT_INIT,      // called before the first frame
    VKL_PRIVATE_EVENT_REFILL,    // called every time the command buffers need to be recreated
    VKL_PRIVATE_EVENT_INTERACT,  // called at every frame, before event enqueue
    VKL_PRIVATE_EVENT_FRAME,     // called at every frame, after event enqueue
    VKL_PRIVATE_EVENT_IMGUI,     // called at every frame, after event enqueue
    VKL_PRIVATE_EVENT_TIMER,     // called every X ms in the main thread, just after FRAME
    VKL_PRIVATE_EVENT_RESIZE,    // called at every resize
    VKL_PRIVATE_EVENT_PRE_SEND,  // called before sending the commands buffers
    VKL_PRIVATE_EVENT_POST_SEND, // called after sending the commands buffers
    VKL_PRIVATE_EVENT_DESTROY,   // called before destruction
} VklPrivateEventType;



// Canvas size type
typedef enum
{
    VKL_CANVAS_SIZE_SCREEN,
    VKL_CANVAS_SIZE_FRAMEBUFFER,
} VklCanvasSizeType;



// Viewport type.
// NOTE: must correspond to values in common.glsl
typedef enum
{
    VKL_VIEWPORT_FULL,
    VKL_VIEWPORT_INNER,
    VKL_VIEWPORT_OUTER,
    VKL_VIEWPORT_OUTER_BOTTOM,
    VKL_VIEWPORT_OUTER_LEFT,
} VklViewportClip;



// Transform axis
// NOTE: must correspond to values in common.glsl
typedef enum
{
    VKL_TRANSFORM_AXIS_DEFAULT,
    VKL_TRANSFORM_AXIS_ALL,  // normal transform
    VKL_TRANSFORM_AXIS_X,    // transform just on the X axis
    VKL_TRANSFORM_AXIS_Y,    // transform just on the Y axis
    VKL_TRANSFORM_AXIS_NONE, // no transform
} VklTransformAxis;



// Mouse state type
typedef enum
{
    VKL_MOUSE_STATE_INACTIVE,
    VKL_MOUSE_STATE_DRAG,
    VKL_MOUSE_STATE_WHEEL,
    VKL_MOUSE_STATE_CLICK,
    VKL_MOUSE_STATE_DOUBLE_CLICK,
    VKL_MOUSE_STATE_CAPTURE,
} VklMouseStateType;



// Key state type
typedef enum
{
    VKL_KEYBOARD_STATE_INACTIVE,
    VKL_KEYBOARD_STATE_ACTIVE,
    VKL_KEYBOARD_STATE_CAPTURE,
} VklKeyboardStateType;



// Transfer status.
typedef enum
{
    VKL_TRANSFER_STATUS_NONE,
    VKL_TRANSFER_STATUS_PROCESSING,
    VKL_TRANSFER_STATUS_DONE,
} VklTransferStatus;



// Canvas refill status.
typedef enum
{
    VKL_REFILL_NONE,
    VKL_REFILL_REQUESTED,
    VKL_REFILL_PROCESSING,
} VklRefillStatus;



// Screencast status.
typedef enum
{
    VKL_SCREENCAST_NONE,
    VKL_SCREENCAST_IDLE,
    VKL_SCREENCAST_AWAIT_COPY,
    VKL_SCREENCAST_AWAIT_TRANSFER,
} VklScreencastStatus;



/*************************************************************************************************/
/*  Event system                                                                                 */
/*************************************************************************************************/

/**
 * Public event types.
 *
 * Public events (also just called "events") are emitted in the main thread and consumed in the
 * background thread by user callbacks.
 */
// Event types
typedef enum
{
    VKL_EVENT_NONE,
    VKL_EVENT_INIT,
    VKL_EVENT_MOUSE_BUTTON,
    VKL_EVENT_MOUSE_MOVE,
    VKL_EVENT_MOUSE_WHEEL,
    VKL_EVENT_MOUSE_DRAG_BEGIN,
    VKL_EVENT_MOUSE_DRAG_END,
    VKL_EVENT_MOUSE_CLICK,
    VKL_EVENT_MOUSE_DOUBLE_CLICK,
    VKL_EVENT_KEY,
    VKL_EVENT_FRAME,
    // VKL_EVENT_TIMER,   // TODO later
    // VKL_EVENT_ONESHOT, // TODO later
    VKL_EVENT_SCREENCAST,
} VklEventType;



// Key type
typedef enum
{
    VKL_KEY_RELEASE,
    VKL_KEY_PRESS,
} VklKeyType;



// Mouse button type
typedef enum
{
    VKL_MOUSE_RELEASE,
    VKL_MOUSE_PRESS,
} VklMouseButtonType;



// Key modifiers
// NOTE: must match GLFW values! no mapping is done for now
typedef enum
{
    VKL_KEY_MODIFIER_NONE = 0x00000000,
    VKL_KEY_MODIFIER_SHIFT = 0x00000001,
    VKL_KEY_MODIFIER_CONTROL = 0x00000002,
    VKL_KEY_MODIFIER_ALT = 0x00000004,
    VKL_KEY_MODIFIER_SUPER = 0x00000008,
} VklKeyModifiers;



// Mouse button
typedef enum
{
    VKL_MOUSE_BUTTON_NONE,
    VKL_MOUSE_BUTTON_LEFT,
    VKL_MOUSE_BUTTON_MIDDLE,
    VKL_MOUSE_BUTTON_RIGHT,
} VklMouseButton;



/*************************************************************************************************/
/*  Type definitions                                                                             */
/*************************************************************************************************/

typedef struct VklScene VklScene;

typedef struct VklMouse VklMouse;
typedef struct VklKeyboard VklKeyboard;
typedef struct VklMouseLocal VklMouseLocal;

// Public events (background thread).
typedef struct VklKeyEvent VklKeyEvent;
typedef struct VklMouseButtonEvent VklMouseButtonEvent;
typedef struct VklMouseMoveEvent VklMouseMoveEvent;
typedef struct VklMouseWheelEvent VklMouseWheelEvent;
typedef struct VklMouseDragEvent VklMouseDragEvent;
typedef struct VklMouseClickEvent VklMouseClickEvent;
typedef struct VklScreencastEvent VklScreencastEvent;
typedef struct VklFrameEvent VklFrameEvent;
typedef union VklEventUnion VklEventUnion;
typedef struct VklEvent VklEvent;

// Private events (main thread).
typedef struct VklTimerEvent VklTimerEvent;
typedef struct VklResizeEvent VklResizeEvent;
typedef struct VklViewport VklViewport;
typedef struct VklRefillEvent VklRefillEvent;
typedef struct VklSubmitEvent VklSubmitEvent;
typedef struct VklPrivateEvent VklPrivateEvent;
typedef union VklPrivateEventUnion VklPrivateEventUnion;

typedef void (*VklCanvasCallback)(VklCanvas*, VklPrivateEvent);
typedef void (*VklEventCallback)(VklCanvas*, VklEvent);

typedef struct VklCanvasCallbackRegister VklCanvasCallbackRegister;
typedef struct VklEventCallbackRegister VklEventCallbackRegister;

typedef struct VklScreencast VklScreencast;
typedef struct VklPendingRefill VklPendingRefill;



/*************************************************************************************************/
/*  Mouse and keyboard structs                                                                   */
/*************************************************************************************************/

struct VklMouse
{
    VklMouseButton button;
    vec2 press_pos;
    vec2 last_pos;
    vec2 cur_pos;
    vec2 wheel_delta;
    float shift_length;

    VklMouseStateType prev_state;
    VklMouseStateType cur_state;

    double press_time;
    double click_time;
};



// In normalize coordinates [-1, +1]
struct VklMouseLocal
{
    vec2 press_pos;
    vec2 last_pos;
    vec2 cur_pos;
    // vec2 delta; // delta between the last and current pos
    // vec2 press_delta; // delta between t
};



struct VklKeyboard
{
    VklKeyCode key_code;
    int modifiers;

    VklKeyboardStateType prev_state;
    VklKeyboardStateType cur_state;

    double press_time;
};



/*************************************************************************************************/
/*  Event structs                                                                                */
/*************************************************************************************************/

struct VklMouseButtonEvent
{
    VklMouseButton button;
    VklMouseButtonType type;
    int modifiers;
};



struct VklMouseMoveEvent
{
    vec2 pos;
};



struct VklMouseWheelEvent
{
    vec2 dir;
};



struct VklMouseDragEvent
{
    vec2 pos;
    VklMouseButton button;
};



struct VklMouseClickEvent
{
    vec2 pos;
    VklMouseButton button;
    bool double_click;
};



struct VklKeyEvent
{
    VklKeyType type;
    VklKeyCode key_code;
    int modifiers;
};



struct VklFrameEvent
{
    uint64_t idx;    // frame index
    double time;     // current time
    double interval; // interval since last event
};



struct VklTimerEvent
{
    uint64_t idx;    // event index
    double time;     // current time
    double interval; // interval since last event
};



struct VklScreencastEvent
{
    uint64_t idx;
    double time;
    double interval;
    uint32_t width;
    uint32_t height;
    uint8_t* rgba;
};



// NOTE: must correspond to the shader structure in common.glsl
struct VklViewport
{
    VkViewport viewport; // Vulkan viewport
    vec4 margins;

    // Position and size of the viewport in screen coordinates.
    uvec2 offset_screen;
    uvec2 size_screen;

    // Position and size of the viewport in framebuffer coordinates.
    uvec2 offset_framebuffer;
    uvec2 size_framebuffer;

    // Options
    // Viewport clipping.
    VklViewportClip clip; // used by the GPU for viewport clipping

    // Used to discard transform on one axis
    VklTransformAxis transform;

    float dpi_scaling;

    // TODO: aspect ratio
};



struct VklRefillEvent
{
    uint32_t img_idx;
    uint32_t cmd_count;
    VklCommands* cmds[32];
    VklViewport viewport;
    VkClearColorValue clear_color;
};



struct VklResizeEvent
{
    uvec2 size_screen;
    uvec2 size_framebuffer;
};



struct VklSubmitEvent
{
    VklSubmit* submit;
};



union VklPrivateEventUnion
{
    VklRefillEvent rf; // for REFILL private events
    VklResizeEvent r;  // for RESIZE private events
    VklFrameEvent t;   // for FRAME private events
    VklFrameEvent f;   // for TIMER private events
    VklSubmitEvent s;  // for SUBMIT private events
};



struct VklPrivateEvent
{
    VklPrivateEventType type;
    void* user_data;
    VklPrivateEventUnion u;
};



union VklEventUnion
{

    VklMouseButtonEvent b; // for MOUSE_BUTTON public events
    VklMouseMoveEvent m;   // for MOUSE_MOVE public events
    VklMouseWheelEvent w;  // for WHEEL public events
    VklMouseDragEvent d;   // for DRAG public events
    VklMouseClickEvent c;  // for DRAG public events
    VklKeyEvent k;         // for KEY public events
    VklFrameEvent f;       // for FRAME public event
    // VklTimerEvent t;       // for TIMER, ONESHOT public events
    VklScreencastEvent s; // for SCREENCAST public events
};



struct VklEvent
{
    VklEventType type;
    void* user_data;
    VklEventUnion u;
};



struct VklCanvasCallbackRegister
{
    VklPrivateEventType type;
    uint64_t idx; // used by TIMER events: increases every time the TIMER event is raised
    double param;
    void* user_data;
    VklCanvasCallback callback;
};



struct VklEventCallbackRegister
{
    VklEventType type;
    // uint64_t idx; // used by TIMER events: increases every time the TIMER event is raised
    double param;
    void* user_data;
    VklEventCallback callback;
};



/*************************************************************************************************/
/*  Misc structs                                                                                 */
/*************************************************************************************************/

struct VklScreencast
{
    VklObject obj;

    VklCanvas* canvas;
    VklCommands cmds;
    VklSemaphores semaphore;
    VklFences fence;
    VklImages staging;
    VklSubmit submit;
    uint64_t frame_idx;
    VklClock clock;
    VklScreencastStatus status;
};



struct VklPendingRefill
{
    bool completed[VKL_MAX_SWAPCHAIN_IMAGES];
    atomic(VklRefillStatus, status);
};



/*************************************************************************************************/
/*  Canvas struct                                                                                */
/*************************************************************************************************/

struct VklCanvas
{
    VklObject obj;
    VklApp* app;
    VklGpu* gpu;

    bool offscreen;
    bool overlay;
    int flags;
    void* user_data;

    // TODO: remove?
    // This thread-safe variable is used by the background thread to
    // safely communicate a status change of the canvas
    atomic(VklObjectStatus, cur_status);
    atomic(VklObjectStatus, next_status);

    VklWindow* window;

    // Swapchain
    VklSwapchain swapchain;
    VklImages depth_image;
    VklFramebuffers framebuffers;
    VklFramebuffers framebuffers_overlay; // used by the overlay renderpass
    VklSubmit submit;

    uint32_t cur_frame; // current frame within the images in flight
    uint64_t frame_idx;
    VklClock clock;
    float fps;

    // TODO: remove
    // when refilling command buffers, keep track of which img_idx were updated until we stop
    // calling the REFILL callbackks
    bool img_updated[VKL_MAX_SWAPCHAIN_IMAGES];

    // Renderpasses.
    VklRenderpass renderpass;         // default renderpass
    VklRenderpass renderpass_overlay; // GUI overlay renderpass

    // Synchronization events.
    VklSemaphores sem_img_available;
    VklSemaphores sem_render_finished;
    VklSemaphores* present_semaphores;
    VklFences fences_render_finished;
    VklFences fences_flight;

    // Default command buffers.
    VklCommands cmds_transfer;
    VklCommands cmds_render;

    // Other command buffers.
    VklContainer commands;

    // Graphics pipelines.
    VklContainer graphics;

    // TODO: remove
    // IMMEDIATE transfers
    VklFifo immediate_queue; // _immediate transfers queue
    VklTransfer immediate_transfers[VKL_MAX_TRANSFERS];
    VklTransfer* immediate_transfer_cur;
    bool immediate_transfer_updated[VKL_MAX_SWAPCHAIN_IMAGES];

    VklFifo transfers;

    // Canvas callbacks, running in the main thread so should be fast to process, especially
    // for internal usage.
    uint32_t canvas_callbacks_count;
    VklCanvasCallbackRegister canvas_callbacks[VKL_MAX_EVENT_CALLBACKS];

    // Event callbacks, running in the background thread, may be slow, for end-users.
    uint32_t event_callbacks_count;
    VklEventCallbackRegister event_callbacks[VKL_MAX_EVENT_CALLBACKS];

    // Event queue.
    VklFifo event_queue;
    VklEvent events[VKL_MAX_FIFO_CAPACITY];
    VklThread event_thread;
    atomic(VklEventType, event_processing);
    VklMouse mouse;
    VklKeyboard keyboard;

    VklScreencast* screencast;
    VklPendingRefill refills;

    VklViewport viewport;
    VklScene* scene;
};



/*************************************************************************************************/
/*  Canvas                                                                                       */
/*************************************************************************************************/

static int _canvas_callbacks(VklCanvas* canvas, VklPrivateEvent event)
{
    int n_callbacks = 0;
    // HACK: we first call the callbacks with no param, then we call the callbacks with a non-zero
    // param. This is a way to use the param as a priority value. This is used by the scene FRAME
    // callback so that it occurs after the user callbacks.
    for (uint32_t pass = 0; pass < 2; pass++)
    {
        for (uint32_t i = 0; i < canvas->canvas_callbacks_count; i++)
        {
            // Will pass the user_data that was registered, to the callback function.
            event.user_data = canvas->canvas_callbacks[i].user_data;

            // Only call the callbacks registered for the specified type.
            if (canvas->canvas_callbacks[i].type == event.type &&
                (pass == 0 || canvas->canvas_callbacks[i].param > 0))
            {
                // log_debug("canvas callback type %d number %d", event.type, i);
                canvas->canvas_callbacks[i].callback(canvas, event);
                n_callbacks++;
            }
        }
    }
    return n_callbacks;
}

// adds callbacks as a function of the backend
// GLFW ex: init_canvas_glfw(VklCanvas* canvas);
// start a background thread that:
// - dequeue event queue (wait)
// - switch the event type
// - call the relevant event callbacks
VKY_EXPORT VklCanvas* vkl_canvas(VklGpu* gpu, uint32_t width, uint32_t height, int flags);

VKY_EXPORT VklCanvas* vkl_canvas_offscreen(VklGpu* gpu, uint32_t width, uint32_t height);

VKY_EXPORT void vkl_canvas_recreate(VklCanvas* canvas);

VKY_EXPORT VklCommands* vkl_canvas_commands(VklCanvas* canvas, uint32_t queue_idx, uint32_t count);



/*************************************************************************************************/
/*  Canvas misc                                                                                  */
/*************************************************************************************************/

VKY_EXPORT void vkl_canvas_clear_color(VklCanvas* canvas, VkClearColorValue color);

VKY_EXPORT void vkl_canvas_size(VklCanvas* canvas, VklCanvasSizeType type, uvec2 size);

VKY_EXPORT void vkl_canvas_close_on_esc(VklCanvas* canvas, bool value);

// screen coordinates
static inline bool _pos_in_viewport(VklViewport viewport, vec2 screen_pos)
{
    ASSERT(viewport.size_screen[0] > 0);
    return (
        viewport.offset_screen[0] <= screen_pos[0] &&                           //
        viewport.offset_screen[1] <= screen_pos[1] &&                           //
        screen_pos[0] <= viewport.offset_screen[0] + viewport.size_screen[0] && //
        screen_pos[1] <= viewport.offset_screen[1] + viewport.size_screen[1]    //
    );
}

VKY_EXPORT VklViewport vkl_viewport_full(VklCanvas* canvas);



/*************************************************************************************************/
/*  Callbacks                                                                                    */
/*************************************************************************************************/

/**
 * Register a callback for private events.
 */
VKY_EXPORT void vkl_canvas_callback(
    VklCanvas* canvas, VklPrivateEventType type, double param, //
    VklCanvasCallback callback, void* user_data);



/**
 * Register a callback for public events.
 *
 * These user callbacks run in the background thread and can access the VklMouse and VklKeyboard
 * structures with the current state of the mouse and keyboard.
 *
 *
 * @par TIMER public events:
 *
 * Callbacks registered with TIMER public events need to specify as `param` the delay, in seconds,
 * between successive TIMER events.
 *
 * TIMER public events are raised by a special thread and enqueued in the Canvas event queue.
 * They are consumed in the background thread (which is a different thread than the TIMER thread).
 *
 */
VKY_EXPORT void vkl_event_callback(
    VklCanvas* canvas, VklEventType type, double param, //
    VklEventCallback callback, void* user_data);



/*************************************************************************************************/
/*  State changes                                                                                */
/*************************************************************************************************/

VKY_EXPORT void vkl_canvas_set_status(VklCanvas* canvas, VklObjectStatus status);

VKY_EXPORT void vkl_canvas_to_refill(VklCanvas* canvas, bool value);

VKY_EXPORT void vkl_canvas_to_close(VklCanvas* canvas, bool value);



/*************************************************************************************************/
/*  Fast transfers                                                                               */
/*************************************************************************************************/

VKY_EXPORT void vkl_canvas_buffers(
    VklCanvas* canvas, VklBufferRegions br, VkDeviceSize offset, VkDeviceSize size,
    const void* data, bool need_refill);

VKY_EXPORT void vkl_upload_buffers_immediate(
    VklCanvas* canvas, VklBufferRegions regions, bool update_all_regions, //
    VkDeviceSize offset, VkDeviceSize size, void* data);



/*************************************************************************************************/
/*  Screencast                                                                                   */
/*************************************************************************************************/

/**
 * Prepare the canvas for a screencast.
 *
 * A **screencast** is a live record of one or several frames of the canvas during the interactive
 * execution of the app. Creating a screencast is required for:
 * - screenshots,
 * - video records (requires ffmpeg)
 *
 * @param canvas
 * @param interval If non-zero, the Canvas will raise periodic SCREENCAST private events every
 *      `interval` seconds. The private event payload will contain a pointer to the grabbed
 *      framebuffer image.
 * @param rgb If NULL, the Canvas will create a CPU buffer with the appropriate size. Otherwise,
 *      the images will be copied to the provided buffer. The caller must ensure the buffer is
 *      allocated with enough memory to store the image. Providing a pointer disables resize
 *      support (the swapchain and GPU images will not be recreated upon resize).
 *
 * This command creates a host-coherent GPU image with the same size as the current framebuffer
 * size.
 *
 */
VKY_EXPORT void vkl_screencast(VklCanvas* canvas, double interval);



/**
 * Destroy the screencast.
 */
VKY_EXPORT void vkl_screencast_destroy(VklCanvas* canvas);



/**
 * Make a screenshot.
 *
 * This function creates a screencast if there isn't one already. It is implemented with hard
 * synchronization commands so this command should *not* be used for creating many successive
 * screenshots. For that, one should register a SCREENCAST private event callback.
 *
 * @param canvas
 * @return A pointer to the 24-bit RGB framebuffer.
 *
 */
VKY_EXPORT uint8_t* vkl_screenshot(VklCanvas* canvas);



/**
 * Make a screenshot and save it to a PNG or PPM file.
 *
 * @param canvas
 * @param filename Path to the screenshot image.
 *
 */
VKY_EXPORT void vkl_screenshot_file(VklCanvas* canvas, const char* filename);



/*************************************************************************************************/
/*  Mouse and keyboard                                                                           */
/*************************************************************************************************/

VKY_EXPORT VklMouse vkl_mouse(void);

VKY_EXPORT void vkl_mouse_reset(VklMouse* mouse);

VKY_EXPORT void vkl_mouse_event(VklMouse* mouse, VklCanvas* canvas, VklEvent ev);

VKY_EXPORT void vkl_mouse_local(
    VklMouse* mouse, VklMouseLocal* mouse_local, VklCanvas* canvas, VklViewport viewport);

VKY_EXPORT VklKeyboard vkl_keyboard(void);

VKY_EXPORT void vkl_keyboard_reset(VklKeyboard* keyboard);

VKY_EXPORT void vkl_keyboard_event(VklKeyboard* keyboard, VklCanvas* canvas, VklEvent ev);



/*************************************************************************************************/
/*  Event system                                                                                 */
/*************************************************************************************************/

VKY_EXPORT void vkl_event_enqueue(VklCanvas* canvas, VklEvent event);

VKY_EXPORT void vkl_event_mouse_button(
    VklCanvas* canvas, VklMouseButtonType type, VklMouseButton button, int modifiers);

VKY_EXPORT void vkl_event_mouse_move(VklCanvas* canvas, vec2 pos);

VKY_EXPORT void vkl_event_mouse_wheel(VklCanvas* canvas, vec2 dir);

VKY_EXPORT void vkl_event_mouse_click(VklCanvas* canvas, vec2 pos, VklMouseButton button);

VKY_EXPORT void vkl_event_mouse_double_click(VklCanvas* canvas, vec2 pos, VklMouseButton button);

VKY_EXPORT void vkl_event_mouse_drag(VklCanvas* canvas, vec2 pos, VklMouseButton button);

VKY_EXPORT void vkl_event_mouse_drag_end(VklCanvas* canvas, vec2 pos, VklMouseButton button);

VKY_EXPORT void
vkl_event_key(VklCanvas* canvas, VklKeyType type, VklKeyCode key_code, int modifiers);

VKY_EXPORT void vkl_event_frame(VklCanvas* canvas, uint64_t idx, double time, double interval);

VKY_EXPORT void vkl_event_timer(VklCanvas* canvas, uint64_t idx, double time, double interval);

VKY_EXPORT VklEvent* vkl_event_dequeue(VklCanvas* canvas, bool wait);

// Return the number of events of the given type that are still being processed or pending in the
// queue.
VKY_EXPORT int vkl_event_pending(VklCanvas* canvas, VklEventType type);

VKY_EXPORT void vkl_event_stop(VklCanvas* canvas);



/*************************************************************************************************/
/*  Event loop                                                                                   */
/*************************************************************************************************/

VKY_EXPORT void vkl_canvas_frame(VklCanvas* canvas);

VKY_EXPORT void vkl_canvas_frame_submit(VklCanvas* canvas);

VKY_EXPORT void vkl_app_run(VklApp* app, uint64_t frame_count);



#ifdef __cplusplus
}
#endif

#endif
