
#if defined ENABLE_COCOA

#include "TrayIcon.h"
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

public:
  ~TrayIconCocoa() {
    if (mStatusItem)
      [[NSStatusBar systemStatusBar] removeStatusItem:mStatusItem];
  }

  bool initialize(TrayIcon::Handler* handler, bool show_reload) override {
    const auto app = [NSApplication sharedApplication];
    const auto delegate = [[TrayAppDelegate alloc] initWithHandler:handler];
    [app setDelegate:delegate];

    const auto statusBar = [NSStatusBar systemStatusBar];
    mStatusItem = [statusBar statusItemWithLength:NSVariableStatusItemLength];
    [mStatusItem.button setToolTip:@"keymapper"];

    // TODO: load own icon
    const auto iconPath = @"/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/SidebarLaptop.icns";
    const auto icon = [[NSImage alloc] initWithContentsOfFile:iconPath];
    if (icon) {
      [icon setSize:NSMakeSize(20, 20)];
      [mStatusItem.button setImage:icon];
    }
    else {
      [mStatusItem.button setTitle:@"KEY"]; 
    }
    auto menu = [[NSMenu alloc] init];
    
    auto header = [[NSMenuItem alloc] init];
    auto attributes = @{NSFontAttributeName: [NSFont boldSystemFontOfSize:[NSFont systemFontSize]]};
    header.attributedTitle = [[NSAttributedString alloc] initWithString:@"keymapper" attributes:attributes];
    header.enabled = NO;
    [menu addItem:header];
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
};

std::unique_ptr<TrayIcon::IImpl> make_tray_icon_cocoa() {
  return std::make_unique<TrayIconCocoa>();
}

#endif // ENABLE_COCOA
