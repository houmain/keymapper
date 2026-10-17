
#if defined ENABLE_COCOA

#include "TrayIcon.h"
#include "common/icon_svg.h"
#include "common/icon_inactive_svg.h"
#import <Cocoa/Cocoa.h>

@interface TrayAppDelegate : NSObject <NSApplicationDelegate>
@property TrayIcon::Handler *handler;
@end

@implementation TrayAppDelegate

- (instancetype)initWithHandler:(TrayIcon::Handler *)handler {
  self = [super init];
  self.handler = handler;
  return self;
}

- (void)onActiveClicked:(id)sender {
  if ([sender state] == NSControlStateValueOn) {
    [sender setState:NSControlStateValueOff];
  }
  else {
    [sender setState:NSControlStateValueOn];
  }
  self.handler->on_toggle_active();
}

- (void)onConfigurationClicked:(id)sender {
  self.handler->on_open_config();
}

- (void)onReloadClicked:(id)sender {
  self.handler->on_reload_config();
}

- (void)onNextKeyInfoClicked:(id)sender {
  self.handler->on_request_next_key_info();
}

- (void)onHelpClicked:(id)sender {
  self.handler->on_open_help();
}

- (void)onAboutClicked:(id)sender {
  self.handler->on_open_about();
}

- (void)onExitClicked:(id)sender {
  self.handler->on_exit();
}

@end

class TrayIconCocoa : public TrayIcon::IImpl {
private:
  NSStatusItem* mStatusItem{ };
  NSImage* mActiveIcon{ };
  NSImage* mInactiveIcon{ };

public:
  ~TrayIconCocoa() {
    if (mStatusItem)
      [[NSStatusBar systemStatusBar] removeStatusItem:mStatusItem];
  }

  bool initialize(TrayIcon::Handler* handler, bool show_reload,
                  const std::filesystem::path& icon_filename,
                  const std::filesystem::path& inactive_icon_filename) override {
    const auto app = [NSApplication sharedApplication];
    const auto delegate = [[TrayAppDelegate alloc] initWithHandler:handler];
    [app setDelegate:delegate];

    const auto statusBar = [NSStatusBar systemStatusBar];
    mStatusItem = [statusBar statusItemWithLength:NSVariableStatusItemLength];
    [mStatusItem.button setToolTip:@"keymapper"];

    const auto load_icon = [](const std::filesystem::path& filename) -> NSImage* {
      if (filename.empty())
        return nil;
      const auto string = filename.string();
      return [[NSImage alloc] initWithContentsOfFile:
        [NSString stringWithUTF8String:string.c_str()]];
    };
    mActiveIcon = load_icon(icon_filename);
    if (!mActiveIcon) {
      const auto iconData = [NSData dataWithBytes:icon_svg length:icon_svg_len];
      mActiveIcon = [[NSImage alloc] initWithData:iconData];
    }
    mInactiveIcon = load_icon(inactive_icon_filename);
    if (!mInactiveIcon) {
      const auto iconData = [NSData dataWithBytes:icon_inactive_svg
                                             length:icon_inactive_svg_len];
      mInactiveIcon = [[NSImage alloc] initWithData:iconData];
    }
    if (mActiveIcon) {
      [mActiveIcon setSize:NSMakeSize(20, 20)];
      [mInactiveIcon setSize:NSMakeSize(20, 20)];
      [mStatusItem.button setImage:mActiveIcon];
    }
    else {
      [mStatusItem.button setTitle:@"KEY"]; 
    }
    auto menu = [[NSMenu alloc] init];
    
    [menu addItem:[NSMenuItem separatorItem]];
    const auto itemActive = [menu addItemWithTitle:@"Active" action:@selector(onActiveClicked:) keyEquivalent:@"a"];
    [itemActive setState:NSControlStateValueOn];
    [menu addItemWithTitle:@"Configuration" action:@selector(onConfigurationClicked:) keyEquivalent:@"c"];
    if (show_reload)
      [menu addItemWithTitle:@"Reload" action:@selector(onReloadClicked:) keyEquivalent:@"r"];
    [menu addItemWithTitle:@"Next Key Info" action:@selector(onNextKeyInfoClicked:) keyEquivalent:@"k"];
    [menu addItemWithTitle:@"Help" action:@selector(onHelpClicked:) keyEquivalent:@""];
    [menu addItemWithTitle:@"About" action:@selector(onAboutClicked:) keyEquivalent:@""];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItemWithTitle:@"Exit" action:@selector(onExitClicked:) keyEquivalent:@"x"];
    
    [mStatusItem setMenu:menu];
    return true;
  }

  void update() override {
    @autoreleasepool {
      for (;;) {
        auto event = [NSApp nextEventMatchingMask:NSEventMaskAny
                     untilDate:[NSDate distantPast]
                     inMode:NSDefaultRunLoopMode
                     dequeue:YES];
        if (event == nil)
          break;

        [NSApp sendEvent:event];
      }
    }
  }

  void on_active_toggled(bool active) override {
    [mStatusItem.button setImage:(active ? mActiveIcon : mInactiveIcon)];
  }
};

std::unique_ptr<TrayIcon::IImpl> make_tray_icon_cocoa() {
  return std::make_unique<TrayIconCocoa>();
}

#endif // ENABLE_COCOA
