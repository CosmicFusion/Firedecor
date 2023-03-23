#include <wayfire/per-output-plugin.hpp>
#include <wayfire/view.hpp>
#include <wayfire/matcher.hpp>
#include <wayfire/workspace-manager.hpp>
#include <wayfire/output.hpp>
#include <wayfire/signal-definitions.hpp>

#include "firedecor-subsurface.hpp"

#include <stdio.h>

class wayfire_firedecor_t : public wf::plugin_interface_t, private wf::per_output_tracker_mixin_t<>
{
    wf::view_matcher_t ignore_views{"firedecor/ignore_views"};
    wf::option_wrapper_t<std::string> extra_themes{"firedecor/extra_themes"};
    wf::config::config_manager_t& config = wf::get_core().config;

    wf::signal::connection_t<wf::view_mapped_signal> view_mapped = [=] (wf::view_mapped_signal *ev) {
        fprintf(stderr, "FIREDECOR: view_mapped()\n");
        update_view_decoration(ev->view);
    };

    wf::signal::connection_t<wf::view_decoration_changed_signal> view_changed = [=] (wf::view_decoration_changed_signal *ev) {
        fprintf(stderr, "FIREDECOR: view_decoration_changed()\n");
        update_view_decoration(ev->view);
    };

    wf::signal::connection_t<wf::view_decoration_state_updated_signal> view_updated = [=] (wf::view_decoration_state_updated_signal *ev) {
        fprintf(stderr, "FIREDECOR: view_decoration_state_updated()\n");
        update_view_decoration(ev->view);
    };

  public:

    void init() override {
        fprintf(stderr, "FIREDECOR: init()\n");
        this->init_output_tracking();
    }

    void handle_new_output(wf::output_t *output) override {
        fprintf(stderr, "FIREDECOR: handle_new_output()\n");

        // subscribe to updates about new views to decorate
        output->connect(&view_mapped);
        output->connect(&view_changed);
        output->connect(&view_updated);
        for (auto& view : output->workspace->get_views_in_layer(wf::ALL_LAYERS)) {
            update_view_decoration(view);
        }
    }

    void handle_output_removed(wf::output_t *output) override
    {
        for (auto& view : output->workspace->get_views_in_layer(wf::ALL_LAYERS))
        {
            wf::firedecor::deinit_view(view);
        }
    }

    wf::wl_idle_call idle_deactivate;

    template<typename T>
    T get_option(std::string theme, std::string option_name) {

        auto option = config.get_option<std::string>(theme + "/" + option_name);
        if (option == nullptr || theme == "invalid") {
            return config.get_option<T>("firedecor/" + option_name)->get_value();
        } else {
            return wf::option_type::from_string<T>(option->get_value()).value();
        }
    }

    wf::firedecor::theme_options get_options(std::string theme) {
        wf::firedecor::theme_options options = {
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

    void update_view_decoration(wayfire_view view) {
        fprintf(stderr, "FIREDECOR: update_view_decoration()\n");
        auto box = view->get_bounding_box();
        fprintf(stderr, "  @%d %d  %d x %d\n", box.x, box.y, box.width, box.height);
        if (view->should_be_decorated() && !ignore_views.matches(view)) {
            std::stringstream themes{extra_themes.value()};
            std::string theme;
            while (themes >> theme) {
                fprintf(stderr, "  theme: %s\n", theme.c_str());
                try {
                    wf::view_matcher_t matcher{theme + "/uses_if"};
                    if (matcher.matches(view)) {
                        fprintf(stderr, "  matcher.matches\n");
                        wf::firedecor::init_view(view, get_options(theme));
                        return;
                    }
                } catch (...) {
                    fprintf(stderr, "  exception\n");
                }
            }
            wf::firedecor::init_view(view, get_options("invalid"));
        } else {
            fprintf(stderr, "  should be decorated: %d  ignored: %d\n", view->should_be_decorated(), ignore_views.matches(view));
            wf::firedecor::deinit_view(view);
        }
    }

    void fini() override {
        for (auto view : wf::get_core().get_all_views()) {
            wf::firedecor::deinit_view(view);
        }
    }
};

DECLARE_WAYFIRE_PLUGIN(wayfire_firedecor_t);
