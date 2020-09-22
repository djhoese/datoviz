import csv
from pathlib import Path

from imageio import imread
import numpy as np

from visky.wrap import viskylib as vl
from visky.wrap import (
    upload_data, pointer, array_pointer, get_const, key_string, to_byte
)
from visky import _constants as const
from visky import _types as tp


class App:
    def __init__(self):
        self._app = vl.vky_create_app(const.BACKEND_GLFW, None)
        self._canvases = []

    def canvas(self, **kwargs):
        c = Canvas(self._app, **kwargs)
        self._canvases.append(c)
        return c

    def run(self):
        vl.vky_run_app(self._app)
        vl.vky_destroy_app(self._app)


class Canvas:
    def __init__(self, app, shape=(1, 1), width=800, height=600, background=None):
        self.n_rows, self.n_cols = shape
        self._canvas = vl.vky_create_canvas(app, width, height)
        self._scene = vl.vky_create_scene(
            self._canvas, get_const(background, 'white'), shape[0], shape[1])

        # HACK: need to keep track of the callbacks in order to prevent them from being
        # garbage-collected, which would lead to a segfault in the C library.
        self._callbacks = []

    def __getitem__(self, shape):
        assert len(shape) == 2
        return Panel(self, row=shape[0], col=shape[1])

    def set_heights(self, heights):
        heights = np.asarray(heights, dtype=np.float32)
        assert heights.ndim == 1
        assert heights.shape == (self.n_rows,)
        vl.vky_set_grid_heights(self._scene, heights)

    def on_key(self, f):
        @tp.canvas_callback
        def _on_key_wrap(p_canvas):
            key = vl.vky_event_key(p_canvas)
            key_s = key_string(key)
            f(key_s)
        self._callbacks.append(_on_key_wrap)
        vl.vky_add_frame_callback(self._canvas, _on_key_wrap)

    def on_mouse(self, f):
        @tp.canvas_callback
        def _on_mouse_wrap(p_canvas):
            pos = vl.vky_event_mouse(p_canvas)
            f((pos.x, pos.y))
        self._callbacks.append(_on_mouse_wrap)
        vl.vky_add_frame_callback(self._canvas, _on_mouse_wrap)


class Panel:
    def __init__(self, canvas, row=0, col=0, controller_type=None, params=None):
        self._canvas = canvas._canvas
        self._scene = canvas._scene
        self.row, self.col = row, col
        self._panel = vl.vky_get_panel(self._scene, row, col)
        controller_type = get_const(controller_type, 'controller_axes_2D')
        self.set_controller(controller_type, params=params)
        self._axes = vl.vky_get_axes(self._panel)

    def set_controller(self, controller_type, params=None):
        vl.vky_set_controller(self._panel, controller_type, params)

    def axes_range(self, x0, x1, y0, y1):
        vl.vky_axes_set_range(self._axes, x0, x1, y0, y1)

    def visual(self, visual_type, params=None, obj=None):
        visual_type = get_const(visual_type)
        assert visual_type
        p_visual = vl.vky_visual(self._scene, visual_type, params, obj)
        vl.vky_add_visual_to_panel(
            p_visual, self._panel, get_const('viewport_inner'), get_const('visual_priority_none'))
        cls = Visual
        if visual_type == const.VISUAL_IMAGE:
            cls = Image
        return cls(self, p_visual)

    def image(self, image):
        height, width = image.shape[:2]
        tex_params = vl.vky_default_texture_params(
            tp.T_IVEC3(width, height, 1))
        visual = self.visual('visual_image', pointer(tex_params))

        # Image vertices.
        vertices = np.zeros((1,), dtype=[
            ('p0', 'f4', 3),
            ('p1', 'f4', 3),
            ('uv0', 'f4', 2),
            ('uv1', 'f4', 2)
        ])
        vertices['p0'][0] = (-1, -1, 0)
        vertices['p1'][0] = (+1, +1, 0)
        vertices['uv0'][0] = (0, 1)
        vertices['uv1'][0] = (1, 0)
        visual.upload(vertices)
        return visual

    def plot(self, points, colors=None, lw=1, miter=4, cap='round', join='round'):
        cap = get_const('cap_%s' % cap)
        join = get_const('join_%s' % join)
        params = tp.T_PATH_PARAMS(lw, miter, cap, join, 0)
        visual = self.visual('visual_path', pointer(params))

        n = len(points)

        # TODO: multiple paths
        items = np.zeros((1,), dtype=np.dtype(tp.T_PATH_DATA))
        items['point_count'][0] = n
        items['points'][0] = int(array_pointer(points).value)
        items['colors'][0] = int(array_pointer(colors).value)
        items['topology'][0] = 0

        visual.upload(items)

        return visual


class Visual:
    def __init__(self, panel, p_visual):
        self.panel = panel
        self._scene = panel._scene
        self._visual = p_visual

    def upload(self, *args, **kwargs):
        upload_data(self._visual, *args, **kwargs)


class Image(Visual):
    def upload_image(self, image):
        vl.vky_visual_image_upload(self._visual, array_pointer(image))


def read_csv(path):
    out = {}
    with open(path, 'r') as f:
        csv_reader = csv.DictReader(f)
        for row in csv_reader:
            out[row['name'].lower()] = (
                int(row['row']), int(row['col']), int(row['size']))
    return out


# Read the colormap texture.
COLORMAP = imread(
    Path(__file__).parent / '../../../data/textures/color_texture.png')
COLORMAP_INFO = read_csv(
    Path(__file__).parent / '../../../data/textures/color_texture.csv')


def get_color(cmap, x, vmin=0, vmax=1, alpha=1):
    out = np.empty(x.shape + (4,), dtype=np.uint8)
    assert cmap in COLORMAP_INFO, cmap
    row, col, size = COLORMAP_INFO.get(cmap)
    if size == 256:
        # continuous colormap with interpolation
        i = to_byte(x, vmin=vmin, vmax=vmax)
        out[..., :] = COLORMAP[row, i, :]
    else:
        # colormap with no interpolation
        out[..., :] = COLORMAP[row, x, :]
    out[..., 3] = to_byte(alpha)
    return out