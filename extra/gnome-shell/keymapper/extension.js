const Shell = imports.gi.Shell;
const Gio = imports.gi.Gio;

const KeymapperInterface = '<node>\
<interface name="com.github.houmain.Keymapper"> \
  <method name="WindowFocus"> \
    <arg name="title" type="s" direction="in"/> \
    <arg name="class" type="s" direction="in"/> \
  </method> \
</interface> \
</node>';

const KeymapperProxy = Gio.DBusProxy.makeProxyWrapper(KeymapperInterface);

const keymapper = new KeymapperProxy(
  Gio.DBus.session,
  "com.github.houmain.Keymapper",
  "/com/github/houmain/Keymapper"
);

let enabled = false;

function init() {
  Shell.WindowTracker.get_default().connect('notify::focus-app', () => {
    const window = global.display.focus_window;
    if (enabled)
      keymapper.WindowFocusSync(
        (window ? window.get_title() : ''),
        (window ? window.get_wm_class() : "root"));
  });
  return {
    enable: ()=>{ enabled = true; },
    disable: ()=>{ enabled = false; }
  };  
}
