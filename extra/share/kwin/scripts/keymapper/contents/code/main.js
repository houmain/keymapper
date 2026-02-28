
const isKDE6 = typeof workspace.windowList === 'function';

const connectWindowActivated = isKDE6
    ? (handler) => workspace.windowActivated.connect(handler)
    : (handler) => workspace.clientActivated.connect(handler);

let activeWindow = null;

function sendWindowFocusDBus() {
    callDBus(
        "com.github.houmain.Keymapper",
        "/com/github/houmain/Keymapper",
        "com.github.houmain.Keymapper",
        "WindowFocus",
        activeWindow?.caption ?? "",
        activeWindow?.resourceClass ?? ""
    );
}

connectWindowActivated((window) => {
    if (activeWindow) {
        activeWindow.captionChanged.disconnect(sendWindowFocusDBus);
        activeWindow.windowClassChanged.disconnect(sendWindowFocusDBus);
    }
    activeWindow = window;
    if (activeWindow) {
        activeWindow.captionChanged.connect(sendWindowFocusDBus);
        activeWindow.windowClassChanged.connect(sendWindowFocusDBus);
    }
    sendWindowFocusDBus();
});
