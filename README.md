# CosmoDecor

CosmoDecor window decoration plugin for the Wayfire window manager. The original code lives at: https://github.com/mntmn/Firedecor

## Dependencies

- `wayfire`
- `librsvg`
- `boost`

## Installation

- Building from source:
  ```
  git clone https://github.com/cosmicfusion/CosmoDecor
  cd CosmoDecor
  meson build
  meson compile -C build
  sudo meson install -C build
  ```

## Configuration

<details><summary>Font options</summary>

- `font` will set what font will be used for titles. Default is `sans-serif`;
- `font_size` will set the font size, in pixels, for the title. Default is `21`;
- `active_title` will set the color for the font of active windows. Default is `\#1d1f21ff`;
- `inactive_title` will set the color for the font of inactive windows. Default is `\#1d1f21ff`;

</details>

<details><summary>Border options</summary>

- `border_size` can take up to 4 parameters. If one is used, it will be the border size for every edge of the windows. If 2 parameters are used, the first one determines the size of the top edge, and the second one determines the size of the remaining edges. If 3 are used, the first one will be used for all edges. If 4 parameters are used, they are used in the following order: top, left, bottom, right. Default is `30 10`;
- `active_border` will set the color for the border of active windows. Default is `#1d1f21e6`;
- `inactive_border` will set the color for the border of inactive windows. Default is `#1d1f21e6`;

</details>

<details><summary>Outline options</summary>

- `outline_size` will set the size for the outline of the window. Default is 0;
- `active_outline` will set the color for the outline of active windows. Default is `#000000ff`;
- `inactive_outline` will set the color for the outline of inactive windows. Default is `#000000ff`;

</details>

<details><summary>Button options</summary>

- `button_size` will set the size of the buttons, in pixels. Default is 20;
- `button_style` is a string that sets the style used for the buttons. By default, there are these styles:
  - `simple`, where the buttons have no symbols inside of them, they are simple circles.
  - `reform`, minimalistic — □ ⨯ buttons.
  - other styles will be removed.

  If you place anything else on this string, say, something like `my_theme`, you will have to provide `png`s or `svg`s so that the plugin can draw custom buttons. To accomplish that, do the following:
  1. Create the folder `/usr/share/cosmodecor/button-styles/`;
  2. In it, create a folder with the name `my_theme`;
  3. Place figures for the buttons. They'll have to be called something like `type-status.png`, where `type` can be `close`, `minimize`, or `toggle-maximize`, and `status` can be `hovered`, `pressed`, or nothing. E.g. close.png, toggle-maximize-hover.png, minimize-pressed.png. Additionally, if `inactive_buttons` is set to `true`, you have to add a additional images with the `status` of `inactive`. You **Must** provide an image for each of the `type`s and `status`es listed above, so 9 images if `inactive_buttons == false`, and 12 images if `inactive_buttons == true`. The images can be equal to each other, if you don't want do differentiate between different `type`s or `status`es, just make sure that every entry is placed.
 - `normal_min`, `normal_max`, and `normal_close` set their respective button colors when the button isn't hovered. Default values are `#c89e2bff`, `#2ebb3aff`, and `#c24045ff`, respectively.
 - `hovered_min`, `hovered_max`, and `hovered_close` set their respective button colors when the button is hovered. Default values are `#ffe450ff`, `#60fc79ff`, and `#ff6572ff`, respectively.
 - `inactive_buttons` is a `bool` that tells the plugin to draw buttons differently, depending on them being in an active or an inactive window. Default is `false`; The default is `wayfire`;

</details>

<details><summary>Icon options</summary>

- `icon_size` sets the size for the icons, in pixels. Default is `20`;
- `icon_theme` sets the theme to be used for the icons, make sure that a folder exists on an appropriate position. Default is `hicolor`;

</details>

<details><summary>Layout options</summary>

- `layout` is a long string that determines where things should be placed on the edges of a window. Here's how it works:
  - Every symbol must be separated by a space;
  - The symbols `title`, `icon`, `maximize`, `minimize`, and `close`, will place their respective symbols on the window;
  - The symbol `p` will introduce a standardized padding, set by the `padding_size` option. The symbol `P` followed by a number, will place that many pixels of padding, for example, `P7` places 7 pixels of padding on the edge;
  - The symbol `|` changes where the symbols are being placed. Normally, they're on the left of the edge, if you place a `|`, they will be on the center, if you place another `|`, they will be placed on the right. Further `|`s will not change position;
  - The symbol `-` will change the edge the symbols are being placed in. By default, it will be the top edge, and every `-` will change the edge, counter-clockwise. In previous versions of `wayfire-cosmodecor`, you needed to end the layout with `-`, that is no longer the case.
  - The symbol `a` will initiate/end an accented area, it will start one if there wasn't one already, and it will end one if there was. You can more precisely position accents by using paddings, for example `a P5 title P5 a` will place a padding between each end of the accent, giving some space for the title. All corners will be rounded with this option.
  - The symbol `A` is much like `a`, but it is followed by a spaceless string, which tells the program what should be done to the edges of the accent. The default behaviour is to create 2 flat edges, and the available options are:
    - Any of `br tr tl bl` will round the respective corner (`t`op and `b`ottom `l`eft and `r`ight). These can be placed in any order, e.g. `Abltr` will round the top right and bottom left corners.
    - `/` and `\\` (must be 2 backslashes) will create a diagonal ending on the respective edge. For example, `A\\/` will create a diagonal that looks like \ on the left edge, and / on the right. This is positioned in relation to the text direction, specifically, they rotate based on the edge they're on. This option will not work if one of the corners on a respective edge is rounded, e.g. `Atr//` will only diagonalize the left edge.
    - `!` is a flat edge. This is just used to skip diagonalization of the left edge, for example, `A!\\` will diagonalize the right edge but keep the left edge flat.

  The default layout is `P5 title | | minimize p maximize p close P5 -`. Here's what this means:
  1. Place a padding with 5 pixels of size, followed by title on the left;
  2. Move to the center, do nothing;
  3. Move to the right;
  4. Place a minimize button, followed by a toggle maximize button and a close button, all separated by a standardized padding;
  5. Place a padding with 5 pixels of size;
  6. Finish the top edge and move on to the left edge, do nothing there;

- `padding_size` determines the size used for `p` on `layout`. Default is `2`;

</details>

<details><summary>Other options</summary>

- `ignore_views` is of `criteria` type, and determines witch windows will be ignored for decorations. In the future, I plan on adding the ability to create multiple themes and use them selectively, for example, a light and dark theme.
- `debug_mode` turns the titles of windows into their respective `app_id`s, followed by the maximum pixel size of the current font, which often differs from the `font_size`. This is used when the plugin fails at finding the icon for an app, or if you want more precision in the positioning of the decorations. More in [App Icon Debugging](#app-icon-debugging). Default is `false`;
- `round_on` chooses which corners will be rounded. `tr` means top right, `tl` is top left, `bl` is bottom left, `br` is bottom right, and `all` is all of them, e.g. `tl br` will round the top left and bottom right corners. Default is `all`;

</details>

<details><summary>Extra theme options</summary>

- `extra_themes` will be the declaration of existance for any extra themes you want to use, e.g. `dark light discord`. If the theme is not in here, no windows will use it. The default is ``;
- When it comes to extra themes, the configuration section will look exactly like the regular `cosmodecor` section, except you won't have the `ignore_views` and `extra_themes` options, and will gain the `uses_if` option;
- `uses_if` is of `criteria` type, and will match all the windows that should use the theme of the current section. There is no default, so if it is not present, no window will use the theme;
- When declaring new themes, you don't need to use every single option on the list. If the option isn't present, the theme will simply use the value from the default `cosmodecor` theme section, so something like:
  ```ini
  [cosmodecor]
  border_size = 10 10 10 10

  title_color = 0.0 0.0 0.0 1.0

  extra_themes = white_title

  [white_title]
  uses_if = app_id is "kitty"

  title_color = 1.0 1.0 1.0 1.0
  ```
  Will change the `title_color` on views with `app_id is "kitty"`, but the `border_size` will stay at `10 10 10 10`.

</details>

## App Icon Debugging
The plugin will automatically try to retrieve icons from the file system, in order to display them on `icon` symbols on your windows. It will first look for folders matching your `icon_theme`. If it doesn't find the icons there, it will look in the remaining folders (hicolor, adwaita, breeze, in that order). However, sometimes, it just fails, and even if there is an icon for said app, the app's `app_id` is too terrible to find a suitable image, e.g. Osu!lazer has an `app_id` of "dotnet", which is completely unusable.
If this ends up happening, the plugin will use a backup icon, provided by the plugin itself. But you also have the ability to manually set icons for your apps. Here's how:
1. Set `debug_mode` to true;
2. Open your app, this should tell you what its `app_id` is, if you have a `title` in `layout`;
3. Find the icon for this app, it can be anywhere in the computer, and can be either a `png` or an `svg` file;
4. Find the file `~/.local/share/cosmodecor_icons`, it should be automatically created by the plugin;
5. Find the line containing the `app_id`, it should look like `my_app_id /full/path/to/default/icon`;
6. Replace the path in that line with the one you found earlier;
7. Done!
