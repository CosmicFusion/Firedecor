#include <glm/gtc/matrix_transform.hpp>

#include <linux/input-event-codes.h>

#include <wayfire/nonstd/wlroots.hpp>
#include <wayfire/output.hpp>
#include <wayfire/opengl.hpp>
#include <wayfire/core.hpp>
#include <wayfire/view-transform.hpp>
#include <wayfire/signal-definitions.hpp>
#include <wayfire/scene-operations.hpp>
#include <wayfire/scene.hpp>
#include "wayfire/toplevel.hpp"
#include <wayfire/toplevel-view.hpp>
#include <wayfire/window-manager.hpp>

#include "cosmodecor-layout.hpp"
#include "cosmodecor-theme.hpp"
#include "cosmodecor-subsurface.hpp"

#include "cairo-simpler.hpp"
#include "cairo-util.hpp"

#define INACTIVE 0
#define ACTIVE 1
#define FORCE true
#define DONT_FORCE false

#include <fstream>

namespace wf::cosmodecor {

    class simple_decoration_node_t : public wf::scene::node_t, public wf::pointer_interaction_t, public wf::touch_interaction_t
    {
        std::weak_ptr<wf::toplevel_view_interface_t> _view;

        wf::signal::connection_t<wf::view_title_changed_signal> title_set = [=, this] (wf::view_title_changed_signal *ev) {
            if (auto view = _view.lock()) {
                title_changed = true;
                view->damage(); // trigger re-render
            }
        };

        void update_title(double scale) {
            if (auto view = _view.lock()) {
                dimensions_t title_size = {
                    (int)(title.dims.width * scale), (int)(title.dims.height * scale)
                };

                auto o = HORIZONTAL;
                std::string text = title.text;
                int count = 0;
                wf::dimensions_t size = title_size;

                for (auto texture : { title.hor }) {
                    for (auto state : { ACTIVE, INACTIVE }) {
                        cairo_surface_t *surface;
                        surface = theme.form_title(text, size, state, o, scale);
                        cairo_surface_upload_to_texture(surface, texture[state]);
                        cairo_surface_destroy(surface);
                    }

                    count++;
                };
                title_needs_update = false;
            }
        }

        void update_icon(double scale) {
            if (auto view = _view.lock()) {
                if (view->get_app_id() != icon.app_id) {
                    icon.app_id = view->get_app_id();
                    auto surface = theme.form_icon(icon.app_id, scale);
                    cairo_surface_upload_to_texture(surface, icon.texture);
                    cairo_surface_destroy(surface);
                }
            }
        }

        void update_layout(bool force, double scale) {
            if (auto view = _view.lock()) {
                if ((title.colors != theme.get_title_colors()) || title_changed || force) {
                    // Update cached variables
                    title.colors = theme.get_title_colors();
                    title.text = view->get_title();

                    wf::dimensions_t cur_size = theme.get_text_size(title.text, size.width, scale);

                    title.dims.height = cur_size.height;
                    title.dims.width = cur_size.width;

                    title_changed = false;
                    title_needs_update = true;

                    // Necessary in order to immediately place areas correctly
                    layout.resize(size.width, size.height, title.dims);
                }
            }
        }

        // Title variables
        struct {
            simple_texture_t hor[2];
            std::string text = "";
            color_set_t colors;
            dimensions_t dims;
        } title;

        bool title_needs_update = false;
        bool title_changed = true;

        // Icon variables
        struct {
            simple_texture_t texture;
            std::string app_id = "";
        } icon;

        // Edge variables
        struct edge_colors_t {
            color_set_t border, outline;
        } edges;

        // Other general variables
        decoration_theme_t theme;
        decoration_layout_t layout;
        region_t cached_region;
        dimensions_t size;

    public:
        border_size_t border_size;

        template<typename T>
        T get_option(std::string theme, std::string option_name) {
            wf::config::config_manager_t& config = wf::get_core().config;
            auto option = config.get_option<std::string>(theme + "/" + option_name);
            if (option == nullptr || theme == "default") {
                return config.get_option<T>("cosmodecor/" + option_name)->get_value();
            } else {
                return wf::option_type::from_string<T>(option->get_value()).value();
            }
        }

        theme_options get_options(std::string theme) {
            theme_options options = {
                get_option<std::string>(theme, "font"),
                get_option<int>(theme, "font_size"),
                get_option<wf::color_t>(theme, "active_title"),
                get_option<wf::color_t>(theme, "inactive_title"),
                get_option<int>(theme, "max_title_size"),

                get_option<std::string>(theme, "border_size"),
                get_option<wf::color_t>(theme, "active_border"),
                get_option<wf::color_t>(theme, "inactive_border"),

                get_option<int>(theme, "outline_size"),
                get_option<wf::color_t>(theme, "active_outline"),
                get_option<wf::color_t>(theme, "inactive_outline"),

                get_option<int>(theme, "button_size"),
                get_option<std::string>(theme, "button_style"),
                get_option<wf::color_t>(theme, "normal_min"),
                get_option<wf::color_t>(theme, "hovered_min"),
                get_option<wf::color_t>(theme, "normal_max"),
                get_option<wf::color_t>(theme, "hovered_max"),
                get_option<wf::color_t>(theme, "normal_close"),
                get_option<wf::color_t>(theme, "hovered_close"),
                get_option<bool>(theme, "inactive_buttons"),

                get_option<int>(theme, "icon_size"),
                get_option<std::string>(theme, "icon_theme"),

                get_option<int>(theme, "padding_size"),
                get_option<std::string>(theme, "layout"),

                get_option<std::string>(theme, "ignore_views"),
                get_option<bool>(theme, "debug_mode")
            };
            return options;
        }

        simple_decoration_node_t(wayfire_toplevel_view view)
            : node_t(false),
              theme{get_options("default")},
              layout{theme, [=, this] (wlr_box box) {
                  wf::scene::damage_node(shared_from_this(), box + get_offset()); }} {
            this->_view = view->weak_from_this();
            view->connect(&title_set);

            title.dims = {0, 0};

            // make sure to hide frame if the view is fullscreen
            update_decoration_size();
        }

        point_t get_offset() {
            return { -border_size.left, -border_size.top };
        }

        class decoration_render_instance_t : public wf::scene::render_instance_t
        {
            simple_decoration_node_t *self;
            wf::scene::damage_callback push_damage;
            wf::signal::connection_t<wf::scene::node_damage_signal> on_surface_damage =
                [=, this] (wf::scene::node_damage_signal *data) {
                    push_damage(data->region);
                };

        public:
            decoration_render_instance_t(simple_decoration_node_t *self, wf::scene::damage_callback push_damage)
            {
                this->self = self;
                this->push_damage = push_damage;
                self->connect(&on_surface_damage);
            }

            void schedule_instructions(std::vector<wf::scene::render_instruction_t>& instructions,
                                       const wf::render_target_t& target, wf::region_t& damage) override
            {
                auto our_region = self->cached_region + self->get_offset();
                wf::region_t our_damage = damage & our_region;
                if (!our_damage.empty()) {
                    instructions.push_back(wf::scene::render_instruction_t{
                            .instance = this,
                            .target   = target,
                            .damage   = std::move(our_damage),
                        });
                }
            }

            void render(const wf::render_target_t& target,
                        const wf::region_t& region) override
            {
                self->update_layout(DONT_FORCE, target.scale);

                for (const auto& box : region) {
                    self->render_scissor_box(target, self->get_offset(), wlr_box_from_pixman_box(box));
                }
            }
        };

        void gen_render_instances(std::vector<wf::scene::render_instance_uptr>& instances,
                                  wf::scene::damage_callback push_damage, wf::output_t *output = nullptr) override
        {
            instances.push_back(std::make_unique<decoration_render_instance_t>(this, push_damage));
        }

        wf::geometry_t get_bounding_box() override
        {
            return wf::construct_box(get_offset(), size);
        }

        void render_title(const render_target_t& fb, geometry_t geometry, geometry_t scissor) {
            if (title_needs_update) {
                update_title(fb.scale);
            }

            if (auto view = _view.lock()) {
                simple_texture_t *texture;
                uint32_t bits = OpenGL::TEXTURE_TRANSFORM_INVERT_Y;
                texture = &title.hor[view->activated];

                // optimization skipping damage that is outside of title bar area (below it)
                if (scissor.y < 0) {
                    OpenGL::render_begin(fb);
                    fb.logic_scissor(scissor);
                    OpenGL::render_texture(texture->tex, fb, geometry, glm::vec4(1.0f), bits);
                    OpenGL::render_end();
                }
            }
        }

        void render_icon(const render_target_t& fb, geometry_t g,
                         const geometry_t& scissor, int32_t bits) {
            update_icon(fb.scale);
            OpenGL::render_begin(fb);
            fb.logic_scissor(scissor);
            OpenGL::render_texture(icon.texture.tex, fb, g, glm::vec4(1.0f), bits);
            OpenGL::render_end();
        }

        color_t alpha_trans(color_t c) {
            return { c.r * c.a, c.g * c.a, c.b * c.a, c.a };
        }

        void render_background_area(const render_target_t& fb, geometry_t g,
                                    point_t rect, geometry_t scissor, unsigned long i,
                                    decoration_area_type_t type, matrix<int> m,
                                    edge_t edge) {
            if (auto view = _view.lock()) {
                // The view's origin
                point_t o = { rect.x, rect.y };

                // Render a single rectangle when the area is a background
                color_t color = (view->activated) ?
                    alpha_trans(theme.get_border_colors().active) :
                    alpha_trans(theme.get_border_colors().inactive);
                color_t o_color = (view->activated) ?
                    alpha_trans(theme.get_outline_colors().active) :
                    alpha_trans(theme.get_outline_colors().inactive);

                wf::geometry_t g_o;
                int o_s = theme.get_outline_size();
                if (edge == wf::cosmodecor::EDGE_TOP) {
                    g_o = { g.x, g.y, g.width, o_s };
                    g = { g.x, g.y + o_s, g.width, g.height - o_s };
                } else if (edge == wf::cosmodecor::EDGE_LEFT) {
                    // the g.y + is probably not correct, but works for now
                    g_o = { g.x, g.y, o_s, g.y + g.height };
                    g = { g.x + o_s, g.y, g.width - o_s, g.y + g.height };
                } else if (edge == wf::cosmodecor::EDGE_BOTTOM) {
                    g_o = { g.x, g.y + g.height - o_s, g.width, o_s };
                    g = { g.x, g.y, g.width, g.height - o_s };
                } else if (edge == wf::cosmodecor::EDGE_RIGHT) {
                    // the g.y + is probably not correct, but works for now
                    g_o = { g.x + g.width - o_s, g.y, o_s, g.y + g.height };
                    g = { g.x, g.y, g.width - o_s, g.y + g.height };
                }
                g_o = g_o + o;

                OpenGL::render_begin(fb);
                fb.logic_scissor(scissor);
                OpenGL::render_rectangle(g + o, color, fb.get_orthographic_projection());
                OpenGL::render_rectangle(g_o, o_color, fb.get_orthographic_projection());
                OpenGL::render_end();
            }
        }

        void render_background(const render_target_t& fb, geometry_t rect,
                               const geometry_t& scissor) {
            edge_colors_t colors = {
                theme.get_border_colors(), theme.get_outline_colors()
            };

            colors.border.active = alpha_trans(colors.border.active);
            colors.border.inactive = alpha_trans(colors.border.inactive);

            // Borders
            unsigned long i = 0;
            point_t rect_o = { rect.x, rect.y };

            for (auto area : layout.get_background_areas()) {
                render_background_area(fb, area->get_geometry(), rect_o, scissor, i, area->get_type(),
                                       area->get_m(), area->get_edge());
                i++;
            }
        }

        void render_scissor_box(const render_target_t& fb, point_t origin,
                                const wlr_box& scissor) {
            // Draw the background
            wlr_box geometry{origin.x, origin.y, size.width, size.height};
            render_background(fb, geometry, scissor);

            wlr_box clip = scissor;
            if (!wlr_box_intersection(&clip, &scissor, &geometry)) {
                return;
            }

            auto renderables = layout.get_renderable_areas();
            for (auto item : renderables) {
                int32_t bits = 0;
                if (item->get_type() == DECORATION_AREA_TITLE) {
                    // clip title so it doesn't overlap with buttons
                    // FIXME: these sizes should not be hardcoded
                    wlr_box title_clip = geometry;
                    title_clip.x += 8;
                    title_clip.width -= 98;

                    if (wlr_box_intersection(&title_clip, &clip, &title_clip)) {
                        render_title(fb, item->get_geometry() + origin, title_clip);
                    }
                } else if (item->get_type() == DECORATION_AREA_BUTTON) {
                    if (auto view = _view.lock()) {
                        item->as_button().set_active(view->activated);
                        //item->as_button().set_maximized(view->tiled_edges);
                    }
                    item->as_button().render(fb, item->get_geometry() + origin, clip);
                } else if (item->get_type() == DECORATION_AREA_ICON) {
                    render_icon(fb, item->get_geometry() + origin, clip, bits);
                }
            }
        }

        std::optional<wf::scene::input_node_t> find_node_at(const wf::pointf_t& at) override {
            wf::pointf_t local = at - wf::pointf_t{get_offset()};
            if (cached_region.contains_pointf(local)) {
                return wf::scene::input_node_t{
                    .node = this,
                    .local_coords = local,
                };
            }

            return {};
        }

        pointer_interaction_t& pointer_interaction() override {
            return *this;
        }

        /* wf::compositor_surface_t implementation */
        void handle_pointer_enter(wf::pointf_t point) override {
            point -= wf::pointf_t{get_offset()};
            layout.handle_motion(point.x, point.y);
        }

        void handle_pointer_leave() override {
            layout.handle_focus_lost();
        }

        void handle_pointer_motion(wf::pointf_t to, uint32_t) override {
            to -= wf::pointf_t{get_offset()};
            handle_action(layout.handle_motion(to.x, to.y));
        }

        void handle_pointer_button(const wlr_pointer_button_event& ev) override {
            if (ev.button != BTN_LEFT) {
                return;
            }

            handle_action(layout.handle_press_event(ev.state == WLR_BUTTON_PRESSED));
        }

        // TODO: implement a pinning button.
        void handle_action(decoration_layout_t::action_response_t action) {
            if (auto view = _view.lock()) {
                switch (action.action) {
                case DECORATION_ACTION_MOVE:
                    return wf::get_core().default_wm->move_request(view);

                case DECORATION_ACTION_RESIZE:
                    return wf::get_core().default_wm->resize_request(view, action.edges);

                case DECORATION_ACTION_CLOSE:
                    return view->close();

                case DECORATION_ACTION_TOGGLE_MAXIMIZE:
                    if (view->pending_tiled_edges()) {
                        wf::get_core().default_wm->tile_request(view, 0);
                    } else {
                        wf::get_core().default_wm->tile_request(view, wf::TILED_EDGES_ALL);
                    }
                    break;

                case DECORATION_ACTION_MINIMIZE:
                    wf::get_core().default_wm->minimize_request(view, true);
                    break;

                default:
                    break;
                }
            }
        }

        void handle_touch_down(uint32_t time_ms, int finger_id, wf::pointf_t position) override
        {
            handle_touch_motion(time_ms, finger_id, position);
            handle_action(layout.handle_press_event());
        }

        void handle_touch_up(uint32_t time_ms, int finger_id, wf::pointf_t lift_off_position) override
        {
            handle_action(layout.handle_press_event(false));
            layout.handle_focus_lost();
        }

        void handle_touch_motion(uint32_t time_ms, int finger_id, wf::pointf_t position) override
        {
            position -= wf::pointf_t{get_offset()};
            layout.handle_motion(position.x, position.y);
        }

        void resize(dimensions_t dims) {
            if (auto view = _view.lock()) {
                view->damage();
                size = dims;

                layout.resize(size.width, size.height, title.dims);

                if (!view->toplevel()->current().fullscreen) {
                    this->cached_region = layout.calculate_region();
                }
                view->damage();
            }
        }

        void update_decoration_size() {
            bool fullscreen = _view.lock()->toplevel()->current().fullscreen;
            if (fullscreen) {
                this->cached_region.clear();
            } else {
                border_size = layout.parse_border(theme.get_border_size());
                this->cached_region = layout.calculate_region();
            }
        }
    };

    wf::cosmodecor::simple_decorator_t::simple_decorator_t(wayfire_toplevel_view view) {
        this->view = view;
        deco       = std::make_shared<simple_decoration_node_t>(view);
        deco->resize(wf::dimensions(view->get_pending_geometry()));
        wf::scene::add_back(view->get_surface_root_node(), deco);

        view->connect(&on_view_activated);
        view->connect(&on_view_geometry_changed);
        view->connect(&on_view_fullscreen);

        on_view_activated = [this] (auto) {
            wf::scene::damage_node(deco, deco->get_bounding_box());
        };

        on_view_geometry_changed = [this] (auto) {
            deco->resize(wf::dimensions(this->view->get_geometry()));
        };

        on_view_fullscreen = [this] (auto) {
            deco->update_decoration_size();
            if (!this->view->toplevel()->current().fullscreen) {
                deco->resize(wf::dimensions(this->view->get_geometry()));
            }
        };
    }

    wf::cosmodecor::simple_decorator_t::~simple_decorator_t() {
        wf::scene::remove_child( deco );
    }

    wf::decoration_margins_t wf::cosmodecor::simple_decorator_t::get_margins(const wf::toplevel_state_t& state) {
        if (state.fullscreen) {
            return {0, 0, 0, 0};
        }

        auto margins = wf::decoration_margins_t {
            .left   = deco->border_size.left,
            .right  = deco->border_size.right,
            .bottom = deco->border_size.bottom,
            .top    = deco->border_size.top,
        };
        return margins;
    }

    // namespace
}
