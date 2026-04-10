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
        UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom];
        button.frame = rect;

        [button addTarget:self action:@selector(buttonDown:) forControlEvents:UIControlEventTouchDown];
        [button addTarget:self action:@selector(buttonUp:) forControlEvents:UIControlEventTouchUpInside | UIControlEventTouchUpOutside | UIControlEventTouchCancel];

        button.tag = [keyStr integerValue];
        [self addSubview:button];
        self.buttons[keyStr] = button;
    }];
}

- (void)buttonDown:(UIButton *)sender {
    [self.delegate controllerViewDidPressKey:(EmulatorKey)sender.tag];
}

- (void)buttonUp:(UIButton *)sender {
    [self.delegate controllerViewDidReleaseKey:(EmulatorKey)sender.tag];
}

@end
