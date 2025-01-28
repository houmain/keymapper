
#if defined ENABLE_COCOA

// mostly generated using ChatGPT

#import <Cocoa/Cocoa.h>

static const int fontSize = 13;
static const int width = 400;

@interface MessageBoxCocoa : NSWindow
                             @property (nonatomic, strong) NSButton *okButton;
@property (nonatomic, strong) NSTextView *textView;

- (instancetype)initWithMessage:(NSString *)message title:(NSString *)title;
- (void)show;
- (void)closeWindow:(id)sender;
@end

@implementation MessageBoxCocoa

- (instancetype)initWithMessage:(NSString *)message title:(NSString *)title {
  NSRect frame = NSMakeRect(100, 100, width + 20, 150);
  self = [super initWithContentRect:frame
         styleMask:(NSWindowStyleMaskTitled |
                    NSWindowStyleMaskClosable)
         backing:NSBackingStoreBuffered
         defer:NO];
  if (self) {
    [self setTitle:title];
    [self setLevel:NSFloatingWindowLevel];

    _textView = [[NSTextView alloc] initWithFrame:NSMakeRect(10, 50, width, 50)];
    [_textView setString:message];
    [_textView setEditable:NO];
    [_textView setSelectable:YES];
    [_textView setFont:[NSFont systemFontOfSize:fontSize]];
    [_textView setBackgroundColor:[NSColor windowBackgroundColor]];
    [_textView setTextColor:[NSColor textColor]];
    [[self contentView] addSubview:_textView];

    // Calculate the required size for the window based on the message
    NSSize textSize = [self calculateTextSizeForMessage:message];
    [_textView setFrameSize:NSMakeSize(width, textSize.height)];

    NSRect newFrame = self.frame;
    newFrame.size.height = textSize.height + 90;
    [self setFrame:newFrame display:YES];

    _okButton = [[NSButton alloc] initWithFrame:NSMakeRect(100, 10, 100, 30)];
    [_okButton setTitle:@"OK"];
    [_okButton setTarget:self];
    [_okButton setAction:@selector(closeWindow:)];
    [[self contentView] addSubview:_okButton];
  }
  return self;
}

- (NSSize)calculateTextSizeForMessage:(NSString *)message {
  NSDictionary *attributes = @{NSFontAttributeName: [NSFont systemFontOfSize:fontSize]};
  NSRect textRect = [message boundingRectWithSize:NSMakeSize(width, CGFLOAT_MAX)
                    options:NSStringDrawingUsesLineFragmentOrigin
                    attributes:attributes];
  return textRect.size;
}

- (void)show {
  [self makeKeyAndOrderFront:nil];
  [self center];
}

- (void)closeWindow:(id)sender {
  [self close];
}

@end

void showMessageBoxCocoa(const char* title, const char* message) {
  NSString *nsMessage = [NSString stringWithUTF8String:message];
  NSString *nsTitle = [NSString stringWithUTF8String:title];
  MessageBoxCocoa *messageBox = [[MessageBoxCocoa alloc] initWithMessage:nsMessage title:nsTitle];
  [messageBox show];
}

#endif // ENABLE_COCOA
