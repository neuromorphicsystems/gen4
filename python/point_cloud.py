import numpy
import vispy.color
import vispy.gloo
import vispy.visuals

VERTEX_SHADER = """
attribute vec3 position;
uniform float base_point_size;
uniform bool floor_timestamps;
uniform float end_t;
float big_float = 1e10;
void main (void) {
    vec4 visual_position = vec4(
        position[0],
        position[1],
        floor_timestamps ? (int(position[2] - end_t) / 10000) * 10000 : position[2] - end_t,
        1.0
    );
    vec4 framebuffer_position = $visual_to_framebuffer(visual_position);
    gl_Position = $framebuffer_to_render(framebuffer_position);
    vec4 scene_position = $framebuffer_to_scene(framebuffer_position);
    vec4 scene_coordinates = $framebuffer_to_scene(vec4(
        big_float,
        framebuffer_position[1],
        framebuffer_position[2],
        framebuffer_position[3]
    )) - scene_position;
    vec4 screen_position = $scene_to_framebuffer(scene_position + normalize(scene_coordinates) * base_point_size);
    gl_PointSize = screen_position[0] / screen_position[3] - framebuffer_position[0] / framebuffer_position[3];
}
"""

FRAGMENT_SHADER = """
uniform vec4 color;
void main() {
    gl_FragColor = color;
}
"""


class PointCloudVisual(vispy.visuals.Visual):
    _shaders = {
        "vertex": VERTEX_SHADER,
        "fragment": FRAGMENT_SHADER,
    }

    def __init__(self, color: numpy.ndarray, base_point_size):
        super().__init__(vcode=VERTEX_SHADER, fcode=FRAGMENT_SHADER)
        self.set_gl_state(
            depth_test=True,
            blend=True,
            blend_func=("src_alpha", "one_minus_src_alpha"),
        )
        self._draw_mode = "points"
        self.vbo = vispy.gloo.VertexBuffer()
        self.set_data(numpy.array([], dtype=[("position", numpy.float32, 3)]))
        self.base_point_size = base_point_size
        self.floor_timestamps = False
        self.end_t = 0.0
        self.set_color(color)
        self.update()

    def set_data(self, pos: numpy.ndarray):
        self.vbo.set_data(pos)
        self.shared_program.bind(self.vbo)

    def _prepare_transforms(self, view):
        view.view_program.vert["visual_to_framebuffer"] = view.get_transform(
            "visual", "framebuffer"
        )
        view.view_program.vert["framebuffer_to_render"] = view.get_transform(
            "framebuffer", "render"
        )
        view.view_program.vert["framebuffer_to_scene"] = view.get_transform(
            "framebuffer", "scene"
        )
        view.view_program.vert["scene_to_framebuffer"] = view.get_transform(
            "scene", "framebuffer"
        )

    def _prepare_draw(self, view=None):
        if view is not None:
            view.view_program["base_point_size"] = self.base_point_size
            view.view_program["floor_timestamps"] = self.floor_timestamps
            view.view_program["end_t"] = self.end_t
        return True

    def set_color(self, color: numpy.ndarray):
        self.shared_program["color"] = color
        self.update()
