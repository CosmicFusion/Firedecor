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

#include "firedecor-layout.hpp"
#include "firedecor-theme.hpp"
#include "firedecor-subsurface.hpp"

#include "cairo-simpler.hpp"
#include "cairo-util.hpp"

#define INACTIVE 0
#define ACTIVE 1
#define FORCE true
#define DONT_FORCE false

#include <fstream>

namespace wf::firedecor {

    class simple_decoration_node_t : public wf::scene::node_t, public wf::pointer_interaction_t, public wf::touch_interaction_t
    {
        wayfire_toplevel_view view;

        wf::signal::connection_t<wf::view_title_changed_signal> title_set = [=, this] (wf::view_title_changed_signal *ev) {
            if (ev->view == view) {
                title_changed = true;
                view->damage(); // trigger re-render
            }
        };

        void update_title(double scale) {
            dimensions_t title_size = {
                (int)(title.dims.width * scale), (int)(title.dims.height * scale)
            };
            dimensions_t dots_size = {
                (int)(title.dots_dims.width * scale),
                (int)(title.dots_dims.height * scale)
            };

            auto o = HORIZONTAL;
            std::string text = title.text;
            int count = 0;
            wf::dimensions_t size = title_size;

            for (auto texture : { title.hor, title.ver,
                                  title.hor_dots, title.ver_dots }) {
                for (auto state : { ACTIVE, INACTIVE }) {
                    cairo_surface_t *surface;
                    surface = theme.form_title(text, size, state, o, scale);
                    cairo_surface_upload_to_texture(surface, texture[state]);
                    cairo_surface_destroy(surface);
                }

                o = (o == HORIZONTAL) ? VERTICAL : HORIZONTAL;
                if (count == 1) {
                    text = "...";
                    size = dots_size;
                }
                count++;
            };
            title_needs_update = false;
        }

        void update_icon(double scale) {
            if (view->get_app_id() != icon.app_id) {
                icon.app_id = view->get_app_id();
                auto surface = theme.form_icon(icon.app_id, scale);
                cairo_surface_upload_to_texture(surface, icon.texture);
                cairo_surface_destroy(surface);
            }
        }

        void update_layout(bool force, double scale) {
            if ((title.colors != theme.get_title_colors()) || title_changed || force) {
                /** Updating the cached variables */
                title.colors = theme.get_title_colors();
                title.text = view->get_title();

                wf::dimensions_t cur_size = theme.get_text_size(title.text, size.width, scale);

                title.dims.height = cur_size.height;

                if (cur_size.width <= theme.get_max_title_size()) {
                    title.dims.width = cur_size.width;
                    title.too_big = false;
                    title.dots_dims = { 0, 0 };
                } else {
                    wf::dimensions_t dots_size = theme.get_text_size("...", size.width, scale);

                    title.dims.width = theme.get_max_title_size() - dots_size.width;
                    title.too_big = true;
                    title.dots_dims = dots_size;
                }

                title_changed = false;
                title_needs_update = true;

                /** Necessary in order to immediately place areas correctly */
                layout.resize(size.width, size.height, title.dims, title.dots_dims);
            }
        }

        /** Title variables */
        struct {
            simple_texture_t hor[2], hor_dots[2];
            simple_texture_t ver[2], ver_dots[2];
            std::string text = "";
            color_set_t colors;
            dimensions_t dims, dots_dims;
            bool dots_set = false, too_big = true;
        } title;

        bool title_needs_update = false;
        bool title_changed = true;

        /** Icon variables */
        struct {
            simple_texture_t texture;
            std::string app_id = "";
        } icon;

        /** Corner variables */
        struct corner_texture_t {
            simple_texture_t tex[2];
            cairo_surface_t *surf[2];
            geometry_t g;
            int r;
        };

        struct {
            corner_texture_t tr, tl, bl, br;
        } corners;

        /** Edge variables */
        struct edge_colors_t {
            color_set_t border, outline;
        } edges;

        /** Other general variables */
        decoration_theme_t theme;
        decoration_layout_t layout;
        region_t cached_region;
        dimensions_t size;

        void update_corners(edge_colors_t colors, int corner_radius, double scale) {
            if ((this->corner_radius != corner_radius) ||
                (edges.border != colors.border) ||
                (edges.outline != colors.outline)) {
                corners.tr.r = corners.tl.r = corners.bl.r = corners.br.r = 0;

                std::stringstream round_on_str(theme.get_round_on());
                std::string corner;
                while (round_on_str >> corner) {
                    if (corner == "all") {
                        corners.tr.r = corners.tl.r = corners.bl.r = corners.br.r
                            = theme.get_corner_radius() * scale;
                        break;
                    } else if (corner == "tr") {
                        corners.tr.r = (theme.get_corner_radius() * scale);
                    } else if (corner == "tl") {
                        corners.tl.r = (theme.get_corner_radius() * scale);
                    } else if (corner == "bl") {
                        corners.bl.r = (theme.get_corner_radius() * scale);
                    } else if (corner == "br") {
                        corners.br.r = (theme.get_corner_radius() * scale);
                    }
                }
                int height = std::max( { corner_radius, border_size.top,
                        border_size.bottom });
                auto create_s_and_t = [&](corner_texture_t& t, matrix<double> m, int r) {
                    for (auto a : { ACTIVE, INACTIVE }) {
                        t.surf[a] = theme.form_corner(a, r, m, height);
                        cairo_surface_upload_to_texture(t.surf[a], t.tex[a]);
                    }
                };
                /** The transformations are how we create 4 different corners */
                create_s_and_t(corners.tr, { scale, 0, 0, scale }, corners.tr.r);
                create_s_and_t(corners.tl, { -scale, 0, 0, scale }, corners.tl.r);
                create_s_and_t(corners.bl, { -scale, 0, 0, -scale }, corners.bl.r);
                create_s_and_t(corners.br, { scale, 0, 0, -scale }, corners.br.r);

                corners.tr.g = { size.width - corner_radius, 0,
                                 corner_radius, height };
                corners.tl.g = { 0, 0, corner_radius, height };
                corners.bl.g = { 0, size.height - height, corner_radius, height };
                corners.br.g = { size.width - corner_radius, size.height - height,
                                 corner_radius, height };

                edges.border.active    = colors.border.active;
                edges.border.inactive  = colors.border.inactive;
                edges.outline.active   = colors.outline.active;
                edges.outline.inactive = colors.outline.inactive;
                this->corner_radius    = corner_radius;
            }
        }

    public:
        border_size_t border_size;
        int corner_radius;

        template<typename T>
        T get_option(std::string theme, std::string option_name) {
            wf::config::config_manager_t& config = wf::get_core().config;
            auto option = config.get_option<std::string>(theme + "/" + option_name);
            if (option == nullptr || theme == "default") {
                return config.get_option<T>("firedecor/" + option_name)->get_value();
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
                get_option<int>(theme, "corner_radius"),

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
                get_option<bool>(theme, "debug_mode"),
                get_option<std::string>(theme, "round_on")
            };
            return options;
        }

        simple_decoration_node_t(wayfire_toplevel_view view)
            : node_t(false),
              theme{get_options("default")},
              layout{theme, [=, this] (wlr_box box) {
                  wf::scene::damage_node(shared_from_this(), box + get_offset()); }} {
            this->view = view;
            view->connect(&title_set);

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
                [=, this] (wf::scene::node_damage_signal *data)
                {
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
                if (!our_damage.empty())
                    {
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

                int h = std::max({ self->corner_radius, self->border_size.top, self->border_size.bottom });
                self->corners.tr.g = { self->size.width - self->corner_radius, 0, self->corner_radius, h };
                self->corners.bl.g = { 0, self->size.height - h, self->corner_radius, h };
                self->corners.br.g = { self->size.width - self->corner_radius,
                                       self->size.height - h, self->corner_radius, h };

                for (const auto& box : region)
                    {
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
            if (view->pending_fullscreen())
                {
                    return view->get_geometry(); // FIXME: was get_wm_geometry
                } else {
                return wf::construct_box(get_offset(), size);
            }
        }

        void render_title(const render_target_t& fb, geometry_t geometry,
                          geometry_t dots_geometry, edge_t edge, geometry_t scissor) {
            if (title_needs_update) {
                update_title(fb.scale);
            }

            simple_texture_t *texture, *dots_texture;
            uint32_t bits = 0;
            if (edge == EDGE_TOP || edge == EDGE_BOTTOM) {
                bits = OpenGL::TEXTURE_TRANSFORM_INVERT_Y;
                texture = &title.hor[view->activated];
                dots_texture = &title.hor_dots[view->activated];
            } else {
                texture = &title.ver[view->activated];
                dots_texture = &title.ver_dots[view->activated];
            }

            OpenGL::render_begin(fb);
            fb.logic_scissor(scissor);
            OpenGL::render_texture(texture->tex, fb, geometry, glm::vec4(1.0f), bits);
            if (title.too_big) {
                OpenGL::render_texture(dots_texture->tex, fb, dots_geometry,
                                       glm::vec4(1.0f), bits);
            }
            OpenGL::render_end();
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
                                    point_t rect, geometry_t scissor,
                                    std::string rounded, unsigned long i,
                                    decoration_area_type_t type, matrix<int> m,
                                    edge_t edge) {
            /** The view's origin */
            point_t o = { rect.x, rect.y };

            if (type == DECORATION_AREA_ACCENT) {
            } else {
                /**** Render a single rectangle when the area is a background */
                color_t color = (view->activated) ?
                    alpha_trans(theme.get_border_colors().active) :
                    alpha_trans(theme.get_border_colors().inactive);
                color_t o_color = (view->activated) ?
                    alpha_trans(theme.get_outline_colors().active) :
                    alpha_trans(theme.get_outline_colors().inactive);

                wf::geometry_t g_o;
                int o_s = theme.get_outline_size();
                if (edge == wf::firedecor::EDGE_TOP) {
                    g_o = { g.x, g.y, g.width, o_s };
                    g = { g.x, g.y + o_s, g.width, g.height - o_s };
                } else if (edge == wf::firedecor::EDGE_LEFT) {
                    // the g.y + is probably not correct, but works for now
                    g_o = { g.x, g.y, o_s, g.y + g.height };
                    g = { g.x + o_s, g.y, g.width - o_s, g.y + g.height };
                } else if (edge == wf::firedecor::EDGE_BOTTOM) {
                    g_o = { g.x, g.y + g.height - o_s, g.width, o_s };
                    g = { g.x, g.y, g.width, g.height - o_s };
                } else if (edge == wf::firedecor::EDGE_RIGHT) {
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
                /****/
            }
        }

        void render_background(const render_target_t& fb, geometry_t rect,
                               const geometry_t& scissor) {
            edge_colors_t colors = {
                theme.get_border_colors(), theme.get_outline_colors()
            };

            colors.border.active = alpha_trans(colors.border.active);
            colors.border.inactive = alpha_trans(colors.border.inactive);

            int r = theme.get_corner_radius() * fb.scale;
            update_corners(colors, r, fb.scale);

            /** Borders */
            unsigned long i = 0;
            point_t rect_o = { rect.x, rect.y };

            for (auto area : layout.get_background_areas()) {
                render_background_area(fb, area->get_geometry(), rect_o, scissor,
                                       area->get_corners(), i, area->get_type(),
                                       area->get_m(), area->get_edge());
                i++;
            }

            /*OpenGL::render_begin(fb);
            fb.logic_scissor(scissor);

            // Outlines
            bool a = view->activated;
            point_t o = { rect.x, rect.y };
            // Rendering all corners
            for (auto *c : { &corners.tr, &corners.tl, &corners.bl, &corners.br }) {
                OpenGL::render_texture(c->tex[a].tex, fb, c->g + o, glm::vec4(1.0f));
            }
            OpenGL::render_end();*/
        }

        void render_scissor_box(const render_target_t& fb, point_t origin,
                                const wlr_box& scissor) {
            /** Draw the background (corners and border) */
            wlr_box geometry{origin.x, origin.y, size.width, size.height};
            render_background(fb, geometry, scissor);

            auto renderables = layout.get_renderable_areas();
            for (auto item : renderables) {
                int32_t bits = 0;
                if (item->get_edge() == EDGE_LEFT) {
                    bits = OpenGL::TEXTURE_TRANSFORM_INVERT_Y;
                } else if (item->get_edge() == EDGE_RIGHT) {
                    bits = OpenGL::TEXTURE_TRANSFORM_INVERT_X;
                }
                if (item->get_type() == DECORATION_AREA_TITLE) {
                    render_title(fb, item->get_geometry() + origin,
                                 item->get_dots_geometry() + origin, item->get_edge(),
                                 scissor);
                } else if (item->get_type() == DECORATION_AREA_BUTTON) {
                    item->as_button().set_active(view->activated);
                    //item->as_button().set_maximized(view->tiled_edges);
                    item->as_button().render(fb, item->get_geometry() + origin, scissor);
                } else if (item->get_type() == DECORATION_AREA_ICON) {
                    render_icon(fb, item->get_geometry() + origin, scissor, bits);
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

        // FIXME
        bool accepts_input(int32_t sx, int32_t sy) {
            return pixman_region32_contains_point(cached_region.to_pixman(),
                                                  sx, sy, NULL);
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
            view->damage();
            size = dims;

            layout.resize(size.width, size.height, title.dims, title.dots_dims);
            if (!view->toplevel()->current().fullscreen) {
                this->cached_region = layout.calculate_region();
            }
            view->damage();
        }

        void update_decoration_size() {
            if (view->toplevel()->current().fullscreen) {
                border_size = { 0, 0, 0, 0 };
                this->cached_region.clear();
            } else {
                border_size = layout.parse_border(theme.get_border_size());
                this->cached_region = layout.calculate_region();
            }
        }
    };

    wf::firedecor::simple_decorator_t::simple_decorator_t(wayfire_toplevel_view view) {
        this->view = view;
        deco       = std::make_shared<simple_decoration_node_t>(view);
        deco->resize(wf::dimensions(view->get_pending_geometry()));
        wf::scene::add_back(view->get_surface_root_node(), deco);

        view->connect(&on_view_activated);
        view->connect(&on_view_geometry_changed);
        view->connect(&on_view_fullscreen);

        on_view_activated = [ this ] (auto) {
            wf::scene::damage_node( deco, deco->get_bounding_box());
        };

        on_view_geometry_changed = [ this ] (auto) {
            deco->resize(wf::dimensions( this->view->get_geometry()));
        };

        on_view_fullscreen = [ this ] (auto) {
            deco->update_decoration_size();
            if (!this->view->toplevel()->current().fullscreen) {
                deco->resize(wf::dimensions(this->view->get_geometry()));
            }
        };
    }

    wf::firedecor::simple_decorator_t::~simple_decorator_t() {
        wf::scene::remove_child( deco );
    }

    wf::decoration_margins_t wf::firedecor::simple_decorator_t::get_margins(const wf::toplevel_state_t& state) {
        return wf::decoration_margins_t {
            .left   = deco->border_size.left,
            .right  = deco->border_size.right,
            .bottom = deco->border_size.bottom,
            .top    = deco->border_size.top,
        };
    }

    // namespace
}
