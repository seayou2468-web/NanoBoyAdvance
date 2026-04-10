#import "AURGameCollectionViewCell.h"

@implementation AURGameCollectionViewCell

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.imageView = [[UIImageView alloc] init];
        self.imageView.contentMode = UIViewContentModeScaleAspectFill;
        self.imageView.clipsToBounds = YES;
        self.imageView.layer.cornerRadius = 10;
        self.imageView.backgroundColor = [UIColor colorWithWhite:0.15 alpha:1.0];
        [self.contentView addSubview:self.imageView];

        self.titleLabel = [[UILabel alloc] init];
        self.titleLabel.font = [UIFont systemFontOfSize:12 weight:UIFontWeightMedium];
        self.titleLabel.textColor = [UIColor whiteColor];
        self.titleLabel.textAlignment = NSTextAlignmentCenter;
        self.titleLabel.numberOfLines = 2;
        [self.contentView addSubview:self.titleLabel];

        self.imageView.translatesAutoresizingMaskIntoConstraints = NO;
        self.titleLabel.translatesAutoresizingMaskIntoConstraints = NO;

        [NSLayoutConstraint activateConstraints:@[
            [self.imageView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor],
            [self.imageView.leadingAnchor constraintEqualToAnchor:self.contentView.leadingAnchor],
            [self.imageView.trailingAnchor constraintEqualToAnchor:self.contentView.trailingAnchor],
            [self.imageView.heightAnchor constraintEqualToAnchor:self.imageView.widthAnchor multiplier:1.2],

            [self.titleLabel.topAnchor constraintEqualToAnchor:self.imageView.bottomAnchor constant:8],
            [self.titleLabel.leadingAnchor constraintEqualToAnchor:self.contentView.leadingAnchor],
            [self.titleLabel.trailingAnchor constraintEqualToAnchor:self.contentView.trailingAnchor],
            [self.titleLabel.bottomAnchor constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor]
        ]];
    }
    return self;
}

@end
