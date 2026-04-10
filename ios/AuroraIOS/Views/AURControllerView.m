#import "AURControllerView.h"

@interface AURControllerView ()
@property (nonatomic, strong) UIImageView *backgroundImageView;
@property (nonatomic, strong) NSMutableDictionary<NSString *, UIButton *> *buttons;
@end

@implementation AURControllerView

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.backgroundImageView = [[UIImageView alloc] initWithFrame:self.bounds];
        self.backgroundImageView.contentMode = UIViewContentModeScaleAspectFit;
        [self addSubview:self.backgroundImageView];
        self.buttons = [NSMutableDictionary dictionary];
    }
    return self;
}

- (void)layoutSubviews {
    [super layoutSubviews];
    self.backgroundImageView.frame = self.bounds;
}

- (void)applySkin:(AURControllerSkin *)skin {
    self.backgroundImageView.image = skin.backgroundImage;

    // Remove old buttons
    for (UIButton *button in self.buttons.allValues) {
        [button removeFromSuperview];
    }
    [self.buttons removeAllObjects];

    // Add new buttons based on skin rects
    [skin.buttonRects enumerateKeysAndObjectsUsingBlock:^(NSString *keyStr, NSValue *rectValue, BOOL *stop) {
        CGRect rect = [rectValue CGRectValue];

        // Convert normalized coordinates to actual frame (assuming 375x812 base for skin design)
        CGFloat scaleX = self.bounds.size.width / 375.0;
        CGFloat scaleY = self.bounds.size.height / 500.0; // Assume controller area is roughly 500pt high
        CGRect scaledRect = CGRectMake(rect.origin.x * scaleX, rect.origin.y * scaleY, rect.size.width * scaleX, rect.size.height * scaleY);

        UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom];
        button.frame = scaledRect;

        // Debug color or visual feedback
        button.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.05];
        button.layer.cornerRadius = MIN(rect.size.width, rect.size.height) * 0.5 * scaleX;

        [button addTarget:self action:@selector(buttonDown:) forControlEvents:UIControlEventTouchDown];
        [button addTarget:self action:@selector(buttonUp:) forControlEvents:UIControlEventTouchUpInside | UIControlEventTouchUpOutside | UIControlEventTouchCancel];

        button.tag = [keyStr integerValue];
        [self addSubview:button];
        self.buttons[keyStr] = button;
    }];
}

- (void)buttonDown:(UIButton *)sender {
    sender.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.2];
    [self.delegate controllerViewDidPressKey:(EmulatorKey)sender.tag];
}

- (void)buttonUp:(UIButton *)sender {
    sender.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.05];
    [self.delegate controllerViewDidReleaseKey:(EmulatorKey)sender.tag];
}

@end
