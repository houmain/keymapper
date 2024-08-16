
const isKDE6 = typeof workspace.windowList === 'function';

let connectWindowActivated;
if (isKDE6) {
    connectWindowActivated = (handler) => workspace.windowActivated.connect(handler);
} else {
    connectWindowActivated = (handler) => workspace.clientActivated.connect(handler);
}

connectWindowActivated(function(client){
    callDBus(
        "com.github.houmain.Keymapper",
        "/com/github/houmain/Keymapper",
        "com.github.houmain.Keymapper",
        "WindowFocus",
        "caption" in client ? client.caption : "",
        "resourceClass" in client ? client.resourceClass : ""
    );
});
