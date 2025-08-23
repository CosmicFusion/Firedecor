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
//#include "cairo-util.hpp"
#include <wayfire/plugins/common/cairo-util.hpp>

#define INACTIVE 0
#define ACTIVE 1
#define FORCE true
#define DONT_FORCE false

#include <fstream>

namespace wf::cosmodecor {
wf::option_wrapper_t<bool> maximized_titlebar{"cosmodecor/maximized_titlebar"};

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


        // Corner Varibles
        struct corner_texture_t {
            simple_texture_t tex[2];
            cairo_surface_t *surf[2];
            geometry_t g;
            int r;
        };

        struct {
            corner_texture_t tr, tl, bl, br;
        } corners;

        // Edge variables
        struct edge_colors_t {
            color_set_t border, outline;
        } edges;

        /** Accent variables */
        struct accent_texture_t {
            simple_texture_t t_trbr[2];
            simple_texture_t t_tlbl[2];
            int radius;
        };

        std::vector<accent_texture_t> accent_textures;

        // Other general variables
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
                get_option<int>(theme, "corner_radius"),

                get_option<int>(theme, "outline_size"),
                get_option<wf::color_t>(theme, "active_outline"),
                get_option<wf::color_t>(theme, "inactive_outline"),

                get_option<int>(theme, "button_size"),
                get_option<std::string>(theme, "button_style"),

                get_option<int>(theme, "icon_size"),

                get_option<wf::color_t>(theme, "active_accent"),
                get_option<wf::color_t>(theme, "inactive_accent"),

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

    void form_accent_corners(int r, geometry_t accent, std::string corner_style,
                             matrix<int> m, edge_t edge) {
        if (auto view = _view.lock()) {
        const auto format = CAIRO_FORMAT_ARGB32;
        cairo_surface_t *surfaces[4];
        double angle = 0;

        /** Colors of the accent and background, respectively */
        color_t a_color;
        color_t b_color;

        wf::point_t a_origin = { accent.x, accent.y };

        int h = std::max({ corner_radius, border_size.top, border_size.bottom });

        wf::geometry_t a_edges[2];
        if (m.xx == 1) {
            a_edges[0] = { 0, 0, r, accent.height };
            a_edges[1] = { accent.width - r, 0, r, accent.height };
        } else {
            a_edges[0] = { 0, 0, accent.width, r };
            a_edges[1] = { 0, accent.height - r, accent.width, r };
        }

        wf::geometry_t cut;
        if (m.xy == 0) { 
            cut = { accent.x + r, accent.y, accent.width - 2 * r, accent.height };
        } else {
            cut = { accent.x, accent.y + r, accent.width, accent.height - 2 * r };
        }

        /**** Creation of the master path, containing all accent edge textures */
        const cairo_matrix_t matrix = {
            (double)m.xx, (double)m.xy, (double)m.yx, (double)m.yy, 0, 0
        };

        /** Array used to determine if a line starts on the corner or not */
        struct { int tr = 0, br = 0, bl = 0, tl = 0; } retract;

        /** Calculate where to retract, based on diagonality */
        for (int i = 0; auto c : corner_style) {
            if (c == '/') {
                if (i == 0) { 
                    retract.tl = r ;
                } else {
                    retract.br = r;
                }
                i++;
            } else if (c == '\\') {
                if (i == 0) { 
                    retract.bl = r ;
                } else {
                    retract.tr = r;
                }
                i++;
            } else if (c == '!') {
                i++;
            }
        }

        /** "Untransformed" accent area, used for correct transformations later on */
        const wf::dimensions_t mod_a = {
            abs(accent.width * m.xx + accent.height * m.xy),
            abs(accent.width * m.yx + accent.height * m.yy)
        };

        auto full_surface = cairo_image_surface_create(format, accent.width,
                                                       accent.height);
        auto cr = cairo_create(full_surface);

        /** A corner indexing array and the string with the final corners to round */
        std::string c_strs[4] = { "br", "tr", "tl", "bl" };
        std::string to_round;

        /** Deciding which corners to round, based on the string and on rotation */
        if (corner_style == "a") {
            to_round = "brtrtlbl";
            retract.br = r;
        } else {
            /** True mathematical modulo */
            auto modulo = [](int a, int b) -> int {
               return a - b * floor((double)a / b);
            };

            for (int c_to_rotate = 0; auto str : c_strs) {
               if (corner_style.find(str) != std::string::npos) {

                   /**
                    * This function effectively rotates the chosen corner.
                    * When m.xy == 1 (left edge), br becomes tr, tr becomes tl, etc.
                    * On the right edge, the opposite happens.
                    * This is to keep the correct corners rounded for the end user.
                    */
                   to_round += c_strs[modulo(c_to_rotate - m.xy, 4)];
                   if (modulo(c_to_rotate - m.xy, 4) == 0) { retract.br = r; }
               }
               c_to_rotate++;
            }
        }

        /** Point of rotation, in case it is needed */
        int rotation_x_d = ((m.xy == 1) ? mod_a.height : mod_a.width ) / 2;
        wf::point_t rotation_point = { rotation_x_d, rotation_x_d };

        /** Rotation depending on the edge */
        cairo_translate(cr, rotation_point);
        cairo_transform(cr, &matrix);
        cairo_translate(cr, -rotation_point);

        /** Lambda that creates a rounded or flat corner */
        auto create_corner = [&](int w, int h, int i) {
            if (to_round.find(c_strs[i]) != std::string::npos) {
                cairo_arc(cr, w + ((i < 2) ? -r : r), h + ((i % 3 == 0) ? r : -r), r,
                          M_PI_2 * (i - 1), M_PI_2 * i);
            } else {
                if (i % 2 == 0) { cairo_line_to(cr, w, h); }
                cairo_line_to(cr, w, h + ((i == 0) ? r : ((i == 2) ? -r : 0)));
            }
        };

        cairo_move_to(cr, mod_a.width - retract.br, 0);
        if (to_round.find("r") == std::string::npos) {
            cairo_line_to(cr, mod_a.width - retract.tr, mod_a.height);
        } else {
            create_corner(mod_a.width, 0, 0);
            create_corner(mod_a.width, mod_a.height, 1);
        }
        if (to_round.find("l") == std::string::npos) {
            cairo_line_to(cr, retract.tl, mod_a.height);
            cairo_line_to(cr, retract.bl, 0);
        } else {
            create_corner(0, mod_a.height, 2);
            create_corner(0, 0, 3);
        }
        cairo_close_path(cr);
        auto master_path = cairo_copy_path(cr);
        cairo_destroy(cr);
        cairo_surface_destroy(full_surface);
        /****/
        
        for (int i = 0, j = 0; i < 4; i++, angle += M_PI / 2, j = i % 2) {
            if (i < 2) {
                a_color = theme.get_accent_colors().inactive;
                b_color = theme.get_border_colors().inactive;
            } else {
                a_color = theme.get_accent_colors().active;
                b_color = theme.get_border_colors().active;
            }
            int width = a_edges[j].width;
            int height = a_edges[j].height;
                
            surfaces[i] = cairo_image_surface_create(format, width, height);
            auto cr_a = cairo_create(surfaces[i]);

            /** Background rectangle, behind the accent's corner */
            cairo_set_source_rgba(cr_a, b_color);
            cairo_rectangle(cr_a, 0, 0, a_edges[j].width, a_edges[j].height);
            cairo_fill(cr_a);

            /**** Outline, done early so it can also be cropped off */
            /** Translation to the correct rotation */
            cairo_translate(cr_a , rotation_point); 
            cairo_transform(cr_a , &matrix);
            cairo_translate(cr_a , -rotation_point); 

            int o_size = theme.get_outline_size();
            auto outline_color = (view->activated) ?
                                 alpha_trans(theme.get_outline_colors().active) :
                                 alpha_trans(theme.get_outline_colors().inactive);


            /** Draw outline on the bottom in case it is in the bottom edge */
            int h_offset = (edge == EDGE_BOTTOM) ? mod_a.height : o_size;

            cairo_set_source_rgba(cr_a, outline_color);
            cairo_rectangle(cr_a, 0, mod_a.height - h_offset, mod_a.width, o_size);
            cairo_fill(cr_a);
            /****/

            /** Dealing with intersection between the view's corners and accent */
            for (auto *c : { &corners.tr, &corners.tl, &corners.bl, &corners.br } ) {

                /** Accent edge translated by the accent's origin */
                wf::geometry_t a_edge = a_edges[j] + a_origin;

                /** Skip everything if the areas don't intersect */
                auto in = geometry_intersection(a_edge, c->g);
                if (in.width == 0 || in.height == 0) { continue; }

                /**** Rectangle to cut the background from the accent's corner */
                /** View's corner position relative to the accent's corner */
                point_t v_rel_a = { 
                    c->g.x - a_edge.x, c->g.y - a_edge.y
                };

                cairo_set_operator(cr_a, CAIRO_OPERATOR_CLEAR);
                cairo_rectangle(cr_a, v_rel_a, corner_radius, h);
                cairo_fill(cr_a);
                /****/

                /** Removal of intersecting areas from the view corner */
                for (auto active : { ACTIVE, INACTIVE }) {

                    /**** Removing the edges with cut, flat, or diagonal corners */
                    /** Surface to remove from, the view corner in this case */
                    auto cr_v = cairo_create(c->surf[active]);
                    cairo_set_operator(cr_v, CAIRO_OPERATOR_CLEAR);

                    /** Transformed br accent corner, relative to the view corner */
                    wf::point_t t_br_rel_v = {
                        (a_origin.x) - c->g.x,
                        (c->g.y + c->g.height) - (a_origin.y + accent.height)
                    };

                    cairo_translate(cr_v, t_br_rel_v.x, t_br_rel_v.y);

                    cairo_translate(cr_v , rotation_point); 
                    cairo_transform(cr_v , &matrix);
                    cairo_translate(cr_v , -rotation_point); 

                    cairo_append_path(cr_v, master_path);
                    cairo_fill(cr);
                    /****/

                    /**** Clear the view's corner with the accent rectangles */
                    /** Bottom of the rectangle relative to the view's corner */
                    wf::point_t reb_rel_v;
                    reb_rel_v.y = (c->g.y + c->g.height) - (cut.y + cut.height);
                    reb_rel_v.x = (cut.x - c->g.x);

                    cairo_set_operator(cr_v, CAIRO_OPERATOR_CLEAR);
                    cairo_rectangle(cr_v, reb_rel_v, cut.width, cut.height);
                    cairo_fill(cr_v);
                    /****/
                            
                    cairo_surface_upload_to_texture(c->surf[active], c->tex[active]);
                    cairo_destroy(cr_v);
                }
            }

            /**** Final drawing of accent corner, overlaying the drawn rectangle */
            cairo_set_operator(cr_a, CAIRO_OPERATOR_SOURCE);

            wf::point_t t_br = {
                (r - mod_a.width) * (m.xx * (i % 2) + m.xy * (1 - i % 2)), 0
            };

            cairo_set_source_rgba(cr_a, a_color);
            cairo_translate(cr_a, t_br.x, t_br.y);
            cairo_append_path(cr_a, master_path);
            cairo_fill(cr_a);
            cairo_destroy(cr_a);
            /****/
        }
        auto& texture = accent_textures.back();
        cairo_surface_upload_to_texture(surfaces[0], texture.t_trbr[INACTIVE]);
        cairo_surface_upload_to_texture(surfaces[1], texture.t_tlbl[INACTIVE]);
        cairo_surface_upload_to_texture(surfaces[2], texture.t_trbr[ACTIVE]);
        cairo_surface_upload_to_texture(surfaces[3], texture.t_tlbl[ACTIVE]);
        texture.radius = r;

        for (auto surface : surfaces) { cairo_surface_destroy(surface); }
        cairo_path_destroy(master_path);
        }
    }

        void render_background_area(const render_target_t& fb, geometry_t g,
                                point_t rect, geometry_t scissor,
                                std::string rounded, unsigned long i,
                                decoration_area_type_t type, matrix<int> m,
                                edge_t edge) {
            if (auto view = _view.lock()) {
                // The view's origin
                point_t o = { rect.x, rect.y };

                /**** Render the corners of an accent */
                int r;

                /** Create the corners, it should happen once per accent */
                if (accent_textures.size() <= i) {
                    accent_textures.resize(i + 1);
                    r = std::min({ ceil((double)g.height / 2), ceil((double)g.width / 2),
                                (double)corner_radius});
                    form_accent_corners(r, g, rounded, m, edge);
                }

                r = accent_textures.at(i).radius;

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
                /** Outlines */
                bool a = view->activated;
                /** Rendering all corners */
                for (auto *c : { &corners.tr, &corners.tl, &corners.bl, &corners.br }) {
                    OpenGL::render_texture(c->tex[a].tex, fb, c->g + o, glm::vec4(1.0f));
                }
                
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

            int r = theme.get_corner_radius() * fb.scale;
            update_corners(colors, r, fb.scale);

            // Borders
            unsigned long i = 0;
            point_t rect_o = { rect.x, rect.y };

            for (auto area : layout.get_background_areas()) {
                render_background_area(fb, area->get_geometry(), rect_o, scissor, area->get_corners(), i, area->get_type(),
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
            if (!this->view->toplevel()->current().fullscreen || !this->view->toplevel()->current().tiled_edges) {
                deco->resize(wf::dimensions(this->view->get_geometry()));
            }
        };
    }

    wf::cosmodecor::simple_decorator_t::~simple_decorator_t() {
        wf::scene::remove_child( deco );
    }

    wf::decoration_margins_t wf::cosmodecor::simple_decorator_t::get_margins(const wf::toplevel_state_t& state) {
        if (state.fullscreen || (!maximized_titlebar && state.tiled_edges)) {
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
