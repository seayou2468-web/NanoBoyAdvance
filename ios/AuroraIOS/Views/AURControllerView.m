#import "AURControllerView.h"

@interface AURControllerView ()
@property (nonatomic, strong) UIView *skinContainer;
@property (nonatomic, strong) NSMutableDictionary<NSString *, UIButton *> *buttons;
@end

@implementation AURControllerView

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.skinContainer = [[UIView alloc] initWithFrame:self.bounds];
        [self addSubview:self.skinContainer];
        self.buttons = [NSMutableDictionary dictionary];
        self.backgroundColor = [UIColor colorWithWhite:0.05 alpha:1.0];
    }
    return self;
}

- (void)layoutSubviews {
    [super layoutSubviews];
    self.skinContainer.frame = self.bounds;
}

- (void)applySkin:(AURControllerSkin *)skin {
    for (UIView *v in self.skinContainer.subviews) [v removeFromSuperview];
    [self.buttons removeAllObjects];

    CGFloat refWidth = 375.0;
    CGFloat refHeight = 500.0;
    CGFloat scaleX = self.bounds.size.width / refWidth;
    CGFloat scaleY = self.bounds.size.height / refHeight;
    CGFloat scale = MIN(scaleX, scaleY);

    [skin.buttonRects enumerateKeysAndObjectsUsingBlock:^(NSString *keyStr, NSValue *rectValue, BOOL *stop) {
        CGRect refRect = [rectValue CGRectValue];
        CGRect rect = CGRectMake(refRect.origin.x * scaleX, refRect.origin.y * scaleY, refRect.size.width * scaleX, refRect.size.height * scaleY);

        UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom];
        button.frame = rect;
        button.tag = [keyStr integerValue];

        [self styleButton:button forEmulatorKey:(EmulatorKey)button.tag scale:scale skinName:skin.name];

        [button addTarget:self action:@selector(buttonDown:) forControlEvents:UIControlEventTouchDown];
        [button addTarget:self action:@selector(buttonUp:) forControlEvents:UIControlEventTouchUpInside | UIControlEventTouchUpOutside | UIControlEventTouchCancel];

        [self.skinContainer addSubview:button];
        self.buttons[keyStr] = button;
    }];
}

- (void)styleButton:(UIButton *)button forEmulatorKey:(EmulatorKey)key scale:(CGFloat)scale skinName:(NSString *)skinName {
    BOOL isGBA = [skinName containsString:@"GBA"];

    UIColor *baseColor = [UIColor colorWithWhite:0.15 alpha:1.0];
    UIColor *textColor = [UIColor whiteColor];
    CGFloat cornerRadius = 4.0 * scale;
    NSString *title = @"";

    switch (key) {
        case EMULATOR_KEY_A:
            title = @"A";
            baseColor = isGBA ? [UIColor colorWithRed:0.2 green:0.2 blue:0.25 alpha:1.0] : [UIColor colorWithRed:0.7 green:0.1 blue:0.1 alpha:1.0];
            cornerRadius = button.bounds.size.width / 2.0;
            break;
        case EMULATOR_KEY_B:
            title = @"B";
            baseColor = isGBA ? [UIColor colorWithRed:0.2 green:0.2 blue:0.25 alpha:1.0] : [UIColor colorWithRed:0.7 green:0.1 blue:0.1 alpha:1.0];
            cornerRadius = button.bounds.size.width / 2.0;
            break;
        case EMULATOR_KEY_UP: title = @"▲"; cornerRadius = 5.0 * scale; break;
        case EMULATOR_KEY_DOWN: title = @"▼"; cornerRadius = 5.0 * scale; break;
        case EMULATOR_KEY_LEFT: title = @"◀"; cornerRadius = 5.0 * scale; break;
        case EMULATOR_KEY_RIGHT: title = @"▶"; cornerRadius = 5.0 * scale; break;
        case EMULATOR_KEY_START: title = @"START"; cornerRadius = 10.0 * scale; break;
        case EMULATOR_KEY_SELECT: title = @"SELECT"; cornerRadius = 10.0 * scale; break;
        case EMULATOR_KEY_L: title = @"L"; cornerRadius = 15.0 * scale; break;
        case EMULATOR_KEY_R: title = @"R"; cornerRadius = 15.0 * scale; break;
        default: break;
    }

    if (isGBA) {
        self.backgroundColor = [UIColor colorWithRed:0.15 green:0.12 blue:0.2 alpha:1.0];
        if (key == EMULATOR_KEY_A || key == EMULATOR_KEY_B) {
            baseColor = [UIColor colorWithWhite:1.0 alpha:0.1];
            button.layer.borderColor = [UIColor colorWithWhite:1.0 alpha:0.2].CGColor;
            button.layer.borderWidth = 1.0;
        } else if (key >= EMULATOR_KEY_UP && key <= EMULATOR_KEY_RIGHT) {
            baseColor = [UIColor colorWithWhite:0.0 alpha:0.3];
        }
    } else {
        self.backgroundColor = [UIColor colorWithWhite:0.8 alpha:1.0];
    }

    [button setTitle:title forState:UIControlStateNormal];
    [button setTitleColor:textColor forState:UIControlStateNormal];
    button.titleLabel.font = [UIFont systemFontOfSize:12.0 * scale weight:UIFontWeightBold];
    button.backgroundColor = baseColor;
    button.layer.cornerRadius = cornerRadius;
    button.clipsToBounds = YES;
}

- (void)buttonDown:(UIButton *)sender {
    [UIView animateWithDuration:0.05 animations:^{
        sender.transform = CGAffineTransformMakeScale(0.92, 0.92);
        sender.alpha = 0.7;
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
