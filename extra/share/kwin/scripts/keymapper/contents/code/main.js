workspace.clientActivated.connect(function(client){
    callDBus(
        "com.github.houmain.Keymapper",
        "/com/github/houmain/Keymapper",
        "com.github.houmain.Keymapper",
        "WindowFocus",
        "caption" in client ? client.caption : "",
        "resourceClass" in client ? client.resourceClass : ""
    );
});