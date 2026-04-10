#import "AURControllerView.h"

@interface AURControllerView ()
@property (nonatomic, strong) UIImageView *backgroundImageView;
@property (nonatomic, strong) UIView *skinContainer;
@property (nonatomic, strong) NSMutableDictionary<NSString *, UIButton *> *buttons;
@end

@implementation AURControllerView

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.backgroundImageView = [[UIImageView alloc] initWithFrame:self.bounds];
        self.backgroundImageView.contentMode = UIViewContentModeScaleAspectFit;
        [self addSubview:self.backgroundImageView];

        self.skinContainer = [[UIView alloc] initWithFrame:self.bounds];
        [self addSubview:self.skinContainer];
        self.buttons = [NSMutableDictionary dictionary];
        self.backgroundColor = [UIColor colorWithWhite:0.05 alpha:1.0];
    }
    return self;
}

- (void)layoutSubviews {
    [super layoutSubviews];
    self.backgroundImageView.frame = self.bounds;
    self.skinContainer.frame = self.bounds;
}

- (void)applySkin:(AURControllerSkin *)skin {
    self.backgroundImageView.image = skin.backgroundImage;
    for (UIView *v in self.skinContainer.subviews) [v removeFromSuperview];
    [self.buttons removeAllObjects];

    // Scale factor for Delta skins which usually assume a fixed resolution (e.g. 375pt width)
    CGFloat refWidth = 375.0;
    CGFloat refHeight = skin.backgroundImage.size.height > 0 ? (skin.backgroundImage.size.height * refWidth / skin.backgroundImage.size.width) : 500.0;

    CGFloat scaleX = self.bounds.size.width / refWidth;
    CGFloat scaleY = self.bounds.size.height / refHeight;
    CGFloat scale = MIN(scaleX, scaleY);

    [skin.buttonRects enumerateKeysAndObjectsUsingBlock:^(NSString *keyName, NSValue *rectValue, BOOL *stop) {
        CGRect refRect = [rectValue CGRectValue];
        CGRect rect = CGRectMake(refRect.origin.x * scaleX, refRect.origin.y * scaleY, refRect.size.width * scaleX, refRect.size.height * scaleY);

        UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom];
        button.frame = rect;

        EmulatorKey key = EMULATOR_KEY_A;
        if ([keyName isEqualToString:@"up"]) key = EMULATOR_KEY_UP;
        else if ([keyName isEqualToString:@"down"]) key = EMULATOR_KEY_DOWN;
        else if ([keyName isEqualToString:@"left"]) key = EMULATOR_KEY_LEFT;
        else if ([keyName isEqualToString:@"right"]) key = EMULATOR_KEY_RIGHT;
        else if ([keyName isEqualToString:@"a"]) key = EMULATOR_KEY_A;
        else if ([keyName isEqualToString:@"b"]) key = EMULATOR_KEY_B;
        else if ([keyName isEqualToString:@"l"]) key = EMULATOR_KEY_L;
        else if ([keyName isEqualToString:@"r"]) key = EMULATOR_KEY_R;
        else if ([keyName isEqualToString:@"start"]) key = EMULATOR_KEY_START;
        else if ([keyName isEqualToString:@"select"]) key = EMULATOR_KEY_SELECT;

        button.tag = (NSInteger)key;

        // If skin has no background image, use programmatic styling
        if (!skin.backgroundImage) {
            [self styleButton:button forEmulatorKey:(EmulatorKey)button.tag scale:scale skinName:skin.name];
        } else {
            // Visual feedback for image-based skins
            button.backgroundColor = [UIColor clearColor];
        }

        [button addTarget:self action:@selector(buttonDown:) forControlEvents:UIControlEventTouchDown];
        [button addTarget:self action:@selector(buttonUp:) forControlEvents:UIControlEventTouchUpInside | UIControlEventTouchUpOutside | UIControlEventTouchCancel];

        [self.skinContainer addSubview:button];
        self.buttons[keyName] = button;
    }];
}

- (void)styleButton:(UIButton *)button forEmulatorKey:(EmulatorKey)key scale:(CGFloat)scale skinName:(NSString *)skinName {
    BOOL isGBA = [skinName containsString:@"GBA"];
    BOOL isNES = [skinName containsString:@"NES"];

    UIColor *baseColor = [UIColor colorWithWhite:0.15 alpha:1.0];
    UIColor *textColor = [UIColor whiteColor];
    CGFloat cornerRadius = 4.0 * scale;
    NSString *title = @"";

    button.layer.shadowColor = [UIColor blackColor].CGColor;
    button.layer.shadowOffset = CGSizeMake(0, 2 * scale);
    button.layer.shadowOpacity = 0.5;
    button.layer.shadowRadius = 2 * scale;

    switch (key) {
        case EMULATOR_KEY_A:
            title = @"A";
            baseColor = isGBA ? [UIColor colorWithRed:0.25 green:0.22 blue:0.4 alpha:1.0] : [UIColor colorWithRed:0.75 green:0.1 blue:0.15 alpha:1.0];
            cornerRadius = button.bounds.size.width / 2.0;
            break;
        case EMULATOR_KEY_B:
            title = @"B";
            baseColor = isGBA ? [UIColor colorWithRed:0.25 green:0.22 blue:0.4 alpha:1.0] : [UIColor colorWithRed:0.75 green:0.1 blue:0.15 alpha:1.0];
            cornerRadius = button.bounds.size.width / 2.0;
            break;
        case EMULATOR_KEY_START: title = @"START"; cornerRadius = 8.0 * scale; break;
        case EMULATOR_KEY_SELECT: title = @"SELECT"; cornerRadius = 8.0 * scale; break;
        case EMULATOR_KEY_L: title = @"L"; cornerRadius = 15.0 * scale; break;
        case EMULATOR_KEY_R: title = @"R"; cornerRadius = 15.0 * scale; break;
        default: break;
    }

    if (isGBA) {
        self.backgroundColor = [UIColor colorWithRed:0.2 green:0.18 blue:0.25 alpha:1.0];
    } else if (isNES) {
        self.backgroundColor = [UIColor colorWithWhite:0.85 alpha:1.0];
    }

    [button setTitle:title forState:UIControlStateNormal];
    [button setTitleColor:textColor forState:UIControlStateNormal];
    button.titleLabel.font = [UIFont systemFontOfSize:12.0 * scale weight:UIFontWeightBold];
    button.backgroundColor = baseColor;
    button.layer.cornerRadius = cornerRadius;
}

- (void)buttonDown:(UIButton *)sender {
    [UIView animateWithDuration:0.05 animations:^{
        sender.transform = CGAffineTransformMakeScale(0.95, 0.95);
        sender.alpha = 0.8;
    }];
    [self.delegate controllerViewDidPressKey:(EmulatorKey)sender.tag];
}

- (void)buttonUp:(UIButton *)sender {
    [UIView animateWithDuration:0.1 animations:^{
        sender.transform = CGAffineTransformIdentity;
        sender.alpha = 1.0;
    }];
    [self.delegate controllerViewDidReleaseKey:(EmulatorKey)sender.tag];
}

@end
