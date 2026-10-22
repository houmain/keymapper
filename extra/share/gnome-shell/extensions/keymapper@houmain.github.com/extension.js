import Shell from 'gi://Shell';
import Gio from 'gi://Gio';

const KeymapperInterface = '<node>\
<interface name="com.github.houmain.Keymapper"> \
  <method name="WindowFocus"> \
    <arg name="title" type="s" direction="in"/> \
    <arg name="class" type="s" direction="in"/> \
  </method> \
</interface> \
</node>';

export default class KeymapperExtension {
  enable() {
    const KeymapperProxy = Gio.DBusProxy.makeProxyWrapper(KeymapperInterface);

    this._keymapper = new KeymapperProxy(
      Gio.DBus.session,
      "com.github.houmain.Keymapper",
      "/com/github/houmain/Keymapper",
      null,
      null,
      Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES |
        Gio.DBusProxyFlags.DO_NOT_CONNECT_SIGNALS |
        Gio.DBusProxyFlags.DO_NOT_AUTO_START
    );

    this._windowTracker = Shell.WindowTracker.get_default();
    this._focusAppChangedId = this._windowTracker.connect('notify::focus-app', () => {
      const window = global.display.focus_window;
      this._keymapper.WindowFocusRemote(
        (window ? window.get_title() : ''),
        (window ? window.get_wm_class() : "root"));
    });
  }

  disable() {
    if (this._focusAppChangedId) {
      this._windowTracker.disconnect(this._focusAppChangedId);
      this._focusAppChangedId = null;
    }

    this._windowTracker = null;
    this._keymapper = null;
  }
}

