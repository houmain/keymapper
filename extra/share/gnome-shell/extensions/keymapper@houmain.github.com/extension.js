import Shell from 'gi://Shell';
import Gio from 'gi://Gio';

export default class KeymapperExtension {
  constructor() {
    const KeymapperInterface = '<node>\
    <interface name="com.github.houmain.Keymapper"> \
      <method name="WindowFocus"> \
        <arg name="title" type="s" direction="in"/> \
        <arg name="class" type="s" direction="in"/> \
      </method> \
    </interface> \
    </node>';

    const KeymapperProxy = Gio.DBusProxy.makeProxyWrapper(KeymapperInterface);

    this._keymapper = new KeymapperProxy(
      Gio.DBus.session,
      "com.github.houmain.Keymapper",
      "/com/github/houmain/Keymapper"
    );

    Shell.WindowTracker.get_default().connect('notify::focus-app', () => {
      const window = global.display.focus_window;
      if (this._enabled)
        this._keymapper.WindowFocusSync(
          (window ? window.get_title() : ''),
          (window ? window.get_wm_class() : "root"));
    });
  }

  enable() {
    this._enabled = true;
  }

  disable() {
    this._enabled = false;
  }
}

